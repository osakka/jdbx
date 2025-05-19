/**
 * Updated Socket binding fix for JSONdb Server
 * 
 * This file contains a refactored implementation of the server
 * socket binding and thread management to ensure that all socket
 * operations happen in the correct order.
 */

#include "core/server.h"
#include "api/api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>

/* Forward declarations */
static int create_and_bind_socket(server_config_t* config);
static int set_socket_to_listen(server_config_t* config);
static int verify_socket_state(server_config_t* config);
static pthread_t create_accept_thread(server_config_t* config);
static int wait_for_thread_initialization(pthread_t thread_id, int timeout_ms);

/* Global socket state indicator */
static enum {
    SOCKET_STATE_UNINITIALIZED,
    SOCKET_STATE_CREATED,
    SOCKET_STATE_BOUND,
    SOCKET_STATE_LISTENING,
    SOCKET_STATE_READY,
    SOCKET_STATE_ERROR
} g_socket_state = SOCKET_STATE_UNINITIALIZED;

/* Global accept thread indicator */
static enum {
    THREAD_STATE_UNINITIALIZED,
    THREAD_STATE_CREATING,
    THREAD_STATE_RUNNING,
    THREAD_STATE_ERROR
} g_thread_state = THREAD_STATE_UNINITIALIZED;

/* Server is running flag - accessible by accept thread */
static int server_running = 0;

/* Mutex for thread synchronization */
static pthread_mutex_t thread_init_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t thread_init_cond = PTHREAD_COND_INITIALIZER;
static int thread_initialized = 0;

/**
 * Initialize server with socket creation and API context
 */
server_status_t server_init(server_config_t* config, api_context_t* api_ctx) {
    if (!config) {
        fprintf(stderr, "Error: NULL server configuration passed to server_init\n");
        return SERVER_ERROR;
    }

    printf("Initializing server with config: port=%d, host=%s\n", 
           config->port, config->host ? config->host : "0.0.0.0");

    /* Store the API context in the server config for sharing with client threads */
    config->api_ctx = api_ctx;
    
    if (g_logger) {
        LOG_INFO("API context set in server config: %p", (void*)api_ctx);
    }

    /* Set default values if not specified */
    if (config->max_connections <= 0) {
        config->max_connections = 10; /* Set to default */
    }

    /* Create socket with detailed logging */
    printf("Creating server socket...\n");
    
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("Socket creation failed");
        fprintf(stderr, "Socket creation failed: %s (errno=%d)\n", strerror(errno), errno);
        return SERVER_SOCKET_ERROR;
    }
    
    printf("Socket created successfully with fd=%d\n", socket_fd);
    
    /* Set socket options */
    int opt = 1;
    
    printf("Setting socket options on fd=%d\n", socket_fd);
    
    /* Set SO_REUSEADDR - critical for quick restarts */
    printf("Setting SO_REUSEADDR socket option on fd=%d\n", socket_fd);
    
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR failed");
        fprintf(stderr, "Failed to set SO_REUSEADDR: %s (errno=%d)\n", strerror(errno), errno);
        close(socket_fd);
        return SERVER_SOCKET_ERROR;
    }
    
    /* Set SO_REUSEPORT if available */
    #ifdef SO_REUSEPORT
    printf("Setting SO_REUSEPORT socket option on fd=%d\n", socket_fd);
    
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        fprintf(stderr, "Warning: Failed to set SO_REUSEPORT: %s (errno=%d) - continuing anyway\n", 
               strerror(errno), errno);
        /* Non-fatal - continue execution */
    } else {
        printf("SO_REUSEPORT set successfully\n");
    }
    #endif

    /* Save the socket descriptor to the config */
    config->socket_fd = socket_fd;
    
    printf("Server socket initialized successfully (fd=%d)\n", config->socket_fd);
    g_socket_state = SOCKET_STATE_CREATED;
    
    return SERVER_OK;
}

/**
 * Start server - handles binding, listening, and thread creation
 * This function has been separated into distinct phases to ensure proper ordering
 */
