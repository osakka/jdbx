#include "core/server.h"
#include "api/api.h"
#include "utils/daemonize.h"
#include "utils/logger.h"
#include "core/thread_pool.h"

/* Define function type for thread pool tasks */
typedef void (*thread_task_func_t)(void*);
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/syscall.h>

/* Server state tracking */
typedef enum {
    SERVER_STATE_UNINITIALIZED,
    SERVER_STATE_INITIALIZED,
    SERVER_STATE_SOCKET_CREATED,
    SERVER_STATE_SOCKET_BOUND,
    SERVER_STATE_LISTENING,
    SERVER_STATE_THREAD_POOL_CREATED,
    SERVER_STATE_RUNNING,
    SERVER_STATE_SHUTDOWN_REQUESTED,
    SERVER_STATE_SHUTDOWN_COMPLETE,
    SERVER_STATE_ERROR
} server_state_t;

/* Global server state */
static server_state_t g_server_state = SERVER_STATE_UNINITIALIZED;
static int g_shutdown_requested = 0;

/* Server initialization steps prototypes */
static int initialize_socket(server_config_t *config);
static int setup_listening_socket(server_config_t *config);
static int verify_socket_binding(server_config_t *config);
static int create_thread_pool(server_config_t *config);
static void set_server_state(server_state_t new_state, const char *message);
static void* accept_loop(void *arg);
static void simple_handle_client(int client_fd, struct api_context *api_ctx);

/* External handle_client definition from server.h */
extern void* handle_client(void* client_data);

/**
 * Improved server initialization and running sequence
 * 
 * This function implements a completely rewritten server initialization sequence:
 * 1. Setup signal handlers
 * 2. Create socket
 * 3. Bind socket
 * 4. Set socket to listen
 * 5. Verify socket binding
 * 6. Create thread pool
 * 7. Run accept loop
 * 
 * @param config Server configuration
 * @param api_ctx API context for request handling
 * @return Server status code
 */
server_status_t server_improved_init_and_run(server_config_t *config, struct api_context *api_ctx) {
    if (!config || !api_ctx) {
        fprintf(stderr, "Error: Invalid server config or API context\n");
        if (g_logger) {
            LOG_ERROR("Invalid server config (%p) or API context (%p)", (void*)config, (void*)api_ctx);
        }
        return SERVER_ERROR;
    }

    /* Store API context in config */
    config->api_ctx = api_ctx;

    /* STEP 1: Set initial state */
    set_server_state(SERVER_STATE_UNINITIALIZED, "Starting server initialization");

    /* STEP 2: Setup signal handling */
    signal(SIGINT, SIG_IGN);  /* Ignore SIGINT during initialization */
    signal(SIGTERM, SIG_IGN); /* Ignore SIGTERM during initialization */
    
    /* STEP 3: Create socket */
    if (initialize_socket(config) != 0) {
        set_server_state(SERVER_STATE_ERROR, "Failed to initialize socket");
        return SERVER_SOCKET_ERROR;
    }
    
    /* STEP 4: Setup listening */
    if (setup_listening_socket(config) != 0) {
        set_server_state(SERVER_STATE_ERROR, "Failed to set socket to listen state");
        return SERVER_LISTEN_ERROR;
    }
    
    /* STEP 5: Verify binding */
    if (verify_socket_binding(config) != 0) {
        set_server_state(SERVER_STATE_ERROR, "Failed to verify socket binding");
        return SERVER_BIND_ERROR;
    }
    
    /* STEP 6: Create thread pool */
    if (create_thread_pool(config) != 0) {
        set_server_state(SERVER_STATE_ERROR, "Failed to create thread pool");
        return SERVER_THREAD_ERROR;
    }
    
    /* STEP 7: Setup signal handlers for running state */
    signal(SIGINT, SIG_DFL);  /* Restore default SIGINT handler */
    signal(SIGTERM, SIG_DFL); /* Restore default SIGTERM handler */
    
    /* STEP 8: Enter accept loop */
    set_server_state(SERVER_STATE_RUNNING, "Server is running and accepting connections");
    
    /* Run accept loop in the current thread */
    accept_loop(config);
    
    /* Cleanup after accept loop finishes */
    if (config->socket_fd > 0) {
        close(config->socket_fd);
        config->socket_fd = 0;
    }
    
    set_server_state(SERVER_STATE_SHUTDOWN_COMPLETE, "Server shutdown complete");
    return SERVER_OK;
}

