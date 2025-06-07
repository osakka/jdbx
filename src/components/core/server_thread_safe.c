#include "core/client_connection.h"
#include "core/server.h" 
#include "core/server_thread_safe.h"
#include "core/thread_pool.h"
#include "utils/logger.h"
#include "utils/metrics.h"
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>

/* Forward declarations for functions we need */
extern http_request_t* parse_http_request(const char* request_str);
extern void free_http_request(http_request_t* request);
extern void free_http_response(http_response_t* response);
extern http_response_t* api_dispatch_request(struct api_context* ctx, http_request_t* request);
extern metric_t* get_server_request_duration_metric(void);
extern metric_t* get_active_connections_metric(void);
extern int thread_pool_add_work(thread_pool_t* pool, void (*function)(void*), void* argument);


/* Global thread-safe mode flag */
static int g_thread_safe_mode_enabled = 0;


/**
 * Thread-safe client handler that uses the new connection management system
 * WITH COMPREHENSIVE TRACING FOR DEBUGGING
 */
void handle_client_thread_safe(void* client_data) {
    LOG_TRACE("TRACE_HANDLER_START: Entering thread-safe client handler, thread_data=%p", client_data);
    
    /* Get start time for performance tracking */
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    /* Start request timer for metrics (skip if disabled for performance) */
    timer_context_t* request_timer = NULL;
    metric_t* request_duration_metric = NULL;
    metric_t* active_connections = NULL;
    
    request_duration_metric = get_server_request_duration_metric();
    if (request_duration_metric) {
        request_timer = metrics_timer_start(request_duration_metric);
    }
    
    /* Increment active connections */
    active_connections = get_active_connections_metric();
    if (active_connections) {
        metrics_gauge_inc(active_connections, 1.0);
    }
    
    /* Cast to our thread-safe connection structure */
    client_connection_t *conn = (client_connection_t*)client_data;
    
    if (!conn) {
        LOG_ERROR("TRACE_HANDLER_ERROR: Null connection passed to thread-safe handler");
        goto cleanup;
    }
    
    LOG_TRACE("TRACE_HANDLER_CONN: Initial connection pointer=%p, connection_id=%lu", 
              (void*)conn, conn->connection_id);
    
    /* Acquire a reference to ensure connection stays valid */
    LOG_TRACE("TRACE_HANDLER_ACQUIRE: Attempting to acquire connection reference...");
    client_connection_t *safe_conn = client_connection_acquire(conn);
    if (!safe_conn) {
        LOG_WARNING("TRACE_HANDLER_ERROR: Failed to acquire connection reference - connection may be closing");
        goto cleanup;
    }
    
    LOG_INFO("TRACE_HANDLER_ACQUIRED: Successfully acquired reference to connection %lu", safe_conn->connection_id);
    
    /* Read HTTP request with timeout - no need to set state, client_connection_read will handle it */
    LOG_TRACE("TRACE_HANDLER_READ: Starting HTTP request read for connection %lu (timeout=5000ms)...", safe_conn->connection_id);
    char request_buffer[8192];
    ssize_t bytes_read = client_connection_read(safe_conn, request_buffer, sizeof(request_buffer) - 1, 5000); /* 5 second timeout */
    
    LOG_TRACE("TRACE_HANDLER_READ: Read operation completed, bytes_read=%zd", bytes_read);
    
    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            LOG_DEBUG("TRACE_HANDLER_CLOSED: Connection %lu closed by client", safe_conn->connection_id);
        } else {
            int error_code;
            char error_msg[256];
            if (client_connection_get_error(safe_conn, &error_code, error_msg, sizeof(error_msg))) {
                LOG_WARNING("TRACE_HANDLER_ERROR: Connection %lu read error: %s", safe_conn->connection_id, error_msg);
            } else {
                LOG_WARNING("TRACE_HANDLER_ERROR: Connection %lu read failed: %s", safe_conn->connection_id, strerror(errno));
            }
        }
        client_connection_set_state(safe_conn, CONN_STATE_CLOSING);
        client_connection_release(safe_conn);
        LOG_TRACE("TRACE_HANDLER_EXIT: Exiting handler due to read failure");
        goto cleanup;
    }
    
    /* Null-terminate the request */
    request_buffer[bytes_read] = '\0';
    
    LOG_INFO("TRACE_HANDLER_DATA: Connection %lu received %zd bytes of HTTP data", safe_conn->connection_id, bytes_read);
    LOG_TRACE("TRACE_HANDLER_RAW: First 100 chars: %.100s", request_buffer);
    
    /* Parse HTTP request */
    LOG_TRACE("TRACE_HANDLER_PARSE: Parsing HTTP request for connection %lu...", safe_conn->connection_id);
    http_request_t *request = parse_http_request(request_buffer);
    if (!request) {
        LOG_WARNING("TRACE_HANDLER_ERROR: Connection %lu: Failed to parse HTTP request", safe_conn->connection_id);
        
        /* Send 400 Bad Request */
        const char *bad_request_response = 
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 11\r\n"
            "Connection: close\r\n"
            "\r\n"
            "Bad Request";
        
        LOG_TRACE("TRACE_HANDLER_RESPONSE: Sending 400 Bad Request to connection %lu", safe_conn->connection_id);
        client_connection_write(safe_conn, bad_request_response, strlen(bad_request_response));
        client_connection_set_state(safe_conn, CONN_STATE_CLOSING);
        client_connection_release(safe_conn);
        LOG_TRACE("TRACE_HANDLER_EXIT: Exiting handler due to parse failure");
        goto cleanup;
    }
    
    LOG_TRACE("TRACE_HANDLER_PARSE: HTTP request parsed successfully for connection %lu", safe_conn->connection_id);
    
    /* Set client IP address */
    char ip_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &(safe_conn->client_address.sin_addr), ip_str, INET_ADDRSTRLEN)) {
        request->remote_addr = strdup(ip_str);
        LOG_TRACE("TRACE_HANDLER_IP: Set remote_addr=%s for connection %lu", ip_str, safe_conn->connection_id);
    }
    
    /* Set processing state now that we have a valid request */
    LOG_TRACE("TRACE_HANDLER_STATE: Setting connection %lu to PROCESSING state...", safe_conn->connection_id);
    if (client_connection_set_state(safe_conn, CONN_STATE_PROCESSING) != 0) {
        LOG_WARNING("TRACE_HANDLER_ERROR: Failed to set PROCESSING state for connection %lu", safe_conn->connection_id);
        free_http_request(request);
        client_connection_release(safe_conn);
        goto cleanup;
    }
    
    /* Get API context from connection */
    LOG_TRACE("TRACE_HANDLER_API: Getting API context for connection %lu...", safe_conn->connection_id);
    struct api_context *api_ctx = NULL;
    WITH_CONNECTION_LOCK(safe_conn, {
        api_ctx = safe_conn->api_ctx;
    });
    
    if (!api_ctx) {
        LOG_ERROR("TRACE_HANDLER_ERROR: Connection %lu: No API context available", safe_conn->connection_id);
        free_http_request(request);
        client_connection_set_state(safe_conn, CONN_STATE_ERROR);
        client_connection_release(safe_conn);
        LOG_TRACE("TRACE_HANDLER_EXIT: Exiting handler due to missing API context");
        goto cleanup;
    }
    
    LOG_TRACE("TRACE_HANDLER_API: API context %p retrieved for connection %lu", (void*)api_ctx, safe_conn->connection_id);
    
    /* Check if this is an admin route (static file) first */
    http_response_t *response = NULL;
    
    extern int is_admin_route(const char* path);
    extern http_response_t* serve_admin_file(const char* path);
    
    if ((request->method == HTTP_GET || request->method == HTTP_HEAD) && is_admin_route(request->path)) {
        LOG_TRACE("TRACE_HANDLER_ADMIN: Serving admin file for path: %s", request->path);
        response = serve_admin_file(request->path);
    } else {
        /* Process the request through the API */
        LOG_TRACE("TRACE_HANDLER_DISPATCH: Dispatching API request for connection %lu...", safe_conn->connection_id);
        response = api_dispatch_request(api_ctx, request);
    }
    if (!response) {
        LOG_ERROR("TRACE_HANDLER_ERROR: Connection %lu: API dispatch failed", safe_conn->connection_id);
        
        /* Send 500 Internal Server Error */
        const char *error_response = 
            "HTTP/1.1 500 Internal Server Error\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 21\r\n"
            "Connection: close\r\n"
            "\r\n"
            "Internal Server Error";
        
        LOG_TRACE("TRACE_HANDLER_RESPONSE: Sending 500 Internal Server Error to connection %lu", safe_conn->connection_id);
        client_connection_write(safe_conn, error_response, strlen(error_response));
        free_http_request(request);
        client_connection_set_state(safe_conn, CONN_STATE_CLOSING);
        client_connection_release(safe_conn);
        LOG_TRACE("TRACE_HANDLER_EXIT: Exiting handler due to API dispatch failure");
        goto cleanup;
    }
    
    LOG_TRACE("TRACE_HANDLER_DISPATCH: Request processing successful for connection %lu, response status=%d", 
              safe_conn->connection_id, response->status);
    
    /* Serialize HTTP response properly (handles binary content) */
    LOG_TRACE("TRACE_HANDLER_BUILD: Serializing HTTP response for connection %lu...", safe_conn->connection_id);
    
    extern char* serialize_http_response_keep_alive_with_length(http_response_t* response, int keep_alive, size_t* length);
    size_t response_len = 0;
    char* response_str = serialize_http_response_keep_alive_with_length(response, request->keep_alive, &response_len);
    
    if (!response_str) {
        LOG_ERROR("TRACE_HANDLER_ERROR: Connection %lu: Failed to serialize response", safe_conn->connection_id);
        free_http_request(request);
        free_http_response(response);
        client_connection_set_state(safe_conn, CONN_STATE_ERROR);
        client_connection_release(safe_conn);
        LOG_TRACE("TRACE_HANDLER_EXIT: Exiting handler due to serialization failure");
        goto cleanup;
    }
    
    LOG_TRACE("TRACE_HANDLER_BUILD: HTTP response serialized successfully for connection %lu, length=%zu", 
              safe_conn->connection_id, response_len);
    
    /* Send response */
    LOG_TRACE("TRACE_HANDLER_SEND: Sending HTTP response to connection %lu (%zu bytes)...", 
              safe_conn->connection_id, response_len);
    ssize_t bytes_sent = client_connection_write(safe_conn, response_str, response_len);
    
    /* Free the serialized response string */
    free(response_str);
    if (bytes_sent < 0) {
        int error_code;
        char error_msg[256];
        if (client_connection_get_error(safe_conn, &error_code, error_msg, sizeof(error_msg))) {
            LOG_WARNING("TRACE_HANDLER_ERROR: Connection %lu write error: %s", safe_conn->connection_id, error_msg);
        } else {
            LOG_WARNING("TRACE_HANDLER_ERROR: Connection %lu write failed: %s", safe_conn->connection_id, strerror(errno));
        }
    } else {
        LOG_INFO("TRACE_HANDLER_SEND: Connection %lu sent %zd bytes successfully", safe_conn->connection_id, bytes_sent);
        
        /* Update request counter */
        WITH_CONNECTION_LOCK(safe_conn, {
            safe_conn->requests_handled++;
        });
        LOG_TRACE("TRACE_HANDLER_STATS: Connection %lu request counter updated", safe_conn->connection_id);
    }
    
    /* Cleanup */
    LOG_TRACE("TRACE_HANDLER_CLEANUP: Cleaning up resources for connection %lu...", safe_conn->connection_id);
    
    /* Check if we should keep the connection alive */
    int should_keep_alive = request->keep_alive;
    
    free_http_request(request);
    free_http_response(response);
    
    if (should_keep_alive) {
        /* Keep connection alive - set to ACTIVE state for next request */
        LOG_TRACE("TRACE_HANDLER_KEEPALIVE: Connection %lu requested keep-alive, continuing to next request", safe_conn->connection_id);
        client_connection_set_state(safe_conn, CONN_STATE_ACTIVE);
        
        /* Release reference and continue the loop to handle next request */
        LOG_TRACE("TRACE_HANDLER_RELEASE: Releasing reference to connection %lu...", safe_conn->connection_id);
        client_connection_release(safe_conn);
        
        /* Loop back to handle the next request on this connection */
        LOG_INFO("TRACE_HANDLER_CONTINUE: Continuing with keep-alive connection %lu", safe_conn->connection_id);
        
        /* Restart the handler for the same connection */
        handle_client_thread_safe(client_data);
        return;
    } else {
        /* Mark connection as closing */
        LOG_TRACE("TRACE_HANDLER_STATE: Setting connection %lu to CLOSING state...", safe_conn->connection_id);
        client_connection_set_state(safe_conn, CONN_STATE_CLOSING);
    }
    
    /* Release our reference */
    LOG_TRACE("TRACE_HANDLER_RELEASE: Releasing reference to connection %lu...", safe_conn->connection_id);
    client_connection_release(safe_conn);
    
    LOG_INFO("TRACE_HANDLER_COMPLETE: Thread-safe handler completed successfully for connection %lu", safe_conn->connection_id);
    