server_status_t server_start(server_config_t* config) {
    if (!config) {
        fprintf(stderr, "Error: Null server configuration passed to server_start\n");
        return SERVER_ERROR;
    }

    /* Create a new socket if the existing one is invalid */
    if (config->socket_fd <= 0) {
        printf("Socket file descriptor is invalid (%d), recreating...\n", config->socket_fd);
        
        /* Create new socket */
        int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd < 0) {
            fprintf(stderr, "Socket recreation failed: %s (errno=%d)\n", strerror(errno), errno);
            return SERVER_SOCKET_ERROR;
        }
        
        /* Set socket options */
        int opt = 1;
        
        if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            fprintf(stderr, "Failed to set SO_REUSEADDR: %s (errno=%d)\n", strerror(errno), errno);
            close(socket_fd);
            return SERVER_SOCKET_ERROR;
        }
        
        #ifdef SO_REUSEPORT
        setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
        #endif
        
        /* Save the socket descriptor */
        config->socket_fd = socket_fd;
        
        printf("Socket recreated successfully with fd=%d\n", config->socket_fd);
        g_socket_state = SOCKET_STATE_CREATED;
    }

    /* PHASE 1: Create and bind socket */
    int bind_result = create_and_bind_socket(config);
    if (bind_result != SERVER_OK) {
        fprintf(stderr, "Error: Socket binding failed\n");
        return bind_result;
    }
    
    /* PHASE 2: Set socket to listen state */
    int listen_result = set_socket_to_listen(config);
    if (listen_result != SERVER_OK) {
        fprintf(stderr, "Error: Socket listen failed\n");
        return listen_result;
    }
    
    /* PHASE 3: Verify socket is in proper state */
    int verify_result = verify_socket_state(config);
    if (verify_result != SERVER_OK) {
        fprintf(stderr, "Error: Socket state verification failed\n");
        return verify_result;
    }
    
    /* Mark server as running before thread creation */
    server_running = 1;
    
    /* PHASE 4: Create accept thread with proper socket descriptor */
    pthread_t accept_thread = create_accept_thread(config);
    if (accept_thread == 0) {
        fprintf(stderr, "Error: Failed to create accept thread\n");
        server_running = 0;
        return SERVER_THREAD_ERROR;
    }
    
    /* PHASE 5: Wait for thread to initialize properly */
    int wait_result = wait_for_thread_initialization(accept_thread, 5000); /* 5 second timeout */
    if (wait_result != 0) {
        fprintf(stderr, "Error: Thread initialization timeout or error\n");
        server_running = 0;
        return SERVER_THREAD_ERROR;
    }

    printf("Server started successfully and is accepting connections on port %d\n", config->port);
    
    return SERVER_OK;
}

/**
 * Create and bind socket to the specified address and port
 */
