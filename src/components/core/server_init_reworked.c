/**
 * Reworked server initialization with thread pool
 * 
 * This file contains a completely reworked server initialization sequence
 * that properly handles TTY, fork/daemon, and thread initialization.
 */

#include "core/server.h"
#include "api/api.h"
#include "utils/daemonize.h"
#include "utils/logger.h"
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
#include <signal.h>
#include <time.h>
#include <sys/syscall.h>

/* Graceful shutdown flag */
static volatile int g_shutdown_requested = 0;

/* Signal pipe for safe shutdown */
static int g_signal_pipe[2] = {-1, -1};

/* Forward declarations */
static int initialize_socket(server_config_t* config);
static int initialize_thread_pool(server_config_t* config);
static void* accept_thread_func(void* arg);
static void handle_signals();
static void signal_handler(int sig);
static int set_socket_non_blocking(int socket_fd);

/* Adapter function for thread pool compatibility */
static void handle_client_adapter(void* client_data) {
    /* Call the original handle_client function but discard its return value */
    handle_client(client_data);
}

/**
 * Initialize and run the server with improved design
 * 
 * This function implements a better initialization sequence:
 * 1. Set up signal handlers FIRST
 * 2. Initialize socket AFTER daemonization (if applicable)
 * 3. Create thread pool
 * 4. Enter accept loop in main thread
 * 
 * @param config Server configuration
 * @param api_ctx API context
 * @return Server status
 */
server_status_t server_initialize_and_run(server_config_t* config, api_context_t* api_ctx) {
    if (!config || !api_ctx) {
        fprintf(stderr, "Error: Invalid server config or API context\n");
        return SERVER_ERROR;
    }
    
    /* Store API context in config */
    config->api_ctx = api_ctx;
    
    /* Set up signal handling first (before any threads) */
    handle_signals();
    
    /* Create signal pipe for safe shutdown */
    if (pipe(g_signal_pipe) < 0) {
        fprintf(stderr, "Error: Failed to create signal pipe: %s\n", strerror(errno));
        return SERVER_ERROR;
    }
    
    /* Set pipe to non-blocking */
    for (int i = 0; i < 2; i++) {
        int flags = fcntl(g_signal_pipe[i], F_GETFL, 0);
        if (flags < 0 || fcntl(g_signal_pipe[i], F_SETFL, flags | O_NONBLOCK) < 0) {
            fprintf(stderr, "Warning: Failed to set signal pipe to non-blocking\n");
        }
    }
    
    /* Print startup message */
    printf("Starting JSONdb server on port %d...\n", config->port);
    
    /* Initialize socket */
    if (initialize_socket(config) != 0) {
        fprintf(stderr, "Error: Failed to initialize server socket\n");
        return SERVER_SOCKET_ERROR;
    }
    
    printf("Socket initialization successful (fd=%d)\n", config->socket_fd);
    
    /* Initialize thread pool */
    if (initialize_thread_pool(config) != 0) {
        fprintf(stderr, "Error: Failed to initialize thread pool\n");
        return SERVER_THREAD_ERROR;
    }
    
    /* Start accept loop in the current thread */
    printf("Starting accept loop on socket %d (port %d)\n", config->socket_fd, config->port);
    accept_thread_func(config);
    
    /* Clean up resources */
    printf("Shutting down server...\n");
    
    /* Close socket */
    if (config->socket_fd > 0) {
        close(config->socket_fd);
        config->socket_fd = 0;
    }
    
    /* Destroy thread pool */
    if (config->thread_pool) {
        thread_pool_destroy(config->thread_pool);
        config->thread_pool = NULL;
    }
    
    /* Close signal pipe */
    for (int i = 0; i < 2; i++) {
        if (g_signal_pipe[i] >= 0) {
            close(g_signal_pipe[i]);
            g_signal_pipe[i] = -1;
        }
    }
    
    printf("Server shutdown complete\n");
    
    return SERVER_OK;
}

/**
 * Initialize server socket
 */
