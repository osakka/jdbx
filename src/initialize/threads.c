#include "init.h"
#include "core/server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Initialize thread pool */
init_status_t init_threads(server_config_t* config) {
    INIT_LOG_PROGRESS("THREADS", "Initializing thread pool");
    
    if (!config) {
        INIT_LOG_FAILURE("THREADS", "NULL server configuration");
        return INIT_THREAD_ERROR;
    }
    
    /* Calculate thread pool size based on configuration */
    int min_threads = config->max_connections > 0 ? config->max_connections / 4 : 4;
    int max_threads = config->max_connections > 0 ? config->max_connections : 16;
    
    INIT_LOG_PROGRESS("THREADS", "Configuring thread pool with min=%d, max=%d threads", 
                   min_threads, max_threads);
    
    thread_pool_config_t pool_config = {
        .min_threads = min_threads,
        .max_threads = max_threads,
        .queue_size = max_threads * 4,  /* Queue size proportional to max threads */
        .idle_timeout = 60  /* 1 minute idle timeout */
    };
    
    /* Create thread pool */
    config->thread_pool = thread_pool_create_config(&pool_config);
    if (!config->thread_pool) {
        INIT_LOG_FAILURE("THREADS", "Failed to create thread pool");
        return INIT_THREAD_ERROR;
    }
    
    INIT_LOG_SUCCESS("THREADS", "Thread pool created successfully with %d-%d threads", 
                  pool_config.min_threads, pool_config.max_threads);
    
    return INIT_OK;
}

/* Run server main loop */
init_status_t run_server(server_config_t* config) {
    INIT_LOG_PROGRESS("SERVER", "Starting server main loop");
    
    if (!config) {
        INIT_LOG_FAILURE("SERVER", "NULL server configuration");
        return INIT_ERROR;
    }
    
    /* Verify socket descriptor is valid before proceeding */
    if (config->socket_fd <= 0) {
        INIT_LOG_FAILURE("SERVER", "Invalid socket descriptor (%d)", config->socket_fd);
        return INIT_SOCKET_ERROR;
    }
    
    /* Verify API context is available */
    if (!config->api_ctx) {
        INIT_LOG_FAILURE("SERVER", "NULL API context");
        return INIT_API_ERROR;
    }
    
    /* For foreground mode, just run the server directly */
    if (config->verbose_mode) {
        /* Start the server with our thread pool implementation */
        INIT_LOG_PROGRESS("SERVER", "Running server in foreground with socket %d", config->socket_fd);
        server_status_t server_status = server_initialize_and_run(config, config->api_ctx);
        
        /* Check if the server completed initialization */
        INIT_LOG_PROGRESS("SERVER", "Server completed with status: %d", server_status);
        
        /* Handle server status */
        if (server_status != SERVER_OK) {
            INIT_LOG_FAILURE("SERVER", "Server failed to start or run with status: %d", server_status);
            return INIT_ERROR;
        }
        
        INIT_LOG_SUCCESS("SERVER", "Server shutdown completed successfully");
        
        return INIT_OK;
    } else {
        /* In daemon mode, let the server run independently (our parent has already exited) */
        /* Just return success without waiting for server to exit */
        INIT_LOG_SUCCESS("SERVER", "Server started in daemon mode (continuing independently)");
        
        /* Fork again to avoid blocking */
        pid_t pid = fork();
        if (pid < 0) {
            INIT_LOG_FAILURE("SERVER", "Failed to fork server process: %s", strerror(errno));
            return INIT_ERROR;
        } else if (pid > 0) {
            /* Parent process exits normally */
            return INIT_OK;
        }
        
        /* Child process runs the server */
        INIT_LOG_PROGRESS("SERVER", "Running server in daemon mode with socket %d", config->socket_fd);
        server_status_t server_status = server_initialize_and_run(config, config->api_ctx);
        
        /* This code should only run when server shuts down */
        if (server_status != SERVER_OK) {
            INIT_LOG_FAILURE("SERVER", "Server failed with status: %d", server_status);
            exit(1);
        }
        
        exit(0);
    }
}