static int create_and_bind_socket(server_config_t* config) {
    if (!config) return SERVER_ERROR;
    
    /* Verify essential configuration */
    printf("Binding socket %d to port %d\n", config->socket_fd, config->port);
    
    if (config->socket_fd <= 0) {
        fprintf(stderr, "Error: Invalid socket file descriptor (%d)\n", config->socket_fd);
        return SERVER_ERROR;
    }

    if (config->port <= 0 || config->port > 65535) {
        fprintf(stderr, "Error: Invalid port number (%d)\n", config->port);
        return SERVER_ERROR;
    }

    /* Create address structure */
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;

    /* Always bind to all interfaces for listening */
    address.sin_addr.s_addr = INADDR_ANY;
    
    /* Use the configured port */
    address.sin_port = htons(config->port);
    
    /* Log binding information */
    printf("Binding to all interfaces (0.0.0.0)\n");
    if (config->host != NULL) {
        printf("Server will be advertised as: %s\n", config->host);
    }
    
    /* Validate socket before binding */
    printf("Validating socket before bind operation\n");
    
    int socket_error = 0;
    socklen_t error_len = sizeof(socket_error);
    if (getsockopt(config->socket_fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_len) < 0) {
        fprintf(stderr, "Warning: Socket validation failed: %s (errno=%d)\n", 
                strerror(errno), errno);
    } else if (socket_error != 0) {
        fprintf(stderr, "Warning: Socket has error state: %s (error=%d)\n", 
                strerror(socket_error), socket_error);
    } else {
        printf("Socket is valid before bind\n");
    }
    
    /* Attempt to bind socket */
    printf("Binding socket %d to port %d...\n", config->socket_fd, config->port);
    
    /* First attempt to bind with configured port */
    int bind_result = bind(config->socket_fd, (struct sockaddr*)&address, sizeof(address));
    int bind_errno = errno;  /* Save original errno */
    
    printf("Initial bind result: %d\n", bind_result);
    
    /* If binding fails, try alternative approaches */
    if (bind_result < 0) {
        fprintf(stderr, "Failed to bind socket to port %d on address %s: %s (errno=%d)\n", 
                config->port, config->host ? config->host : "0.0.0.0", 
                strerror(bind_errno), bind_errno);
        
        /* Handle port already in use */
        if (bind_errno == EADDRINUSE) {
            fprintf(stderr, "Port %d is already in use\n", config->port);
            
            /* Try with a different port */
            int alternate_port = config->port + 1;
            
            fprintf(stderr, "Trying alternate port %d...\n", alternate_port);
            
            /* Close and recreate socket to ensure clean state */
            close(config->socket_fd);
            
            config->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (config->socket_fd < 0) {
                fprintf(stderr, "Error: Failed to recreate socket: %s (errno=%d)\n", 
                        strerror(errno), errno);
                return SERVER_SOCKET_ERROR;
            }
            
            /* Set socket options again */
            int socket_opt = 1;
            if (setsockopt(config->socket_fd, SOL_SOCKET, SO_REUSEADDR, &socket_opt, sizeof(socket_opt)) < 0) {
                fprintf(stderr, "Error: Failed to set SO_REUSEADDR on new socket: %s (errno=%d)\n", 
                        strerror(errno), errno);
                close(config->socket_fd);
                config->socket_fd = 0;
                return SERVER_SOCKET_ERROR;
            }
            
            #ifdef SO_REUSEPORT
            setsockopt(config->socket_fd, SOL_SOCKET, SO_REUSEPORT, &socket_opt, sizeof(socket_opt));
            #endif
            
            /* Update port in address structure */
            address.sin_port = htons(alternate_port);
            config->port = alternate_port;
            
            /* Try binding with alternate port */
            bind_result = bind(config->socket_fd, (struct sockaddr*)&address, sizeof(address));
            if (bind_result < 0) {
                fprintf(stderr, "Error: Failed to bind socket to alternate port %d: %s (errno=%d)\n", 
                        alternate_port, strerror(errno), errno);
                close(config->socket_fd);
                config->socket_fd = 0;
                return SERVER_BIND_ERROR;
            }
            
            fprintf(stderr, "Successfully bound to alternate port %d\n", alternate_port);
        } else {
            /* Other binding error - cleanup and return error */
            fprintf(stderr, "Error: Failed to bind socket to port %d: %s (errno=%d)\n", 
                    config->port, strerror(bind_errno), bind_errno);
            close(config->socket_fd);
            config->socket_fd = 0;
            return SERVER_BIND_ERROR;
        }
    } else {
        /* Binding successful on first attempt */
        printf("Successfully bound to port %d on first attempt\n", config->port);
    }
    
    /* Success! */
    g_socket_state = SOCKET_STATE_BOUND;
    printf("Socket successfully bound to port %d\n", config->port);
    
    return SERVER_OK;
}

/**
 * Set socket to listening state
 */
