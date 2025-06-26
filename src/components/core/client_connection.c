/**
 * @file client_connection.c
 * @brief Client connection lifecycle management and resource tracking
 * 
 * Implements connection management for JDBX server, handling client connection
 * creation, tracking, timeout management, and resource cleanup. Provides
 * centralized connection state management with thread-safe operations.
 * 
 * Key Features:
 * - Connection pooling and reuse
 * - Automatic timeout detection and cleanup
 * - Thread-safe connection state management
 * - Resource tracking and leak prevention
 * - Statistics collection for monitoring
 */

#include "core/client_connection.h"
#include "utils/logger.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <poll.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <arpa/inet.h>
#include "utils/buffer_pool.h"

/**
 * Global connection manager instance
 * 
 * Singleton connection manager providing centralized client connection
 * tracking and lifecycle management. Initialized during server startup
 * and accessed by request handlers for connection state management.
 * 
 * Thread Safety: Protected by internal mutexes for concurrent access
 * Lifecycle: Created during server init, destroyed during shutdown
 */
static connection_manager_t *g_connection_manager = NULL;

/* === INTERNAL HELPER FUNCTIONS === */

/**
 * Get current monotonic timestamp
 * 
 * Retrieves the current monotonic time, unaffected by system clock
 * adjustments. Used for accurate timeout calculations and connection
 * lifecycle management.
 * 
 * @param ts Timespec structure to populate with current time
 */
static void get_current_time(struct timespec *ts) {
    clock_gettime(CLOCK_MONOTONIC, ts);
}

/**
 * Check if connection has exceeded timeout threshold
 * 
 * Determines whether a client connection has been inactive beyond
 * the configured timeout period. Used for connection cleanup and
 * resource management.
 * 
 * @param conn Client connection to check (must not be NULL)
 * @param timeout_seconds Maximum allowed inactive time in seconds
 * @return 1 if connection expired, 0 if still valid
 */
static int is_connection_expired(client_connection_t *conn, uint32_t timeout_seconds) {
    if (!conn) return 1;
    
    struct timespec now, diff;
    get_current_time(&now);
    
    WITH_CONNECTION_LOCK(conn, {
        diff.tv_sec = now.tv_sec - conn->last_activity.tv_sec;
        diff.tv_nsec = now.tv_nsec - conn->last_activity.tv_nsec;
        if (diff.tv_nsec < 0) {
            diff.tv_sec--;
            diff.tv_nsec += 1000000000;
        }
    });
    
    return diff.tv_sec > timeout_seconds;
}

/**
 * Internal connection destruction (called when ref_count reaches 0)
 */
static void client_connection_destroy_internal(client_connection_t *conn) {
    if (!conn) return;
    
    LOG_DEBUG("Destroying connection %lu", conn->connection_id);
    
    /* Mark as being destroyed */
    atomic_store(&conn->is_destroyed, true);
    atomic_store(&conn->cleanup_in_progress, true);
    
    /* Close socket if still open */
    WITH_CONNECTION_LOCK(conn, {
        if (conn->socket_fd >= 0) {
            close(conn->socket_fd);
            conn->socket_fd = -1;
        }
        
        /* Cleanup SSL */
        if (conn->ssl_conn) {
            // ssl_connection_cleanup(conn->ssl_conn); // Implement as needed
            conn->ssl_conn = NULL;
        }
        
        /* Free buffers */
        if (conn->request_buffer) {
            BUFFER_FREE(conn->request_buffer);
            conn->request_buffer = NULL;
        }
        
        if (conn->response_buffer) {
            BUFFER_FREE(conn->response_buffer);
            conn->response_buffer = NULL;
        }
        
        conn->state = CONN_STATE_CLOSED;
    });
    
    /* Signal cleanup complete */
    pthread_cond_broadcast(&conn->cleanup_complete);
    
    /* Destroy synchronization objects */
    pthread_mutex_destroy(&conn->state_mutex);
    pthread_cond_destroy(&conn->cleanup_complete);
    
    /* Free the connection structure */
    memset(conn, 0, sizeof(client_connection_t)); /* Zero for safety */
    BUFFER_FREE(conn);
    
    TRACE_NET("Connection destruction complete.");
}