static int initialize_socket(server_config_t* config) {
    if (!config) {
        fprintf(stderr, "Error: NULL server configuration\n");
        return -1;
    }
    
    /* Create socket */
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        fprintf(stderr, "Error: Failed to create socket: %s\n", strerror(errno));
        return -1;
    }
    
    printf("Created socket with descriptor %d\n", socket_fd);
    
    /* Set socket options */
    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        fprintf(stderr, "Error: Failed to set SO_REUSEADDR: %s\n", strerror(errno));
        close(socket_fd);
        return -1;
    }
    
    /* Set timeouts */
    struct timeval timeout;
    timeout.tv_sec = 30;  /* 30 second timeout */
    timeout.tv_usec = 0;
    
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        fprintf(stderr, "Warning: Failed to set SO_RCVTIMEO: %s\n", strerror(errno));
    }
    
    if (setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        fprintf(stderr, "Warning: Failed to set SO_SNDTIMEO: %s\n", strerror(errno));
    }
    
    /* Create address structure */
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(config->port);
    
    printf("Binding to port %d...\n", config->port);
    
    /* Bind socket */
    if (bind(socket_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        fprintf(stderr, "Error: Failed to bind socket: %s\n", strerror(errno));
        
        /* Try using an alternate port if the original port is in use */
        if (errno == EADDRINUSE) {
            int alternate_port = config->port + 1;
            printf("Port %d is in use, trying alternate port %d...\n", config->port, alternate_port);
            
            /* Update port in address structure */
            address.sin_port = htons(alternate_port);
            
            /* Try binding again */
            if (bind(socket_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
                fprintf(stderr, "Error: Failed to bind socket to alternate port: %s\n", strerror(errno));
                close(socket_fd);
                return -1;
            }
            
            /* Update config with new port */
            config->port = alternate_port;
            printf("Successfully bound to alternate port %d\n", alternate_port);
        } else {
            close(socket_fd);
            return -1;
        }
    }
    
    /* Set socket to listen state */
    if (listen(socket_fd, config->max_connections > 0 ? config->max_connections : 10) < 0) {
        fprintf(stderr, "Error: Failed to listen on socket: %s\n", strerror(errno));
        close(socket_fd);
        return -1;
    }
    
    /* Set socket to non-blocking mode */
    if (set_socket_non_blocking(socket_fd) < 0) {
        fprintf(stderr, "Warning: Failed to set socket to non-blocking mode\n");
    }
    
    /* Store socket descriptor in config */
    config->socket_fd = socket_fd;
    
    printf("Socket successfully initialized and listening on port %d\n", config->port);
    
    return 0;
}

/**
 * Initialize thread pool
 */
static int initialize_thread_pool(server_config_t* config) {
    if (!config) {
        fprintf(stderr, "Error: NULL server configuration\n");
        return -1;
    }
    
    /* Calculate thread pool size based on configuration */
    int min_threads = 4;  /* Default minimum */
    int max_threads = config->max_connections > 0 ? config->max_connections : 16;
    
    thread_pool_config_t pool_config = {
        .min_threads = min_threads,
        .max_threads = max_threads,
        .queue_size = max_threads * 4,  /* Queue size proportional to max threads */
        .idle_timeout = 60  /* 1 minute idle timeout */
    };
    
    /* Create thread pool */
    config->thread_pool = thread_pool_create_config(&pool_config);
    if (!config->thread_pool) {
        fprintf(stderr, "Error: Failed to create thread pool\n");
        return -1;
    }
    
    printf("Thread pool created successfully with %d-%d threads\n", 
           pool_config.min_threads, pool_config.max_threads);
    
    return 0;
}

/**
 * Accept thread function
 */
static void* accept_thread_func(void* arg) {
    server_config_t* config = (server_config_t*)arg;
    
    if (!config) {
        fprintf(stderr, "Error: NULL server configuration in accept thread\n");
        return NULL;
    }
    
    /* Get thread ID for logging */
    pthread_t tid = pthread_self();
    pid_t system_tid = (pid_t)syscall(SYS_gettid);
    
    printf("Accept thread started (pthread_id=%lu, system_tid=%d)\n", 
           (unsigned long)tid, system_tid);
    
    /* Verify socket is valid */
    if (config->socket_fd <= 0) {
        fprintf(stderr, "Error: Invalid socket descriptor in accept thread\n");
        return NULL;
    }
    
    /* Verify API context is available */
    if (!config->api_ctx) {
        fprintf(stderr, "Error: NULL API context in accept thread\n");
        return NULL;
    }
    
    /* Verify thread pool is available */
    if (!config->thread_pool) {
        fprintf(stderr, "Error: NULL thread pool in accept thread\n");
        return NULL;
    }
    
    /* Accept loop */
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd;
    int connection_count = 0;
    
    /* Set up file descriptor set for select */
    fd_set read_fds;
    int max_fd = config->socket_fd;
    
    /* If signal pipe is active, include it in select */
    if (g_signal_pipe[0] >= 0 && g_signal_pipe[0] > max_fd) {
        max_fd = g_signal_pipe[0];
    }
    
    printf("Starting accept loop on socket %d (port %d)\n", config->socket_fd, config->port);
    
    /* Continue until shutdown is requested */
    while (!g_shutdown_requested) {
        /* Clear and set file descriptor set */
        FD_ZERO(&read_fds);
        FD_SET(config->socket_fd, &read_fds);
        
        /* Add signal pipe to set */
        if (g_signal_pipe[0] >= 0) {
            FD_SET(g_signal_pipe[0], &read_fds);
        }
        
        /* Wait for activity with timeout */
        struct timeval tv;
        tv.tv_sec = 1;  /* 1 second timeout */
        tv.tv_usec = 0;
        
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
        
        /* Check for errors */
        if (activity < 0) {
            if (errno == EINTR) {
                /* Interrupted by signal, continue */
                continue;
            }
            
            fprintf(stderr, "Error: select() failed: %s\n", strerror(errno));
            break;
        }
        
        /* Check for timeout */
        if (activity == 0) {
            /* No activity, continue */
            continue;
        }
        
        /* Check for signal pipe activity */
        if (g_signal_pipe[0] >= 0 && FD_ISSET(g_signal_pipe[0], &read_fds)) {
            /* Received shutdown signal */
            char buffer[10];
            read(g_signal_pipe[0], buffer, sizeof(buffer));
            printf("Received shutdown signal in accept thread\n");
            break;
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
                continue;
            }
            
            /* Connection accepted */
            connection_count++;
            printf("Accepted connection #%d from %s:%d\n", 
                   connection_count, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            
            /* Create client connection structure */
            client_conn_t* client = (client_conn_t*)malloc(sizeof(client_conn_t));
            if (!client) {
                fprintf(stderr, "Error: Failed to allocate memory for client connection\n");
                close(client_fd);
                continue;
            }
            
            /* Initialize client connection */
            client->client_fd = client_fd;
            client->address = client_addr;
            client->api_ctx = config->api_ctx;
            
            /* Add client to thread pool using our adapter function */
            if (thread_pool_add_work(config->thread_pool, handle_client_adapter, client) != 0) {
                fprintf(stderr, "Error: Failed to add client to thread pool\n");
                free(client);
                close(client_fd);
                continue;
            }
            
            /* Log thread pool statistics periodically */
            if (connection_count % 10 == 0) {
                int active_threads = 0;
                int queue_size = 0;
                uint64_t tasks_processed = 0;
                
                thread_pool_stats(config->thread_pool, &active_threads, &queue_size, &tasks_processed);
                
                printf("Thread pool stats: active=%d, queue=%d, processed=%llu\n", 
                       active_threads, queue_size, (unsigned long long)tasks_processed);
            }
        }
    }
    
    printf("Accept loop terminated after handling %d connections\n", connection_count);
    
    return NULL;
}

/**
 * Set up signal handlers
 */
static void handle_signals() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    
    /* Register signal handlers */
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        fprintf(stderr, "Warning: Failed to register SIGINT handler\n");
    }
    
    if (sigaction(SIGTERM, &sa, NULL) < 0) {
        fprintf(stderr, "Warning: Failed to register SIGTERM handler\n");
    }
    
    if (sigaction(SIGPIPE, &sa, NULL) < 0) {
        fprintf(stderr, "Warning: Failed to register SIGPIPE handler\n");
    }
    
    printf("Signal handlers registered\n");
}