static int set_socket_to_listen(server_config_t* config) {
    if (!config) return SERVER_ERROR;
    
    if (config->socket_fd <= 0) {
        fprintf(stderr, "Error: Invalid socket file descriptor (%d) for listen\n", config->socket_fd);
        return SERVER_ERROR;
    }
    
    /* Ensure max_connections is reasonable */
    int backlog = config->max_connections;
    if (backlog <= 0) {
        backlog = 10; /* Default value */
    }
    
    printf("Setting socket %d to listen with backlog=%d\n", config->socket_fd, backlog);
    
    /* Attempt to listen */
    int listen_result = listen(config->socket_fd, backlog);
    int listen_errno = errno; /* Save original errno */
    
    printf("Listen result: %d\n", listen_result);
    
    if (listen_result < 0) {
        fprintf(stderr, "Error: Failed to listen on socket: %s (errno=%d)\n", 
                strerror(listen_errno), listen_errno);
        
        /* Try with minimum backlog as fallback */
        if (backlog > 1) {
            fprintf(stderr, "Retrying listen with minimum backlog...\n");
            
            listen_result = listen(config->socket_fd, 1);
            if (listen_result < 0) {
                fprintf(stderr, "Error: Fallback listen also failed: %s (errno=%d)\n", 
                        strerror(errno), errno);
                close(config->socket_fd);
                config->socket_fd = 0;
                return SERVER_LISTEN_ERROR;
            }
            
            fprintf(stderr, "Fallback listen succeeded with minimum backlog\n");
        } else {
            close(config->socket_fd);
            config->socket_fd = 0;
            return SERVER_LISTEN_ERROR;
        }
    } else {
        printf("Successfully set socket %d to listen state with backlog=%d\n", 
              config->socket_fd, backlog);
    }
    
    g_socket_state = SOCKET_STATE_LISTENING;
    
    return SERVER_OK;
}

/**
 * Verify socket is in proper state for accepting connections
 */
static int verify_socket_state(server_config_t* config) {
    if (!config) return SERVER_ERROR;
    
    if (config->socket_fd <= 0) {
        fprintf(stderr, "Error: Invalid socket file descriptor (%d) for verification\n", config->socket_fd);
        return SERVER_ERROR;
    }
    
    printf("Verifying socket %d state...\n", config->socket_fd);
    
    /* Check socket error state */
    int socket_error = 0;
    socklen_t error_len = sizeof(socket_error);
    if (getsockopt(config->socket_fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_len) < 0) {
        fprintf(stderr, "Warning: Socket validation failed: %s (errno=%d)\n", 
                strerror(errno), errno);
    } else if (socket_error != 0) {
        fprintf(stderr, "Warning: Socket has error state: %s (error=%d)\n", 
                strerror(socket_error), socket_error);
        return SERVER_SOCKET_ERROR;
    } else {
        printf("Socket is valid with no errors\n");
    }
    
    /* Check if socket is in listening state */
    int acceptconn = 0;
    socklen_t acceptconn_len = sizeof(acceptconn);
    if (getsockopt(config->socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &acceptconn, &acceptconn_len) < 0) {
        fprintf(stderr, "Warning: Failed to check SO_ACCEPTCONN: %s (errno=%d)\n", 
                strerror(errno), errno);
    } else {
        printf("Socket %d listening state: %s\n", 
              config->socket_fd, acceptconn ? "LISTENING" : "NOT LISTENING");
        
        if (!acceptconn) {
            fprintf(stderr, "Error: Socket is NOT in listening state after listen() call\n");
            return SERVER_SOCKET_ERROR;
        }
    }
    
    /* Verify with netstat */
    printf("Verifying port %d visibility with netstat...\n", config->port);
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "netstat -tuln | grep :%d || echo 'Port %d NOT FOUND in netstat'", 
             config->port, config->port);
    system(cmd);
    
    /* Set socket to non-blocking for accept */
#ifdef O_NONBLOCK
    printf("Setting socket to non-blocking mode\n");
    
    int flags = fcntl(config->socket_fd, F_GETFL, 0);
    if (flags >= 0) {
        if (fcntl(config->socket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            fprintf(stderr, "Warning: Failed to set socket non-blocking: %s (errno=%d)\n", 
                   strerror(errno), errno);
        } else {
            printf("Successfully set socket non-blocking\n");
        }
    }
#endif
    
    g_socket_state = SOCKET_STATE_READY;
    
    return SERVER_OK;
}

/**
 * Thread function for accepting connections
 */