/* === PUBLIC CONNECTION LIFECYCLE FUNCTIONS === */

client_connection_t* client_connection_create(
    int socket_fd,
    struct sockaddr_in *client_addr,
    struct api_context *api_ctx,
    int use_ssl
) {
    TRACE_NET("TRACE_CONN_CREATE: Starting connection creation for fd=%d, use_ssl=%d", socket_fd, use_ssl);
    
    if (socket_fd < 0 || !client_addr || !api_ctx) {
        LOG_ERROR("Invalid parameters for connection creation: fd=%d, addr=%p, api=%p",
                  socket_fd, (void*)client_addr, (void*)api_ctx);
        return NULL;
    }
    
    TRACE_NET("TRACE_CONN_CREATE: Client address: %s:%d", 
              inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
    
    /* Allocate connection structure */
    TRACE_NET("TRACE_CONN_CREATE: Allocating connection structure...");
    client_connection_t *conn =BUFFER_CALLOC(1, sizeof(client_connection_t));
    if (!conn) {
        LOG_ERROR("Failed to allocate memory for client connection");
        return NULL;
    }
    
    TRACE_NET("TRACE_CONN_CREATE: Connection allocated at %p", (void*)conn);
    
    /* Initialize immutable fields */
    if (g_connection_manager) {
        conn->connection_id = atomic_fetch_add(&g_connection_manager->next_connection_id, 1);
        TRACE_NET("TRACE_CONN_CREATE: Assigned connection_id=%lu from manager", conn->connection_id);
    } else {
        conn->connection_id = (uint64_t)time(NULL) * 1000000 + (socket_fd % 1000000);
        TRACE_NET("TRACE_CONN_CREATE: Generated connection_id=%lu (no manager)", conn->connection_id);
    }
    
    conn->client_address = *client_addr;
    conn->api_ctx = api_ctx;
    conn->use_ssl = use_ssl;
    get_current_time(&conn->created_at);
    
    TRACE_NET("TRACE_CONN_CREATE: Initializing atomic fields...");
    /* Initialize atomic fields */
    atomic_init(&conn->ref_count, 1); /* Start with 1 reference */
    atomic_init(&conn->cleanup_in_progress, false);
    atomic_init(&conn->is_destroyed, false);
    
    TRACE_NET("TRACE_CONN_CREATE: Initializing mutex...");
    /* Initialize mutex and condition variable */
    if (pthread_mutex_init(&conn->state_mutex, NULL) != 0) {
        LOG_ERROR("Failed to initialize connection mutex: %s", strerror(errno));
        BUFFER_FREE(conn);
        return NULL;
    }
    
    TRACE_NET("TRACE_CONN_CREATE: Initializing condition variable...");
    if (pthread_cond_init(&conn->cleanup_complete, NULL) != 0) {
        LOG_ERROR("Failed to initialize connection condition variable: %s", strerror(errno));
        pthread_mutex_destroy(&conn->state_mutex);
        BUFFER_FREE(conn);
        return NULL;
    }
    
    TRACE_NET("TRACE_CONN_CREATE: Initializing mutable state with lock...");
    /* Initialize mutable state (protected by mutex) */
    WITH_CONNECTION_LOCK(conn, {
        conn->state = CONN_STATE_INITIALIZING;
        conn->socket_fd = socket_fd;
        conn->ssl_conn = NULL;
        
        /* Initialize buffers if manager provides limits */
        if (g_connection_manager) {
            conn->request_buffer_size = g_connection_manager->max_request_buffer_size;
            conn->response_buffer_size = g_connection_manager->max_response_buffer_size;
            TRACE_NET("TRACE_CONN_CREATE: Using manager buffer sizes: req=%zu, resp=%zu", 
                      conn->request_buffer_size, conn->response_buffer_size);
        } else {
            conn->request_buffer_size = 8192;  /* Default 8KB */
            conn->response_buffer_size = 65536; /* Default 64KB */
            TRACE_NET("TRACE_CONN_CREATE: Using default buffer sizes: req=%zu, resp=%zu", 
                      conn->request_buffer_size, conn->response_buffer_size);
        }
        
        TRACE_NET("TRACE_CONN_CREATE: Allocating buffers...");
        conn->request_buffer =BUFFER_ALLOC(conn->request_buffer_size);
        conn->response_buffer =BUFFER_ALLOC(conn->response_buffer_size);
        
        if (!conn->request_buffer || !conn->response_buffer) {
            LOG_ERROR("Failed to allocate connection buffers");
            if (conn->request_buffer) BUFFER_FREE(conn->request_buffer);
            if (conn->response_buffer) BUFFER_FREE(conn->response_buffer);
            pthread_mutex_destroy(&conn->state_mutex);
            pthread_cond_destroy(&conn->cleanup_complete);
            BUFFER_FREE(conn);
            return NULL;
        }
        
        TRACE_NET("TRACE_CONN_CREATE: Buffers allocated: req=%p, resp=%p", 
                  (void*)conn->request_buffer, (void*)conn->response_buffer);
        
        conn->request_bytes_received = 0;
        conn->response_bytes_sent = 0;
        conn->last_activity = conn->created_at;
        conn->requests_handled = 0;
        conn->error_code = 0;
        conn->error_message[0] = '\0';
        
        /* Transition to active state */
        conn->state = CONN_STATE_ACTIVE;
        TRACE_NET("TRACE_CONN_CREATE: Connection state set to ACTIVE.");
    });
    
    /* Register with connection manager if available */
    if (g_connection_manager) {
        TRACE_NET("TRACE_CONN_CREATE: Registering connection with manager...");
        if (connection_manager_register(conn) != 0) {
            LOG_ERROR("Failed to register connection with manager");
            /* Continue anyway - connection is still usable */
        } else {
            TRACE_NET("TRACE_CONN_CREATE: Connection registered with manager successfully.");
        }
    }
    
    TRACE_NET("Created connection %lu for fd %d successfully", conn->connection_id, socket_fd);
    return conn;
}

client_connection_t* client_connection_acquire(client_connection_t *conn) {
    if (!conn) return NULL;
    
    /* Check if connection is being destroyed */
    if (atomic_load(&conn->is_destroyed) || atomic_load(&conn->cleanup_in_progress)) {
        return NULL;
    }
    
    /* Atomically increment reference count */
    int old_count = atomic_fetch_add(&conn->ref_count, 1);
    
    /* Double-check destruction state after incrementing */
    if (atomic_load(&conn->is_destroyed)) {
        /* Undo the reference increment */
        atomic_fetch_sub(&conn->ref_count, 1);
        return NULL;
    }
    
    if (old_count <= 0) {
        /* Connection was already being destroyed */
        atomic_fetch_sub(&conn->ref_count, 1);
        return NULL;
    }
    
    return conn;
}

void client_connection_release(client_connection_t *conn) {
    if (!conn) return;
    
    /* Atomically decrement reference count */
    int new_count = atomic_fetch_sub(&conn->ref_count, 1) - 1;
    
    if (new_count == 0) {
        /* Last reference - destroy the connection */
        if (g_connection_manager) {
            connection_manager_unregister(conn);
        }
        client_connection_destroy_internal(conn);
    } else if (new_count < 0) {
        /* This should never happen - indicates a bug */
        LOG_ERROR("Connection reference count went negative: %d", new_count);
    }
}

/* === CONNECTION STATE MANAGEMENT === */

int client_connection_set_state(client_connection_t *conn, connection_state_t new_state) {
    if (!conn) return -1;
    
    /* Check if connection is valid for state changes */
    if (atomic_load(&conn->is_destroyed)) {
        return -1;
    }
    
    int result = 0;
    WITH_CONNECTION_LOCK(conn, {
        connection_state_t old_state = conn->state;
        
        /* Validate state transition */
        switch (old_state) {
            case CONN_STATE_IDLE:
                if (new_state != CONN_STATE_ACTIVE && new_state != CONN_STATE_ERROR && new_state != CONN_STATE_CLOSING) {
                    result = -1;
                }
                break;
            case CONN_STATE_INITIALIZING:
                if (new_state != CONN_STATE_ACTIVE && new_state != CONN_STATE_ERROR && new_state != CONN_STATE_CLOSING) {
                    result = -1;
                }
                break;
            case CONN_STATE_ACTIVE:
                /* Can transition to any state from active */
                break;
            case CONN_STATE_READING:
                if (new_state == CONN_STATE_INITIALIZING) {
                    result = -1;
                }
                break;
            case CONN_STATE_PROCESSING:
                if (new_state == CONN_STATE_INITIALIZING || new_state == CONN_STATE_READING) {
                    result = -1;
                }
                break;
            case CONN_STATE_WRITING:
                if (new_state == CONN_STATE_INITIALIZING || new_state == CONN_STATE_READING || new_state == CONN_STATE_PROCESSING) {
                    result = -1;
                }
                break;
            case CONN_STATE_CLOSING:
            case CONN_STATE_CLOSED:
            case CONN_STATE_ERROR:
                /* Can only transition to closed or stay in current state */
                if (new_state != CONN_STATE_CLOSED && new_state != old_state) {
                    result = -1;
                }
                break;
        }
        
        if (result == 0) {
            conn->state = new_state;
            get_current_time(&conn->last_activity);
            TRACE_NET("Connection %lu state: %d -> %d", conn->connection_id, old_state, new_state);
        }
    });
    
    return result;
}

connection_state_t client_connection_get_state(client_connection_t *conn) {
    if (!conn) return CONN_STATE_ERROR;
    
    connection_state_t state;
    WITH_CONNECTION_LOCK(conn, {
        state = conn->state;
    });
    
    return state;
}

int client_connection_is_active(client_connection_t *conn) {
    if (!conn || atomic_load(&conn->is_destroyed)) {
        return 0;
    }
    
    connection_state_t state = client_connection_get_state(conn);
    return (state >= CONN_STATE_ACTIVE && state <= CONN_STATE_WRITING);
}

/* === I/O OPERATIONS === */

ssize_t client_connection_read(client_connection_t *conn, void *buffer, size_t buffer_size, int timeout_ms) {
    if (!conn || !buffer || buffer_size == 0) {
        errno = EINVAL;
        return -1;
    }
    
    if (!client_connection_is_active(conn)) {
        errno = ENOTCONN;
        return -1;
    }
    
    /* Set reading state */
    if (client_connection_set_state(conn, CONN_STATE_READING) != 0) {
        errno = ENOTCONN;
        return -1;
    }
    
    ssize_t bytes_read = -1;
    int socket_fd = -1;
    
    WITH_CONNECTION_LOCK(conn, {
        socket_fd = conn->socket_fd;
    });
    
    if (socket_fd < 0) {
        client_connection_set_state(conn, CONN_STATE_ERROR);
        errno = EBADF;
        return -1;
    }
    
    /* Validate socket before polling */
    TRACE_NET("TRACE_CONN_READ_POLL: Preparing to poll socket_fd=%d for connection %lu", 
              socket_fd, conn->connection_id);
    
    /* Check if socket is valid using getsockopt */
    int sock_error = 0;
    socklen_t err_len = sizeof(sock_error);
    if (getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &sock_error, &err_len) < 0) {
        LOG_ERROR("getsockopt failed for fd=%d: %s", 
                  socket_fd, strerror(errno));
        client_connection_set_error(conn, errno, "Socket validation failed");
        client_connection_set_state(conn, CONN_STATE_ERROR);
        return -1;
    }
    
    if (sock_error != 0) {
        LOG_ERROR("Socket has pending error for fd=%d: %s", 
                  socket_fd, strerror(sock_error));
        client_connection_set_error(conn, sock_error, strerror(sock_error));
        client_connection_set_state(conn, CONN_STATE_ERROR);
        errno = sock_error;
        return -1;
    }
    
    /* Use poll for timeout */
    struct pollfd pfd = {
        .fd = socket_fd,
        .events = POLLIN,
        .revents = 0
    };
    
    TRACE_NET("TRACE_CONN_READ_POLL: Calling poll() with timeout=%dms on fd=%d", 
              timeout_ms, socket_fd);
    
    int poll_result = poll(&pfd, 1, timeout_ms);
    
    TRACE_NET("TRACE_CONN_READ_POLL: poll() returned %d, revents=0x%x, errno=%d", 
              poll_result, pfd.revents, errno);
    
    if (poll_result < 0) {
        LOG_ERROR("Poll failed: %s (errno=%d)", 
                  strerror(errno), errno);
        client_connection_set_error(conn, errno, "Poll failed during read");
        client_connection_set_state(conn, CONN_STATE_ERROR);
        return -1;
    } else if (poll_result == 0) {
        /* Timeout */
        LOG_DEBUG("Poll timed out after %dms", timeout_ms);
        client_connection_set_state(conn, CONN_STATE_ACTIVE);
        errno = ETIMEDOUT;
        return -1;
    }
    
    /* Check for error conditions */
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        LOG_ERROR("Socket error detected by poll: revents=0x%x (POLLERR=%d, POLLHUP=%d, POLLNVAL=%d)", 
                  pfd.revents, !!(pfd.revents & POLLERR), !!(pfd.revents & POLLHUP), !!(pfd.revents & POLLNVAL));
        client_connection_set_error(conn, ECONNRESET, "Socket error during read");
        client_connection_set_state(conn, CONN_STATE_ERROR);
        errno = ECONNRESET;
        return -1;
    }
    
    TRACE_NET("TRACE_CONN_READ_READY: Socket ready for reading (revents=0x%x)", pfd.revents);
    
    /* Perform the read */
    TRACE_NET("TRACE_CONN_READ_SYSCALL: Calling read() on fd=%d for %zu bytes", 
              socket_fd, buffer_size);
    
    bytes_read = read(socket_fd, buffer, buffer_size);
    
    TRACE_NET("TRACE_CONN_READ_RESULT: read() returned %zd, errno=%d (%s)", 
              bytes_read, errno, bytes_read < 0 ? strerror(errno) : "success");
    
    if (bytes_read < 0) {
        client_connection_set_error(conn, errno, "Read failed");
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            client_connection_set_state(conn, CONN_STATE_ACTIVE);
        } else {
            client_connection_set_state(conn, CONN_STATE_ERROR);
        }
    } else if (bytes_read == 0) {
        /* EOF - client closed connection */
        client_connection_set_state(conn, CONN_STATE_CLOSING);
    } else {
        /* Update statistics */
        WITH_CONNECTION_LOCK(conn, {
            conn->request_bytes_received += bytes_read;
            get_current_time(&conn->last_activity);
        });
        client_connection_set_state(conn, CONN_STATE_ACTIVE);
    }
    
    return bytes_read;
}