/**
 * Socket initialization step 
 */
static int initialize_socket(server_config_t *config) {
    if (!config) {
        return -1;
    }
    
    printf("STEP 3: Creating socket (PID: %d)\n", getpid());
    if (g_logger) {
        LOG_INFO("STEP 3: Creating socket (PID: %d)", getpid());
    }
    
    /* Create socket in blocking mode */
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        fprintf(stderr, "Error: Failed to create socket: %s (errno=%d)\n", 
                strerror(errno), errno);
        if (g_logger) {
            LOG_ERROR("Failed to create socket: %s (errno=%d)", 
                     strerror(errno), errno);
        }
        return -1;
    }
    
    printf("Socket created successfully (fd=%d)\n", socket_fd);
    if (g_logger) {
        LOG_INFO("Socket created successfully (fd=%d)", socket_fd);
    }
    
    /* Set socket options */
    int reuse = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        fprintf(stderr, "Warning: Failed to set SO_REUSEADDR: %s\n", strerror(errno));
        if (g_logger) {
            LOG_WARNING("Failed to set SO_REUSEADDR: %s", strerror(errno));
        }
    }
    
    /* Prepare address structure */
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(config->port);
    
    /* Resolve hostname */
    if (config->host && strcmp(config->host, "0.0.0.0") != 0) {
        if (strcmp(config->host, "127.0.0.1") == 0 || strcmp(config->host, "localhost") == 0) {
            address.sin_addr.s_addr = inet_addr("127.0.0.1");
            printf("Using localhost (127.0.0.1) for binding\n");
            if (g_logger) {
                LOG_INFO("Using localhost (127.0.0.1) for binding");
            }
        } else {
            /* Try to resolve hostname */
            struct addrinfo hints, *result;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            
            int status = getaddrinfo(config->host, NULL, &hints, &result);
            if (status == 0 && result != NULL) {
                /* Use the first result */
                struct sockaddr_in* resolved_addr = (struct sockaddr_in*)result->ai_addr;
                address.sin_addr = resolved_addr->sin_addr;
                
                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(address.sin_addr), ip_str, INET_ADDRSTRLEN);
                printf("Resolved %s to %s\n", config->host, ip_str);
                if (g_logger) {
                    LOG_INFO("Resolved %s to %s", config->host, ip_str);
                }
                
                freeaddrinfo(result);
            } else {
                /* Fallback to INADDR_ANY */
                address.sin_addr.s_addr = INADDR_ANY;
                printf("Failed to resolve %s, falling back to INADDR_ANY\n", config->host);
                if (g_logger) {
                    LOG_WARNING("Failed to resolve %s, falling back to INADDR_ANY", 
                              config->host);
                }
            }
        }
    } else {
        address.sin_addr.s_addr = INADDR_ANY;
        printf("Using INADDR_ANY (0.0.0.0) for binding\n");
        if (g_logger) {
            LOG_INFO("Using INADDR_ANY (0.0.0.0) for binding");
        }
    }
    
    /* Bind socket */
    printf("Binding socket to port %d\n", config->port);
    if (g_logger) {
        LOG_INFO("Binding socket to port %d", config->port);
    }
    
    if (bind(socket_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        fprintf(stderr, "Error: Failed to bind socket: %s (errno=%d)\n", 
                strerror(errno), errno);
        if (g_logger) {
            LOG_ERROR("Failed to bind socket: %s (errno=%d)", 
                     strerror(errno), errno);
        }
        close(socket_fd);
        return -1;
    }
    
    printf("Socket bound successfully to port %d\n", config->port);
    if (g_logger) {
        LOG_INFO("Socket bound successfully to port %d", config->port);
    }
    
    /* Store socket descriptor in config */
    config->socket_fd = socket_fd;
    
    /* Update server state */
    set_server_state(SERVER_STATE_SOCKET_BOUND, "Socket bound successfully");
    
    return 0;
}

/**
 * Setup listening socket
 */