/**
 * Signal handler function
 */
static void signal_handler(int sig) {
    /* Set shutdown flag */
    g_shutdown_requested = 1;
    
    /* Only certain operations are safe in signal handlers */
    const char* signal_name = sig == SIGINT ? "SIGINT" :
                              sig == SIGTERM ? "SIGTERM" :
                              sig == SIGPIPE ? "SIGPIPE" : "Unknown";
    
    /* Write to stderr (should be safe) */
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "Received signal %s (%d), initiating shutdown\n", signal_name, sig);
    write(STDERR_FILENO, buffer, strlen(buffer));
    
    /* Write to signal pipe (safe) */
    if (g_signal_pipe[1] >= 0) {
        char byte = 1;
        write(g_signal_pipe[1], &byte, 1);
    }
}

/**
 * Request server shutdown from another function
 */
void server_request_shutdown() {
    /* Set shutdown flag */
    g_shutdown_requested = 1;
    
    /* Signal through pipe */
    if (g_signal_pipe[1] >= 0) {
        char byte = 1;
        write(g_signal_pipe[1], &byte, 1);
    }
    
    printf("Server shutdown requested\n");
}

/**
 * Set socket to non-blocking mode
 */
static int set_socket_non_blocking(int socket_fd) {
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0) {
        fprintf(stderr, "Error: fcntl(F_GETFL) failed: %s\n", strerror(errno));
        return -1;
    }
    
    if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        fprintf(stderr, "Error: fcntl(F_SETFL, O_NONBLOCK) failed: %s\n", strerror(errno));
        return -1;
    }
    
    return 0;
}