ssize_t client_connection_write(client_connection_t *conn, const void *data, size_t data_size) {
    if (!conn || !data || data_size == 0) {
        errno = EINVAL;
        return -1;
    }
    
    if (!client_connection_is_active(conn)) {
        errno = ENOTCONN;
        return -1;
    }
    
    /* Set writing state */
    if (client_connection_set_state(conn, CONN_STATE_WRITING) != 0) {
        errno = ENOTCONN;
        return -1;
    }
    
    ssize_t bytes_written = -1;
    int socket_fd = -1;
    
    WITH_CONNECTION_LOCK(conn, {
        socket_fd = conn->socket_fd;
    });
    
    if (socket_fd < 0) {
        client_connection_set_state(conn, CONN_STATE_ERROR);
        errno = EBADF;
        return -1;
    }
    
    /* Perform the write */
    bytes_written = write(socket_fd, data, data_size);
    
    if (bytes_written < 0) {
        client_connection_set_error(conn, errno, "Write failed");
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            client_connection_set_state(conn, CONN_STATE_ACTIVE);
        } else {
            client_connection_set_state(conn, CONN_STATE_ERROR);
        }
    } else {
        /* Update statistics */
        WITH_CONNECTION_LOCK(conn, {
            conn->response_bytes_sent += bytes_written;
            get_current_time(&conn->last_activity);
        });
        client_connection_set_state(conn, CONN_STATE_ACTIVE);
    }
    
    return bytes_written;
}

