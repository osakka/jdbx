/**
 * High-performance epoll-based server implementation for JDBX
 * 
 * This implements Phase 3 optimization: Event-driven I/O with epoll()
 * to achieve 10x connection scalability improvement.
 */

#define _GNU_SOURCE /* For accept4 */
#include "core/server.h"
#include "api/api.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include <sys/epoll.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

/* epoll configuration */
#define MAX_EPOLL_EVENTS 1024
#define EPOLL_TIMEOUT_MS 1000
#define MAX_EPOLL_CONNECTIONS 10000
#define CONNECTION_TIMEOUT_SEC 30

/* Connection states for event-driven processing */
typedef enum {
    CONN_ACCEPTING,     /* New connection being accepted */
    CONN_READING,       /* Reading HTTP request data */
    CONN_PROCESSING,    /* Processing request (may involve blocking operations) */
    CONN_WRITING,       /* Writing HTTP response data */
    CONN_CLOSING        /* Connection being closed */
} conn_state_t;

/* Event-driven connection structure */
typedef struct epoll_connection {
    int fd;
    conn_state_t state;
    
    /* Timing */
    time_t last_activity;
    
    /* Buffers for incremental I/O */
    char* read_buffer;
    size_t read_buffer_size;
    size_t read_buffer_used;
    
    char* write_buffer;
    size_t write_buffer_size;
    size_t write_buffer_sent;
    
    /* HTTP processing */
    struct sockaddr_in client_addr;
    api_context_t* api_ctx;
    
    /* Linked list for connection management */
    struct epoll_connection* next;
} epoll_connection_t;

/* Global epoll server state */
static struct {
    int epoll_fd;
    int server_fd;
    epoll_connection_t* connections;
    size_t connection_count;
    api_context_t* api_ctx;
    volatile int shutdown_requested;
} g_epoll_server = {0};

/* Forward declarations */
static int setup_epoll_server(server_config_t* config);
static int handle_accept_events(void);
static int handle_client_events(epoll_connection_t* conn, uint32_t events);
static epoll_connection_t* create_connection(int client_fd, struct sockaddr_in* client_addr);
static void destroy_connection(epoll_connection_t* conn);
static int process_read_event(epoll_connection_t* conn);
static int process_write_event(epoll_connection_t* conn);
static int set_nonblocking(int fd);
static void cleanup_expired_connections(void);

/**
 * Initialize and run epoll-based server
 * 
 * @param config Server configuration
 * @param api_ctx API context  
 * @return Server status
 */
server_status_t epoll_server_run(server_config_t* config, api_context_t* api_ctx) {
    if (!config || !api_ctx) {
        if (g_logger) {
            LOG_ERROR("Invalid configuration or API context for epoll server.");
        }
        return SERVER_ERROR;
    }
    
    if (g_logger) {
        LOG_INFO("Starting high-performance epoll server on %s:%d", 
                 config->host ? config->host : "0.0.0.0", config->port);
    }
    
    /* Initialize epoll server state */
    g_epoll_server.api_ctx = api_ctx;
    g_epoll_server.server_fd = config->socket_fd;
    g_epoll_server.shutdown_requested = 0;
    
    /* Set up epoll instance */
    if (setup_epoll_server(config) != 0) {
        return SERVER_ERROR;
    }
    
    /* Main event loop */
    struct epoll_event events[MAX_EPOLL_EVENTS];
    time_t last_cleanup = time(NULL);
    
    if (g_logger) {
        LOG_INFO("epoll event loop started (max_events=%d, timeout=%dms)", 
                 MAX_EPOLL_EVENTS, EPOLL_TIMEOUT_MS);
    }
    
    while (!g_epoll_server.shutdown_requested) {
        /* Wait for events */
        int nfds = epoll_wait(g_epoll_server.epoll_fd, events, MAX_EPOLL_EVENTS, EPOLL_TIMEOUT_MS);
        
        if (nfds == -1) {
            if (errno == EINTR) {
                continue; /* Interrupted by signal */
            }
            if (g_logger) {
                LOG_ERROR("epoll_wait failed: %s", strerror(errno));
            }
            break;
        }
        
        /* Process events */
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == g_epoll_server.server_fd) {
                /* Handle new connections */
                if (handle_accept_events() != 0) {
                    if (g_logger) {
                        LOG_WARNING("Error handling accept events.");
                    }
                }
            } else {
                /* Handle client I/O events */
                epoll_connection_t* conn = (epoll_connection_t*)events[i].data.ptr;
                if (conn && handle_client_events(conn, events[i].events) != 0) {
                    /* Connection error - will be cleaned up */
                }
            }
        }
        
        /* Periodic cleanup of expired connections */
        time_t now = time(NULL);
        if (now - last_cleanup > 10) {
            cleanup_expired_connections();
            last_cleanup = now;
        }
    }
    
    /* Cleanup */
    if (g_epoll_server.epoll_fd >= 0) {
        close(g_epoll_server.epoll_fd);
    }
    
    /* Clean up all connections */
    epoll_connection_t* conn = g_epoll_server.connections;
    while (conn) {
        epoll_connection_t* next = conn->next;
        destroy_connection(conn);
        conn = next;
    }
    
    if (g_logger) {
        LOG_INFO("epoll server shutdown complete.");
    }
    
    return SERVER_OK;
}