void* server_accept_loop(void* config_ptr) {
    server_config_t* config = (server_config_t*)config_ptr;
    
    /* Signal initialization start */
    pthread_mutex_lock(&thread_init_mutex);
    g_thread_state = THREAD_STATE_CREATING;
    pthread_mutex_unlock(&thread_init_mutex);
    
    printf("Starting server accept thread (TID: %lu)...\n", (unsigned long)pthread_self());
    
    /* Validate configuration */
    if (!config) {
        fprintf(stderr, "Error: NULL server configuration passed to accept loop\n");
        
        /* Signal initialization failure */
        pthread_mutex_lock(&thread_init_mutex);
        g_thread_state = THREAD_STATE_ERROR;
        thread_initialized = -1;
        pthread_cond_signal(&thread_init_cond);
        pthread_mutex_unlock(&thread_init_mutex);
        
        return NULL;
    }
    
    /* Verify API context is available */
    if (!config->api_ctx) {
        fprintf(stderr, "Error: NULL API context in accept loop\n");
        
        /* Signal initialization failure */
        pthread_mutex_lock(&thread_init_mutex);
        g_thread_state = THREAD_STATE_ERROR;
        thread_initialized = -1;
        pthread_cond_signal(&thread_init_cond);
        pthread_mutex_unlock(&thread_init_mutex);
        
        return NULL;
    }
    
    /* Verify socket with detailed logging */
    if (config->socket_fd <= 0) {
        fprintf(stderr, "Error: Invalid socket descriptor (%d) in accept loop\n", config->socket_fd);
        
        /* Signal initialization failure */
        pthread_mutex_lock(&thread_init_mutex);
        g_thread_state = THREAD_STATE_ERROR;
        thread_initialized = -1;
        pthread_cond_signal(&thread_init_cond);
        pthread_mutex_unlock(&thread_init_mutex);
        
        return NULL;
    }
    
    /* Verify socket is in listening state */
    int acceptconn = 0;
    socklen_t acceptconn_len = sizeof(acceptconn);
    if (getsockopt(config->socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &acceptconn, &acceptconn_len) < 0) {
        fprintf(stderr, "Error: Failed to check if socket is in listening state: %s (errno=%d)\n", 
               strerror(errno), errno);
    } else if (acceptconn == 0) {
        fprintf(stderr, "Error: Socket is not in listening state\n");
        
        /* Try to restart listening */
        if (listen(config->socket_fd, 10) < 0) {
            fprintf(stderr, "Error: Failed to restart socket in listening state: %s (errno=%d)\n", 
                   strerror(errno), errno);
            
            /* Signal initialization failure */
            pthread_mutex_lock(&thread_init_mutex);
            g_thread_state = THREAD_STATE_ERROR;
            thread_initialized = -1;
            pthread_cond_signal(&thread_init_cond);
            pthread_mutex_unlock(&thread_init_mutex);
            
            return NULL;
        }
    }
    
    /* Verify socket state */
    int socket_error = 0;
    socklen_t error_len = sizeof(socket_error);
    if (getsockopt(config->socket_fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_len) < 0) {
        fprintf(stderr, "Error: Failed to check socket state: %s (errno=%d)\n", 
               strerror(errno), errno);
    } else if (socket_error != 0) {
        fprintf(stderr, "Error: Socket has error state: %s (error=%d)\n", 
               strerror(socket_error), socket_error);
        
        /* Signal initialization failure */
        pthread_mutex_lock(&thread_init_mutex);
        g_thread_state = THREAD_STATE_ERROR;
        thread_initialized = -1;
        pthread_cond_signal(&thread_init_cond);
        pthread_mutex_unlock(&thread_init_mutex);
        
        return NULL;
    }
    
    /* Signal successful initialization */
    pthread_mutex_lock(&thread_init_mutex);
    g_thread_state = THREAD_STATE_RUNNING;
    thread_initialized = 1;
    pthread_cond_signal(&thread_init_cond);
    pthread_mutex_unlock(&thread_init_mutex);
    
    printf("Server accept thread initialized successfully\n");
    
    /* Log the API context address for debugging */
    printf("API context in accept thread: %p\n", (void*)config->api_ctx);
    if (g_logger) {
        LOG_INFO("API context in accept thread: %p", (void*)config->api_ctx);
    }
    
    /* Handle the rest of the accept loop */
    /* ... */
    
    return NULL;
}

/**
 * Create the thread that will accept connections
 */