/* === ERROR HANDLING === */

void client_connection_set_error(client_connection_t *conn, int error_code, const char *error_message) {
    if (!conn) return;
    
    WITH_CONNECTION_LOCK(conn, {
        conn->error_code = error_code;
        if (error_message) {
            strncpy(conn->error_message, error_message, sizeof(conn->error_message) - 1);
            conn->error_message[sizeof(conn->error_message) - 1] = '\0';
        } else {
            conn->error_message[0] = '\0';
        }
        get_current_time(&conn->last_activity);
    });
    
    LOG_DEBUG("Connection %lu error: %d - %s", conn->connection_id, error_code, error_message ? error_message : "Unknown");
}

int client_connection_get_error(client_connection_t *conn, int *error_code, char *error_message, size_t message_size) {
    if (!conn || !error_code) return 0;
    
    int has_error = 0;
    WITH_CONNECTION_LOCK(conn, {
        if (conn->error_code != 0) {
            *error_code = conn->error_code;
            if (error_message && message_size > 0) {
                strncpy(error_message, conn->error_message, message_size - 1);
                error_message[message_size - 1] = '\0';
            }
            has_error = 1;
        }
    });
    
    return has_error;
}

/* === CONNECTION MANAGER === */

int connection_manager_init(size_t max_connections, size_t max_request_buffer, 
                           size_t max_response_buffer, uint32_t timeout_seconds) {
    LOG_INFO("Starting connection manager initialization...");
    LOG_DEBUG("Connection manager parameters: max_conn=%zu, req_buf=%zu, resp_buf=%zu, timeout=%us",
             max_connections, max_request_buffer, max_response_buffer, timeout_seconds);
    
    if (g_connection_manager) {
        LOG_DEBUG("Connection manager already initialized");
        return 0;
    }
    
    TRACE_NET("TRACE_CONNMGR_INIT: Allocating connection manager structure...");
    g_connection_manager =BUFFER_CALLOC(1, sizeof(connection_manager_t));
    if (!g_connection_manager) {
        LOG_ERROR("Failed to allocate memory for connection manager");
        return -1;
    }
    
    TRACE_NET("TRACE_CONNMGR_INIT: Initializing manager mutex...");
    /* Initialize manager mutex */
    if (pthread_mutex_init(&g_connection_manager->manager_mutex, NULL) != 0) {
        LOG_ERROR("Failed to initialize manager mutex: %s", strerror(errno));
        BUFFER_FREE(g_connection_manager);
        g_connection_manager = NULL;
        return -1;
    }
    
    TRACE_NET("TRACE_CONNMGR_INIT: Initializing atomic fields...");
    /* Initialize fields */
    atomic_init(&g_connection_manager->next_connection_id, 1);
    atomic_init(&g_connection_manager->active_count, 0);
    
    g_connection_manager->max_connections = max_connections;
    g_connection_manager->max_request_buffer_size = max_request_buffer;
    g_connection_manager->max_response_buffer_size = max_response_buffer;
    g_connection_manager->connection_timeout_seconds = timeout_seconds;
    
    TRACE_NET("TRACE_CONNMGR_INIT: Allocating connection tracking array for %zu connections...", max_connections);
    /* Allocate connection tracking array */
    g_connection_manager->active_connections =BUFFER_CALLOC(max_connections, sizeof(client_connection_t*));
    if (!g_connection_manager->active_connections) {
        LOG_ERROR("Failed to allocate connection tracking array");
        pthread_mutex_destroy(&g_connection_manager->manager_mutex);
        BUFFER_FREE(g_connection_manager);
        g_connection_manager = NULL;
        return -1;
    }
    
    LOG_INFO("Connection manager initialized successfully: max_connections=%zu, timeout=%us", 
             max_connections, timeout_seconds);
    TRACE_NET("TRACE_CONNMGR_INIT: Manager address=%p, active_connections array=%p", 
              (void*)g_connection_manager, (void*)g_connection_manager->active_connections);
    return 0;
}