cleanup:
    /* Calculate execution time */
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double execution_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0;
    execution_time += (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;
    
    /* Decrement active connections */
    if (active_connections) {
        metrics_gauge_dec(active_connections, 1.0);
    }
    
    /* Stop request timer */
    if (request_timer) {
        metrics_timer_stop(request_timer);
    }
    
    LOG_DEBUG("Thread-safe handler execution time: %.2f ms", execution_time);
}

/**
 * Enhanced server accept loop with thread-safe connection management (for direct call)
 * WITH COMPREHENSIVE TRACING FOR DEBUGGING
 */
void server_accept_loop_thread_safe_direct(server_config_t *config) {
    LOG_INFO("TRACE_ACCEPT_START: Entering thread-safe accept loop function");
    
    if (!config) {
        LOG_ERROR("TRACE_ACCEPT_ERROR: Null configuration passed to accept loop");
        return;
    }
    
    LOG_INFO("TRACE_ACCEPT_CONFIG: socket_fd=%d, max_connections=%d, use_ssl=%d, api_ctx=%p, thread_pool=%p", 
             config->socket_fd, config->max_connections, config->use_ssl, 
             (void*)config->api_ctx, (void*)config->thread_pool);
    
    /* Validate essential components before starting */
    if (config->socket_fd <= 0) {
        LOG_ERROR("TRACE_ACCEPT_ERROR: Invalid socket_fd=%d", config->socket_fd);
        return;
    }
    
    if (!config->api_ctx) {
        LOG_ERROR("TRACE_ACCEPT_ERROR: Missing API context");
        return;
    }
    
    if (!config->thread_pool) {
        LOG_ERROR("TRACE_ACCEPT_ERROR: Missing thread pool");
        return;
    }
    
    LOG_INFO("TRACE_ACCEPT_VALIDATION: All essential components validated");
    
    /* Initialize connection manager */
    LOG_INFO("TRACE_ACCEPT_INIT: Initializing connection manager...");
    
    int max_connections = config->max_connections ? config->max_connections : 1000;
    LOG_INFO("TRACE_ACCEPT_INIT: Connection manager params: max_conn=%d, req_buf=16KB, resp_buf=64KB, timeout=300s", max_connections);
    
    if (connection_manager_init(
        max_connections,  /* max connections */
        16384,           /* max request buffer (16KB) */
        65536,           /* max response buffer (64KB) */
        300              /* connection timeout (5 minutes) */
    ) != 0) {
        LOG_ERROR("TRACE_ACCEPT_ERROR: Failed to initialize connection manager");
        return;
    }
    
    LOG_INFO("TRACE_ACCEPT_INIT: Connection manager initialized successfully");
    
    /* Verify socket state before starting loop */
    int acceptconn = 0;
    socklen_t acceptconn_len = sizeof(acceptconn);
    if (getsockopt(config->socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &acceptconn, &acceptconn_len) < 0) {
        LOG_ERROR("TRACE_ACCEPT_ERROR: Failed to check socket listen state: %s", strerror(errno));
        connection_manager_shutdown();
        return;
    }
    
    if (!acceptconn) {
        LOG_ERROR("TRACE_ACCEPT_ERROR: Socket %d is not in listening state", config->socket_fd);
        connection_manager_shutdown();
        return;
    }
    
    LOG_INFO("TRACE_ACCEPT_SOCKET: Socket %d verified in listening state", config->socket_fd);
    
    /* Get and log bound address */
    struct sockaddr_in bound_addr;
    socklen_t bound_len = sizeof(bound_addr);
    if (getsockname(config->socket_fd, (struct sockaddr*)&bound_addr, &bound_len) == 0) {
        LOG_INFO("TRACE_ACCEPT_SOCKET: Socket bound to %s:%d", 
                 inet_ntoa(bound_addr.sin_addr), ntohs(bound_addr.sin_port));
    }
    
    LOG_INFO("TRACE_ACCEPT_START_LOOP: Thread-safe server accept loop starting on fd %d", config->socket_fd);
    
    int connection_count = 0;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    
    while (1) {
        LOG_INFO("TRACE_ACCEPT_LOOP_ITERATION: Starting iteration %d, calling accept() on fd %d...", 
                 connection_count + 1, config->socket_fd);
        
        /* Accept new connection */
        int client_fd = accept(config->socket_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        
        LOG_INFO("TRACE_ACCEPT_RESULT: accept() returned fd=%d, errno=%d (%s)", 
                 client_fd, errno, client_fd < 0 ? strerror(errno) : "success");
        
        if (client_fd < 0) {
            if (errno == EINTR) {
                LOG_TRACE("TRACE_ACCEPT_INTERRUPTED: accept() interrupted by signal, continuing");
                continue; /* Interrupted by signal, try again */
            }
            
            LOG_ERROR("TRACE_ACCEPT_ERROR: Accept failed: %s (errno=%d)", strerror(errno), errno);
            
            /* Log additional socket diagnostics */
            int socket_error = 0;
            socklen_t error_len = sizeof(socket_error);
            if (getsockopt(config->socket_fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_len) == 0) {
                LOG_ERROR("TRACE_ACCEPT_ERROR: Socket error state: %d (%s)", 
                          socket_error, socket_error ? strerror(socket_error) : "no error");
            }
            
            break;
        }
        
        connection_count++;
        
        LOG_INFO("TRACE_ACCEPT_SUCCESS: Connection #%d accepted, client_fd=%d, from %s:%d", 
                 connection_count, client_fd, 
                 inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        /* Create thread-safe connection */
        LOG_TRACE("TRACE_ACCEPT_CREATE_CONN: Creating client connection structure...");
        
        client_connection_t *conn = client_connection_create(
            client_fd,
            &client_addr,
            config->api_ctx,
            config->use_ssl
        );
        
        if (!conn) {
            LOG_ERROR("TRACE_ACCEPT_ERROR: Failed to create connection structure for fd %d", client_fd);
            close(client_fd);
            LOG_TRACE("TRACE_ACCEPT_CLEANUP: Closed client_fd %d due to connection creation failure", client_fd);
            continue;
        }
        
        LOG_INFO("TRACE_ACCEPT_CONN_CREATED: Connection %lu created successfully for fd %d", 
                 conn->connection_id, client_fd);
        
        /* Add to thread pool */
        LOG_TRACE("TRACE_ACCEPT_THREAD_POOL: Adding connection %lu to thread pool...", conn->connection_id);
        
        if (thread_pool_add_work(config->thread_pool, handle_client_thread_safe, conn) != 0) {
            LOG_ERROR("TRACE_ACCEPT_ERROR: Failed to add connection %lu to thread pool", conn->connection_id);
            client_connection_set_state(conn, CONN_STATE_ERROR);
            client_connection_release(conn); /* Release our reference */
            LOG_TRACE("TRACE_ACCEPT_CLEANUP: Released connection %lu due to thread pool failure", conn->connection_id);
            continue;
        }
        
        LOG_TRACE("TRACE_ACCEPT_THREAD_POOL: Connection %lu successfully added to thread pool", conn->connection_id);
        
        /* Periodic cleanup and statistics */
        if (connection_count % 100 == 0) { /* Less frequent cleanup to avoid deadlock */
            LOG_DEBUG("TRACE_ACCEPT_PERIODIC: Processing connection #%d, logging statistics...", connection_count);
            
            /* Only log statistics, don't do cleanup in accept loop to avoid deadlock */
            size_t active_count;
            uint64_t total_created;
            connection_manager_get_stats(&active_count, &total_created);
            
            LOG_DEBUG("TRACE_ACCEPT_STATS: active=%zu, total_created=%lu, current_count=%d", 
                     active_count, total_created, connection_count);
            
            /* Note: Cleanup is handled by worker threads or a separate cleanup thread */
        }
        
        LOG_TRACE("TRACE_ACCEPT_LOOP_END: Completed processing connection #%d, continuing to next iteration", connection_count);
    }
    
    LOG_INFO("TRACE_ACCEPT_TERMINATED: Accept loop terminated after handling %d connections", connection_count);
    
    /* Shutdown connection manager */
    LOG_INFO("TRACE_ACCEPT_SHUTDOWN: Shutting down connection manager...");
    connection_manager_shutdown();
    LOG_INFO("TRACE_ACCEPT_SHUTDOWN: Connection manager shutdown complete");
    
    LOG_INFO("TRACE_ACCEPT_EXIT: Exiting thread-safe accept loop function");
}

/**
 * Initialize thread-safe server components
 */
int server_init_thread_safe(server_config_t *config) {
    if (!config) {
        LOG_ERROR("Null configuration for thread-safe server init");
        return -1;
    }
    
    /* Initialize any additional thread-safe components here */
    LOG_INFO("Thread-safe server components initialized");
    
    
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
    LOG_INFO("Thread-safe mode enabled - using enhanced connection management");
}

/**
 * Check if thread-safe mode is enabled
 */
int server_is_thread_safe_mode_enabled(void) {
    return g_thread_safe_mode_enabled;
}