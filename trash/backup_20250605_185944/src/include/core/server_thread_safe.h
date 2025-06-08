#ifndef SERVER_THREAD_SAFE_H
#define SERVER_THREAD_SAFE_H

#include "core/server.h"
#include "core/client_connection.h"

/**
 * Thread-safe server initialization and management
 */

/**
 * Enhanced thread-safe client handler
 */
void handle_client_thread_safe(void* client_data);

/**
 * Thread-safe server accept loop (for pthread)
 */
void* server_accept_loop_thread_safe(void* arg);

/**
 * Thread-safe server accept loop (for direct call)
 */
void server_accept_loop_thread_safe_direct(server_config_t *config);

/**
 * Initialize thread-safe server components
 */
int server_init_thread_safe(server_config_t *config);

/**
 * Get comprehensive connection statistics
 */
void server_get_connection_stats(size_t *active_connections, uint64_t *total_connections, 
                                int *thread_pool_active, int *thread_pool_queue_size);

/**
 * Enable thread-safe mode - call this before server initialization
 */
void server_enable_thread_safe_mode(void);

/**
 * Check if thread-safe mode is enabled
 */
int server_is_thread_safe_mode_enabled(void);

#endif /* SERVER_THREAD_SAFE_H */