int connection_manager_register(client_connection_t *conn) {
    if (!g_connection_manager || !conn) return -1;
    
    int result = -1;
    pthread_mutex_lock(&g_connection_manager->manager_mutex);
    
    size_t active_count = atomic_load(&g_connection_manager->active_count);
    if (active_count >= g_connection_manager->max_connections) {
        LOG_WARNING("Maximum connections reached (%zu), rejecting new connection", g_connection_manager->max_connections);
        pthread_mutex_unlock(&g_connection_manager->manager_mutex);
        return -1;
    }
    
    /* Find empty slot */
    for (size_t i = 0; i < g_connection_manager->max_connections; i++) {
        if (g_connection_manager->active_connections[i] == NULL) {
            g_connection_manager->active_connections[i] = conn;
            atomic_fetch_add(&g_connection_manager->active_count, 1);
            result = 0;
            break;
        }
    }
    
    pthread_mutex_unlock(&g_connection_manager->manager_mutex);
    
    if (result == 0) {
        LOG_DEBUG("Registered connection %lu, active count: %zu", conn->connection_id, active_count + 1);
    }
    
    return result;
}

void connection_manager_unregister(client_connection_t *conn) {
    if (!g_connection_manager || !conn) return;
    
    pthread_mutex_lock(&g_connection_manager->manager_mutex);
    
    /* Find and remove connection */
    for (size_t i = 0; i < g_connection_manager->max_connections; i++) {
        if (g_connection_manager->active_connections[i] == conn) {
            g_connection_manager->active_connections[i] = NULL;
            atomic_fetch_sub(&g_connection_manager->active_count, 1);
            LOG_DEBUG("Unregistered connection %lu", conn->connection_id);
            break;
        }
    }
    
    pthread_mutex_unlock(&g_connection_manager->manager_mutex);
}