static int setup_listening_socket(server_config_t *config) {
    if (!config || config->socket_fd <= 0) {
        return -1;
    }
    
    printf("STEP 4: Setting socket to listen state\n");
    if (g_logger) {
        LOG_INFO("STEP 4: Setting socket to listen state");
    }
    
    /* Set up listening */
    if (listen(config->socket_fd, 10) < 0) {
        fprintf(stderr, "Error: Failed to listen on socket: %s (errno=%d)\n", 
                strerror(errno), errno);
        if (g_logger) {
            LOG_ERROR("Failed to listen on socket: %s (errno=%d)", 
                     strerror(errno), errno);
        }
        close(config->socket_fd);
        config->socket_fd = 0;
        return -1;
    }
    
    printf("Socket is now listening\n");
    if (g_logger) {
        LOG_INFO("Socket is now listening");
    }
    
    /* Update server state */
    set_server_state(SERVER_STATE_LISTENING, "Socket is listening");
    
    return 0;
}

/**
 * Verify socket binding
 */
static int verify_socket_binding(server_config_t *config) {
    if (!config || config->socket_fd <= 0) {
        return -1;
    }
    
    printf("STEP 5: Verifying socket binding\n");
    if (g_logger) {
        LOG_INFO("STEP 5: Verifying socket binding");
    }
    
    /* Verify socket is in listening state */
    int acceptconn = 0;
    socklen_t optlen = sizeof(acceptconn);
    if (getsockopt(config->socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &acceptconn, &optlen) < 0) {
        fprintf(stderr, "Warning: Failed to check SO_ACCEPTCONN: %s\n", strerror(errno));
        if (g_logger) {
            LOG_WARNING("Failed to check SO_ACCEPTCONN: %s", strerror(errno));
        }
    } else if (!acceptconn) {
        fprintf(stderr, "Error: Socket is not in listening state\n");
        if (g_logger) {
            LOG_ERROR("Socket is not in listening state");
        }
        return -1;
    }
    
    /* Check with netstat */
    printf("Verifying with netstat:\n");
    if (g_logger) {
        LOG_INFO("Verifying with netstat");
    }
    
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "netstat -tuln | grep ':%d' || echo 'Port %d NOT FOUND in netstat'", 
             config->port, config->port);
    system(cmd);
    
    /* Add a short delay to give the OS time to update network tables */
    printf("Waiting for OS to update network tables...\n");
    if (g_logger) {
        LOG_INFO("Waiting for OS to update network tables");
    }
    sleep(1);
    
    return 0;
}

/**
 * Create thread pool
 */
static int create_thread_pool(server_config_t *config) {
    if (!config) {
        return -1;
    }
    
    printf("STEP 6: Creating thread pool\n");
    if (g_logger) {
        LOG_INFO("STEP 6: Creating thread pool");
    }
    
    /* Calculate thread pool size based on configuration */
    int min_threads = 4;
    int max_threads = config->max_connections > 0 ? config->max_connections : 16;
    
    thread_pool_config_t pool_config = {
        .min_threads = min_threads,
        .max_threads = max_threads,
        .queue_size = max_threads * 4,
        .idle_timeout = 60
    };
    
    /* Create thread pool */
    config->thread_pool = thread_pool_create_config(&pool_config);
    if (!config->thread_pool) {
        fprintf(stderr, "Error: Failed to create thread pool\n");
        if (g_logger) {
            LOG_ERROR("Failed to create thread pool");
        }
        return -1;
    }
    
    printf("Thread pool created successfully with %d-%d threads\n", 
           pool_config.min_threads, pool_config.max_threads);
    if (g_logger) {
        LOG_INFO("Thread pool created successfully with %d-%d threads",
               pool_config.min_threads, pool_config.max_threads);
    }
    
    /* Update server state */
    set_server_state(SERVER_STATE_THREAD_POOL_CREATED, "Thread pool created");
    
    return 0;
}

/**
 * Accept loop
 */
