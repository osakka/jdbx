/**
 * High-performance epoll-based server for JSONdb
 * 
 * Phase 3 optimization: Event-driven I/O with epoll()
 */

#ifndef EPOLL_SERVER_H
#define EPOLL_SERVER_H

#include "core/server.h"
#include "api/api.h"

/**
 * Initialize and run epoll-based server
 * 
 * @param config Server configuration
 * @param api_ctx API context  
 * @return Server status
 */
server_status_t epoll_server_run(server_config_t* config, api_context_t* api_ctx);

/**
 * Request shutdown of epoll server
 */
void epoll_server_shutdown(void);

#endif /* EPOLL_SERVER_H */