/**
 * Set up epoll instance and add server socket
 */
static int setup_epoll_server(server_config_t* config __attribute__((unused))) {
    /* Create epoll instance */
    g_epoll_server.epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (g_epoll_server.epoll_fd == -1) {
        if (g_logger) {
            LOG_ERROR("Cannot create epoll instance: %s", strerror(errno));
        }
        return -1;
    }
    
    /* Set server socket to non-blocking */
    if (set_nonblocking(g_epoll_server.server_fd) != 0) {
        return -1;
    }
    
    /* Add server socket to epoll with edge-triggered events */
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET; /* Edge-triggered for accept */
    ev.data.fd = g_epoll_server.server_fd;
    
    if (epoll_ctl(g_epoll_server.epoll_fd, EPOLL_CTL_ADD, g_epoll_server.server_fd, &ev) == -1) {
        if (g_logger) {
            LOG_ERROR("Cannot add server socket to epoll: %s", strerror(errno));
        }
        close(g_epoll_server.epoll_fd);
        return -1;
    }
    
    if (g_logger) {
        LOG_INFO("epoll server setup complete (fd=%d, server_fd=%d)", 
                 g_epoll_server.epoll_fd, g_epoll_server.server_fd);
    }
    
    return 0;
}

/**
 * Handle accept events on server socket
 */
static int handle_accept_events(void) {
    int accepted = 0;
    
    /* Accept all pending connections (edge-triggered) */
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept4(g_epoll_server.server_fd, 
                               (struct sockaddr*)&client_addr, 
                               &client_len, 
                               SOCK_NONBLOCK | SOCK_CLOEXEC);
        
        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* No more connections to accept */
                break;
            }
            if (g_logger) {
                LOG_WARNING("accept4 failed: %s", strerror(errno));
            }
            return -1;
        }
        
        /* Check connection limit */
        if (g_epoll_server.connection_count >= MAX_EPOLL_CONNECTIONS) {
            if (g_logger) {
                LOG_WARNING("Connection limit reached (%zu), rejecting connection", 
                           g_epoll_server.connection_count);
            }
            close(client_fd);
            continue;
        }
        
        /* Create connection structure */
        epoll_connection_t* conn = create_connection(client_fd, &client_addr);
        if (!conn) {
            close(client_fd);
            continue;
        }
        
        /* Add to epoll */
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET; /* Edge-triggered read */
        ev.data.ptr = conn;
        
        if (epoll_ctl(g_epoll_server.epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
            if (g_logger) {
                LOG_ERROR("Cannot add client socket to epoll: %s", strerror(errno));
            }
            destroy_connection(conn);
            continue;
        }
        
        accepted++;
        
        if (g_logger) {
            LOG_DEBUG("Accepted connection %d (total: %zu)", client_fd, g_epoll_server.connection_count);
        }
    }
    
    if (accepted > 0 && g_logger) {
        LOG_DEBUG("Accepted %d new connections", accepted);
    }
    
    return 0;
}