void connection_manager_get_stats(size_t *active_count, uint64_t *total_created) {
    if (!g_connection_manager) {
        if (active_count) *active_count = 0;
        if (total_created) *total_created = 0;
        return;
    }
    
    if (active_count) {
        *active_count = atomic_load(&g_connection_manager->active_count);
    }
    
    if (total_created) {
        *total_created = atomic_load(&g_connection_manager->next_connection_id) - 1;
    }
}

int connection_manager_cleanup_expired(void) {
    if (!g_connection_manager) return 0;
    
    int cleaned_up = 0;
    pthread_mutex_lock(&g_connection_manager->manager_mutex);
    
    for (size_t i = 0; i < g_connection_manager->max_connections; i++) {
        client_connection_t *conn = g_connection_manager->active_connections[i];
        if (conn && is_connection_expired(conn, g_connection_manager->connection_timeout_seconds)) {
            LOG_DEBUG("Cleaning up expired connection %lu", conn->connection_id);
            
            /* Mark as closing and remove from tracking */
            client_connection_set_state(conn, CONN_STATE_CLOSING);
            g_connection_manager->active_connections[i] = NULL;
            atomic_fetch_sub(&g_connection_manager->active_count, 1);
            
            /* Release manager's reference */
            client_connection_release(conn);
            cleaned_up++;
        }
    }
    
    pthread_mutex_unlock(&g_connection_manager->manager_mutex);
    
    if (cleaned_up > 0) {
        LOG_INFO("Cleaned up %d expired connections", cleaned_up);
    }
    
    return cleaned_up;
}

void connection_manager_shutdown(void) {
    if (!g_connection_manager) return;
    
    LOG_INFO("Shutting down connection manager.");
    
    pthread_mutex_lock(&g_connection_manager->manager_mutex);
    
    /* Close all active connections */
    for (size_t i = 0; i < g_connection_manager->max_connections; i++) {
        client_connection_t *conn = g_connection_manager->active_connections[i];
        if (conn) {
            client_connection_set_state(conn, CONN_STATE_CLOSING);
            client_connection_release(conn);
            g_connection_manager->active_connections[i] = NULL;
        }
    }
    
    atomic_store(&g_connection_manager->active_count, 0);
    
    pthread_mutex_unlock(&g_connection_manager->manager_mutex);
    
    /* Cleanup manager */
    BUFFER_FREE(g_connection_manager->active_connections);
    pthread_mutex_destroy(&g_connection_manager->manager_mutex);
    BUFFER_FREE(g_connection_manager);
    g_connection_manager = NULL;
    
    LOG_INFO("Connection manager shutdown complete.");
}