static void* accept_loop(void *arg) {
    server_config_t *config = (server_config_t*)arg;
    
    if (!config || config->socket_fd <= 0 || !config->api_ctx) {
        fprintf(stderr, "Error: Invalid configuration for accept loop\n");
        if (g_logger) {
            LOG_ERROR("Invalid configuration for accept loop");
        }
        return NULL;
    }
    
    printf("STEP 8: Starting accept loop (PID: %d)\n", getpid());
    if (g_logger) {
        LOG_INFO("STEP 8: Starting accept loop (PID: %d)", getpid());
    }
    
    /* Set socket to non-blocking mode */
    int flags = fcntl(config->socket_fd, F_GETFL, 0);
    if (flags < 0 || fcntl(config->socket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        fprintf(stderr, "Warning: Failed to set socket to non-blocking mode: %s\n", 
                strerror(errno));
        if (g_logger) {
            LOG_WARNING("Failed to set socket to non-blocking mode: %s", 
                      strerror(errno));
        }
    }
    
    /* Accept loop variables */
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd;
    int connection_count = 0;
    
    /* File descriptor set for select */
    fd_set read_fds;
    int max_fd = config->socket_fd;
    
    /* Accept loop */
    while (!g_shutdown_requested) {
        /* Clear and set file descriptor set */
        FD_ZERO(&read_fds);
        FD_SET(config->socket_fd, &read_fds);
        
        /* Wait for activity with timeout */
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
        
        /* Check for errors */
        if (activity < 0) {
            if (errno == EINTR) {
                /* Interrupted by signal, check if shutdown requested */
                if (g_shutdown_requested) {
                    break;
                }
                continue;
            }
            
            fprintf(stderr, "Error: select() failed: %s\n", strerror(errno));
            if (g_logger) {
                LOG_ERROR("select() failed: %s", strerror(errno));
            }
            break;
        }
        
        /* Check for timeout */
        if (activity == 0) {
            /* No activity, check if shutdown requested */
            if (g_shutdown_requested) {
                break;
            }
            continue;
        }
        
        /* Check for socket activity */
        if (FD_ISSET(config->socket_fd, &read_fds)) {
            /* Accept connection */
            client_fd = accept(config->socket_fd, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_fd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    /* Non-blocking socket with no connections ready */
                    continue;
                }
                
                fprintf(stderr, "Error: accept() failed: %s\n", strerror(errno));
                if (g_logger) {
                    LOG_ERROR("accept() failed: %s", strerror(errno));
                }
                continue;
            }
            
            /* Connection accepted */
            connection_count++;
            printf("Accepted connection #%d from %s:%d\n", 
                   connection_count, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            if (g_logger) {
                LOG_INFO("Accepted connection #%d from %s:%d", 
                       connection_count, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            }
            
            /* Create client connection object */
            client_conn_t *client_conn = malloc(sizeof(client_conn_t));
            if (!client_conn) {
                fprintf(stderr, "Error: Failed to allocate memory for client connection\n");
                if (g_logger) {
                    LOG_ERROR("Failed to allocate memory for client connection");
                }
                close(client_fd);
                continue;
            }
            
            /* Initialize client connection */
            client_conn->client_fd = client_fd;
            client_conn->address = client_addr;
            client_conn->api_ctx = config->api_ctx;
            
            /* Submit to thread pool */
            if (config->thread_pool) {
                /* Use function adapter to avoid cast warning */
                void handle_client_wrapper(void* arg) {
                    handle_client(arg);
                }
                thread_pool_add_work(config->thread_pool, handle_client_wrapper, client_conn);
            } else {
                /* Fallback to direct handling */
                simple_handle_client(client_fd, config->api_ctx);
                close(client_fd);
                free(client_conn);
            }
        }
    }
    
    printf("Accept loop terminated\n");
    if (g_logger) {
        LOG_INFO("Accept loop terminated");
    }
    
    return NULL;
}

/**
 * Update server state with message
 */
static void set_server_state(server_state_t new_state, const char *message) {
    g_server_state = new_state;
    
    if (message) {
        printf("SERVER STATE: %s\n", message);
        if (g_logger) {
            LOG_INFO("SERVER STATE: %s", message);
        }
    }
}

/**
 * Simple client handler for fallback
 */
static void simple_handle_client(int client_fd, struct api_context *api_ctx) {
    /* Mark unused parameter */
    (void)api_ctx;
    
    /* Simple response */
    const char *response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 19\r\n\r\nServer is running!\r\n";
    write(client_fd, response, strlen(response));
}

/**
 * Request server shutdown
 */
void server_request_shutdown_improved(void) {
    g_shutdown_requested = 1;
    set_server_state(SERVER_STATE_SHUTDOWN_REQUESTED, "Shutdown requested");
}