/**
 * Handle I/O events on client connections
 */
static int handle_client_events(epoll_connection_t* conn, uint32_t events) {
    if (!conn) return -1;
    
    conn->last_activity = time(NULL);
    
    /* Handle error conditions */
    if (events & (EPOLLERR | EPOLLHUP)) {
        if (g_logger) {
            LOG_DEBUG("Connection %d error/hangup (events=0x%x)", conn->fd, events);
        }
        destroy_connection(conn);
        return -1;
    }
    
    /* Handle read events */
    if (events & EPOLLIN) {
        if (process_read_event(conn) != 0) {
            destroy_connection(conn);
            return -1;
        }
    }
    
    /* Handle write events */
    if (events & EPOLLOUT) {
        if (process_write_event(conn) != 0) {
            destroy_connection(conn);
            return -1;
        }
    }
    
    return 0;
}

/**
 * Process read event for connection
 */
static int process_read_event(epoll_connection_t* conn) {
    if (conn->state != CONN_READING) {
        conn->state = CONN_READING;
    }
    
    /* Read data into buffer */
    while (1) {
        /* Ensure buffer space */
        if (conn->read_buffer_used >= conn->read_buffer_size - 1) {
            /* Grow buffer */
            size_t new_size = conn->read_buffer_size * 2;
            char* new_buffer = buffer_pool_realloc(conn->read_buffer, new_size);
            if (!new_buffer) {
                if (g_logger) {
                    LOG_ERROR("Cannot grow read buffer for connection %d", conn->fd);
                }
                return -1;
            }
            conn->read_buffer = new_buffer;
            conn->read_buffer_size = new_size;
        }
        
        ssize_t bytes_read = read(conn->fd, 
                                 conn->read_buffer + conn->read_buffer_used,
                                 conn->read_buffer_size - conn->read_buffer_used - 1);
        
        if (bytes_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* No more data available */
                break;
            }
            if (g_logger) {
                LOG_DEBUG("Read error on connection %d: %s", conn->fd, strerror(errno));
            }
            return -1;
        }
        
        if (bytes_read == 0) {
            /* Connection closed by client */
            if (g_logger) {
                LOG_DEBUG("Connection %d closed by client", conn->fd);
            }
            return -1;
        }
        
        conn->read_buffer_used += bytes_read;
        conn->read_buffer[conn->read_buffer_used] = '\0';
        
        /* Check if we have a complete HTTP request */
        if (strstr(conn->read_buffer, "\r\n\r\n") != NULL) {
            /* Complete request received - process it */
            conn->state = CONN_PROCESSING;
            
            /* For now, just echo back a simple response */
            const char* response = "HTTP/1.1 200 OK\r\n"
                                  "Content-Length: 13\r\n"
                                  "Connection: close\r\n"
                                  "\r\n"
                                  "Hello, epoll!";
            
            /* Allocate write buffer */
            size_t response_len = strlen(response);
            conn->write_buffer = BUFFER_ALLOC(response_len + 1);
            if (!conn->write_buffer) {
                return -1;
            }
            strcpy(conn->write_buffer, response);
            conn->write_buffer_size = response_len;
            conn->write_buffer_sent = 0;
            
            conn->state = CONN_WRITING;
            
            /* Modify epoll to wait for write events */
            struct epoll_event ev;
            ev.events = EPOLLOUT | EPOLLET;
            ev.data.ptr = conn;
            epoll_ctl(g_epoll_server.epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev);
            
            break;
        }
    }
    
    return 0;
}

/**
 * Process write event for connection
 */