static pthread_t create_accept_thread(server_config_t* config) {
    if (!config) return 0;
    
    printf("Creating thread for accepting connections...\n");
    
    /* Verify socket state */
    if (config->socket_fd <= 0) {
        fprintf(stderr, "Error: Invalid socket descriptor (%d) for thread creation\n", 
                config->socket_fd);
        return 0;
    }
    
    /* Verify API context */
    if (!config->api_ctx) {
        fprintf(stderr, "Error: NULL API context for thread creation\n");
        return 0;
    }
    
    /* Create a thread-specific copy of the config */
    server_config_t* thread_config = malloc(sizeof(server_config_t));
    if (!thread_config) {
        perror("Failed to allocate memory for thread config");
        return 0;
    }
    
    /* Copy the config */
    memcpy(thread_config, config, sizeof(server_config_t));
    
    /* Double-check socket descriptor is valid */
    printf("Passing socket FD %d to accept thread\n", thread_config->socket_fd);
    
    /* Double-check API context is valid */
    printf("Passing API context %p to accept thread\n", (void*)thread_config->api_ctx);
    
    /* Initialize thread attributes */
    pthread_attr_t thread_attr;
    int attr_init_result = pthread_attr_init(&thread_attr);
    if (attr_init_result != 0) {
        fprintf(stderr, "Thread attributes initialization failed: %s (errno=%d)\n",
                strerror(attr_init_result), attr_init_result);
        free(thread_config);
        return 0;
    }
    
    /* Reset thread initialization flag */
    pthread_mutex_lock(&thread_init_mutex);
    thread_initialized = 0;
    pthread_mutex_unlock(&thread_init_mutex);
    
    /* Create the accept thread */
    pthread_t accept_thread;
    int thread_result = pthread_create(&accept_thread, &thread_attr, server_accept_loop, thread_config);
    
    /* Clean up thread attributes */
    pthread_attr_destroy(&thread_attr);
    
    if (thread_result != 0) {
        fprintf(stderr, "Failed to create server accept thread: %s (errno=%d)\n", 
                strerror(thread_result), thread_result);
        free(thread_config);
        return 0;
    }
    
    printf("Created accept thread with ID %lu\n", (unsigned long)accept_thread);
    
    return accept_thread;
}

/**
 * Wait for thread to initialize properly with timeout
 */
static int wait_for_thread_initialization(pthread_t thread_id, int timeout_ms) {
    struct timespec timeout;
    int result;
    
    /* Get current time */
    clock_gettime(CLOCK_REALTIME, &timeout);
    
    /* Add timeout */
    timeout.tv_sec += timeout_ms / 1000;
    timeout.tv_nsec += (timeout_ms % 1000) * 1000000;
    
    /* Normalize timespec */
    if (timeout.tv_nsec >= 1000000000) {
        timeout.tv_sec += 1;
        timeout.tv_nsec -= 1000000000;
    }
    
    printf("Waiting for accept thread %lu to initialize...\n", (unsigned long)thread_id);
    
    /* Wait with timeout */
    pthread_mutex_lock(&thread_init_mutex);
    
    while (thread_initialized == 0) {
        result = pthread_cond_timedwait(&thread_init_cond, &thread_init_mutex, &timeout);
        
        if (result == ETIMEDOUT) {
            pthread_mutex_unlock(&thread_init_mutex);
            fprintf(stderr, "Thread initialization timed out\n");
            return -1;
        }
        
        if (result != 0) {
            pthread_mutex_unlock(&thread_init_mutex);
            fprintf(stderr, "Error waiting for thread initialization: %s (errno=%d)\n", 
                    strerror(result), result);
            return -1;
        }
    }
    
    /* Check if initialization was successful */
    if (thread_initialized < 0) {
        pthread_mutex_unlock(&thread_init_mutex);
        fprintf(stderr, "Thread reported initialization failure\n");
        return -1;
    }
    
    pthread_mutex_unlock(&thread_init_mutex);
    printf("Accept thread initialized successfully\n");
    
    return 0;
}

/**
 * Stop the server
 */
void server_stop(server_config_t* config) {
    if (!config) {
        fprintf(stderr, "Warning: Attempt to stop NULL server config\n");
        return;
    }
    
    /* Set server stop flag */
    server_running = 0;
    
    /* Close server socket */
    if (config->socket_fd > 0) {
        printf("Closing server socket (FD: %d)\n", config->socket_fd);
        close(config->socket_fd);
        config->socket_fd = 0;
    }
    
    /* Wait for client threads to finish */
    usleep(100000);  /* 100ms delay */
    
    printf("Server stopped successfully\n");
}