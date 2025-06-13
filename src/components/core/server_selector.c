/**
 * Server mode selector for JDBX
 * 
 * Allows choosing between standard thread-pool server and high-performance epoll server
 * based on configuration or environment variables.
 */

#include "core/server.h"
#include "core/epoll_server.h"
#include "utils/logger.h"
#include "utils/environment.h"
#include <stdlib.h>
#include <string.h>

/* Performance mode configuration */
typedef enum {
    SERVER_MODE_STANDARD,   /* Standard thread-pool based server */
    SERVER_MODE_EPOLL      /* High-performance epoll-based server */
} server_mode_t;

/**
 * Determine server mode from environment or configuration
 */
static server_mode_t get_server_mode(void) {
    /* Check environment variable first */
    const char* mode_env = getenv("JDBX_SERVER_MODE");
    if (mode_env) {
        if (strcasecmp(mode_env, "epoll") == 0 || strcasecmp(mode_env, "high_performance") == 0) {
            return SERVER_MODE_EPOLL;
        }
        if (strcasecmp(mode_env, "standard") == 0 || strcasecmp(mode_env, "thread_pool") == 0) {
            return SERVER_MODE_STANDARD;
        }
    }
    
    /* Check for high connection load indicators */
    const char* max_conn_env = getenv("JDBX_MAX_CONNECTIONS");
    if (max_conn_env) {
        int max_connections = atoi(max_conn_env);
        if (max_connections > 1000) {
            /* High connection count - use epoll for better scalability */
            return SERVER_MODE_EPOLL;
        }
    }
    
    /* Default to standard mode for compatibility */
    return SERVER_MODE_STANDARD;
}

/**
 * Run server with automatic mode selection
 */
server_status_t run_server_auto(server_config_t* config, api_context_t* api_ctx) {
    if (!config || !api_ctx) {
        return SERVER_ERROR;
    }
    
    server_mode_t mode = get_server_mode();
    
    switch (mode) {
        case SERVER_MODE_EPOLL:
            if (g_logger) {
                LOG_INFO("Starting high-performance epoll server mode.");
            }
            return epoll_server_run(config, api_ctx);
            
        case SERVER_MODE_STANDARD:
        default:
            if (g_logger) {
                LOG_INFO("Starting standard thread-pool server mode.");
            }
            return run_server_standard(config, api_ctx);
    }
}

/**
 * Force epoll server mode
 */
server_status_t run_server_epoll(server_config_t* config, api_context_t* api_ctx) {
    if (!config || !api_ctx) {
        return SERVER_ERROR;
    }
    
    if (g_logger) {
        LOG_INFO("Forcing high-performance epoll server mode.");
    }
    
    return epoll_server_run(config, api_ctx);
}

/**
 * Force standard server mode 
 */
server_status_t run_server_standard(server_config_t* config, api_context_t* api_ctx) {
    if (!config || !api_ctx) {
        return SERVER_ERROR;
    }
    
    if (g_logger) {
        LOG_INFO("Forcing standard thread-pool server mode.");
    }
    
    return server_initialize_and_run(config, api_ctx);
}