static int process_write_event(epoll_connection_t* conn) {
    if (conn->state != CONN_WRITING || !conn->write_buffer) {
        return -1;
    }
    
    /* Write remaining data */
    while (conn->write_buffer_sent < conn->write_buffer_size) {
        ssize_t bytes_written = write(conn->fd,
                                     conn->write_buffer + conn->write_buffer_sent,
                                     conn->write_buffer_size - conn->write_buffer_sent);
        
        if (bytes_written == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* Would block - try again later */
                return 0;
            }
            if (g_logger) {
                LOG_DEBUG("Write error on connection %d: %s", conn->fd, strerror(errno));
            }
            return -1;
        }
        
        conn->write_buffer_sent += bytes_written;
    }
    
    /* Response fully sent - close connection */
    conn->state = CONN_CLOSING;
    return -1; /* Signal to destroy connection */
}

/**
 * Create new connection structure
 */
static epoll_connection_t* create_connection(int client_fd, struct sockaddr_in* client_addr) {
    epoll_connection_t* conn = BUFFER_ALLOC(sizeof(epoll_connection_t));
    if (!conn) {
        return NULL;
    }
    
    memset(conn, 0, sizeof(epoll_connection_t));
    conn->fd = client_fd;
    conn->state = CONN_READING;
    conn->last_activity = time(NULL);
    conn->client_addr = *client_addr;
    conn->api_ctx = g_epoll_server.api_ctx;
    
    /* Allocate initial read buffer */
    conn->read_buffer_size = 4096;
    conn->read_buffer = BUFFER_ALLOC(conn->read_buffer_size);
    if (!conn->read_buffer) {
        buffer_pool_free(conn);
        return NULL;
    }
    
    /* Add to connection list */
    conn->next = g_epoll_server.connections;
    g_epoll_server.connections = conn;
    g_epoll_server.connection_count++;
    
    return conn;
}

/**
 * Destroy connection and free resources
 */
static void destroy_connection(epoll_connection_t* conn) {
    if (!conn) return;
    
    /* Remove from epoll */
    epoll_ctl(g_epoll_server.epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
    
    /* Close socket */
    close(conn->fd);
    
    /* Free buffers */
    if (conn->read_buffer) {
        buffer_pool_free(conn->read_buffer);
    }
    if (conn->write_buffer) {
        buffer_pool_free(conn->write_buffer);
    }
    
    /* Remove from connection list */
    if (g_epoll_server.connections == conn) {
        g_epoll_server.connections = conn->next;
    } else {
        epoll_connection_t* prev = g_epoll_server.connections;
        while (prev && prev->next != conn) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = conn->next;
        }
    }
    
    g_epoll_server.connection_count--;
    buffer_pool_free(conn);
    
    if (g_logger) {
        LOG_DEBUG("Destroyed connection %d (remaining: %zu)", conn->fd, g_epoll_server.connection_count);
    }
}

/**
 * Set socket to non-blocking mode
 */
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        if (g_logger) {
            LOG_ERROR("fcntl F_GETFL failed: %s", strerror(errno));
        }
        return -1;
    }
    
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        if (g_logger) {
            LOG_ERROR("fcntl F_SETFL failed: %s", strerror(errno));
        }
        return -1;
    }
    
    return 0;
}

/**
 * Clean up expired connections
 */
static void cleanup_expired_connections(void) {
    time_t now = time(NULL);
    epoll_connection_t* conn = g_epoll_server.connections;
    
    while (conn) {
        epoll_connection_t* next = conn->next;
        
        if (now - conn->last_activity > CONNECTION_TIMEOUT_SEC) {
            if (g_logger) {
                LOG_DEBUG("Connection %d timed out", conn->fd);
            }
            destroy_connection(conn);
        }
        
        conn = next;
    }
}

/**
 * Request shutdown of epoll server
 */
void epoll_server_shutdown(void) {
    g_epoll_server.shutdown_requested = 1;
}