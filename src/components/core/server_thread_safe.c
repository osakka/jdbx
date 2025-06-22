#include "core/client_connection.h"
#include "core/server.h" 
#include "core/server_thread_safe.h"
#include "core/thread_pool.h"
#include "utils/logger.h"
#include "utils/metrics.h"
#include "utils/memory_manager.h"
#include "core/rate_limiter.h"
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>
#include "utils/buffer_pool.h"

/* Forward declarations for functions we need */
extern http_request_t* parse_http_request(const char* request_str);
extern void free_http_request(http_request_t* request);
extern void free_http_response(http_response_t* response);
extern http_response_t* api_dispatch_request(struct api_context* ctx, http_request_t* request);
extern metric_t* get_server_request_duration_metric(void);
extern metric_t* get_active_connections_metric(void);
extern int thread_pool_add_work(thread_pool_t* pool, void (*function)(void*), void* argument);

/* External rate limiter instance */
extern rate_limiter_t* g_rate_limiter;


/* Global thread-safe mode flag */
static int g_thread_safe_mode_enabled = 0;


/* handle_client_thread_safe has been removed - use handle_client directly */

/**
 * Enhanced server accept loop with thread-safe connection management
 * 
 * This now uses the standard handle_client which has full SSL support
 */
void server_accept_loop_thread_safe_direct(server_config_t *config) {
    LOG_INFO("Starting thread-safe accept loop (using unified SSL-aware handler).");
    
    if (!config) {
        LOG_ERROR("Null configuration passed to accept loop.");
        return;
    }
    
    TRACE_NET("Accept loop config: socket_fd=%d, max_connections=%d, use_ssl=%d", 
             config->socket_fd, config->max_connections, config->use_ssl);
    
    /* Validate essential components before starting */
    if (config->socket_fd <= 0) {
        LOG_ERROR("Invalid socket_fd=%d", config->socket_fd);
        return;
    }
    
    if (!config->api_ctx) {
        LOG_ERROR("Missing API context.");
        return;
    }
    
    if (!config->thread_pool) {
        LOG_ERROR("Missing thread pool.");
        return;
    }
    
    LOG_DEBUG("Essential components validated.");
    
    /* Initialize connection manager */
    int max_connections = config->max_connections ? config->max_connections : 1000;
    TRACE_NET("Initializing connection manager: max_conn=%d", max_connections);
    
    if (connection_manager_init(
        max_connections,  /* max connections */
        16384,           /* max request buffer (16KB) */
        65536,           /* max response buffer (64KB) */
        300              /* connection timeout (5 minutes) */
    ) != 0) {
        LOG_ERROR("Cannot initialize connection manager.");
        return;
    }
    
    LOG_INFO("Connection manager initialized.");
    
    /* Verify socket state before starting loop */
    int acceptconn = 0;
    socklen_t acceptconn_len = sizeof(acceptconn);
    if (getsockopt(config->socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &acceptconn, &acceptconn_len) < 0) {
        LOG_ERROR("Cannot check socket listen state: %s", strerror(errno));
        connection_manager_shutdown();
        return;
    }
    
    if (!acceptconn) {
        LOG_ERROR("Socket %d is not in listening state", config->socket_fd);
        connection_manager_shutdown();
        return;
    }
    
    LOG_DEBUG("Socket %d verified in listening state", config->socket_fd);
    
    /* Get and log bound address */
    struct sockaddr_in bound_addr;
    socklen_t bound_len = sizeof(bound_addr);
    if (getsockname(config->socket_fd, (struct sockaddr*)&bound_addr, &bound_len) == 0) {
        LOG_INFO("Socket bound to %s:%d", 
                 inet_ntoa(bound_addr.sin_addr), ntohs(bound_addr.sin_port));
    }
    
    LOG_INFO("Accept loop starting on fd %d", config->socket_fd);
    
    int connection_count = 0;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    
    while (1) {
        TRACE_NET("Starting accept iteration %d", connection_count + 1);
        
        /* Accept new connection */
        int client_fd = accept(config->socket_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        
        if (client_fd < 0) {
            if (errno == EINTR) {
                TRACE_NET("Accept interrupted by signal, continuing.");
                continue; /* Interrupted by signal, try again */
            }
            
            LOG_ERROR("Accept failed: %s", strerror(errno));
            
            /* Log additional socket diagnostics */
            int socket_error = 0;
            socklen_t error_len = sizeof(socket_error);
            if (getsockopt(config->socket_fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_len) == 0) {
                LOG_ERROR("Socket error state: %d (%s)", 
                          socket_error, socket_error ? strerror(socket_error) : "no error");
            }
            
            break;
        }
        
        connection_count++;
        
        LOG_DEBUG("Connection #%d accepted, client_fd=%d, from %s:%d", 
                 connection_count, client_fd, 
                 inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        /* Check connection rate limit */
        const char* client_ip = inet_ntoa(client_addr.sin_addr);
        if (g_rate_limiter && !connection_rate_check(g_rate_limiter, client_ip)) {
            LOG_WARNING("Connection rate limit exceeded for IP %s - rejecting connection", client_ip);
            close(client_fd);
            continue;
        }
        
        /* Record the connection for rate limiting */
        if (g_rate_limiter) {
            connection_rate_record(g_rate_limiter, client_ip);
        }
        
        /* Create client_conn_t structure for the unified handler */
        client_conn_t* client = (client_conn_t*)BUFFER_ALLOC(sizeof(client_conn_t));
        if (!client) {
            LOG_ERROR("Cannot allocate memory for client connection");
            close(client_fd);
            continue;
        }
        
        /* CRITICAL: Client connections must survive checkpoint rewinds as they're used
         * throughout the entire request handling lifecycle. The thread pool worker
         * creates checkpoints during request processing, but the client structure
         * must persist beyond those checkpoints. */
        memory_promote(client);
        
        /* Initialize client structure */
        client->client_fd = client_fd;
        client->api_ctx = config->api_ctx;
        client->address = client_addr;
        client->use_ssl = config->use_ssl;
        client->ssl_conn = NULL;
        
        LOG_DEBUG("Created client structure for fd %d, SSL=%d", client_fd, config->use_ssl);
        
        /* Add to thread pool using the unified handler */
        if (thread_pool_add_work(config->thread_pool, handle_client, client) != 0) {
            LOG_ERROR("Cannot add connection to thread pool");
            close(client_fd);
            BUFFER_FREE(client);
            continue;
        }
        
        TRACE_NET("Connection added to thread pool for processing");
        
        /* Periodic cleanup and statistics */
        if (connection_count % 100 == 0) {
            size_t active_count;
            uint64_t total_created;
            connection_manager_get_stats(&active_count, &total_created);
            
            LOG_DEBUG("Connection stats: active=%zu, total_created=%lu", active_count, total_created);
        }
    }
    
    LOG_INFO("Accept loop terminated after handling %d connections", connection_count);
    
    /* Shutdown connection manager */
    connection_manager_shutdown();
    LOG_INFO("Connection manager shutdown complete.");
}

/**
 * Initialize thread-safe server components
 */
int server_init_thread_safe(server_config_t *config) {
    if (!config) {
        LOG_ERROR("Null configuration for thread-safe server init.");
        return -1;
    }
    
    LOG_INFO("Thread-safe server components initialized.");
    return 0;
}

/**
 * Get connection statistics
 */
void server_get_connection_stats(size_t *active_connections, uint64_t *total_connections, 
                                int *thread_pool_active, int *thread_pool_queue_size) {
    /* Get connection manager stats */
    if (active_connections || total_connections) {
        connection_manager_get_stats(active_connections, total_connections);
    }
    
    /* Get thread pool stats if available */
    if (thread_pool_active || thread_pool_queue_size) {
        /* These would need to be implemented in the thread pool module */
        if (thread_pool_active) *thread_pool_active = 0;
        if (thread_pool_queue_size) *thread_pool_queue_size = 0;
    }
}

/**
 * Enable thread-safe mode
 */
void server_enable_thread_safe_mode(void) {
    g_thread_safe_mode_enabled = 1;
    LOG_INFO("Thread-safe mode enabled.");
}

/**
 * Check if thread-safe mode is enabled
 */
int server_is_thread_safe_mode_enabled(void) {
    return g_thread_safe_mode_enabled;
}