#ifndef CLIENT_CONNECTION_H
#define CLIENT_CONNECTION_H

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <stdint.h>

/* Forward declarations */
struct api_context;
struct ssl_connection_t;

/**
 * Connection State Machine - Thread-safe connection lifecycle
 */
typedef enum {
    CONN_STATE_INITIALIZING = 0,  /* Being created */
    CONN_STATE_ACTIVE,            /* Actively processing */
    CONN_STATE_READING,           /* Reading request data */
    CONN_STATE_PROCESSING,        /* Processing request */
    CONN_STATE_WRITING,           /* Writing response */
    CONN_STATE_CLOSING,           /* Being closed */
    CONN_STATE_CLOSED,            /* Fully closed */
    CONN_STATE_ERROR              /* Error state */
} connection_state_t;

/**
 * Thread-safe client connection structure with reference counting
 * and comprehensive concurrency protection
 */
typedef struct client_connection {
    /* === IMMUTABLE FIELDS (set once, never changed) === */
    uint64_t connection_id;              /* Unique connection identifier */
    struct sockaddr_in client_address;   /* Client address info */
    struct api_context *api_ctx;         /* API context reference */
    struct timespec created_at;          /* Connection creation timestamp */
    
    /* === ATOMIC REFERENCE COUNTING === */
    atomic_int ref_count;                /* Reference counter for safe cleanup */
    
    /* === MUTEX-PROTECTED MUTABLE STATE === */
    pthread_mutex_t state_mutex;         /* Protects all mutable state */
    
    /* Connection state (protected by state_mutex) */
    connection_state_t state;
    int socket_fd;                       /* Socket file descriptor */
    int use_ssl;                         /* SSL flag */
    struct ssl_connection_t *ssl_conn;   /* SSL connection (if enabled) */
    
    /* Request processing state (protected by state_mutex) */
    char *request_buffer;                /* Current request buffer */
    size_t request_buffer_size;          /* Buffer size */
    size_t request_bytes_received;       /* Bytes received so far */
    
    /* Response state (protected by state_mutex) */
    char *response_buffer;               /* Response buffer */
    size_t response_buffer_size;         /* Response buffer size */
    size_t response_bytes_sent;          /* Bytes sent so far */
    
    /* Timing and metrics (protected by state_mutex) */
    struct timespec last_activity;       /* Last activity timestamp */
    struct timespec processing_start;    /* When request processing started */
    uint32_t requests_handled;           /* Number of requests on this connection */
    
    /* Error handling (protected by state_mutex) */
    int error_code;                      /* Last error code */
    char error_message[256];             /* Last error message */
    
    /* === CLEANUP SYNCHRONIZATION === */
    pthread_cond_t cleanup_complete;     /* Signals when cleanup is done */
    atomic_bool cleanup_in_progress;     /* Cleanup is happening */
    atomic_bool is_destroyed;            /* Connection fully destroyed */
    
} client_connection_t;

/**
 * Connection manager for thread-safe operations
 */
typedef struct {
    pthread_mutex_t manager_mutex;       /* Protects manager state */
    atomic_ullong next_connection_id;  /* Next connection ID to assign */
    
    /* Connection tracking */
    client_connection_t **active_connections;  /* Array of active connections */
    size_t max_connections;                     /* Maximum concurrent connections */
    atomic_size_t active_count;                 /* Current active connection count */
    
    /* Resource limits */
    size_t max_request_buffer_size;      /* Maximum request buffer size */
    size_t max_response_buffer_size;     /* Maximum response buffer size */
    uint32_t connection_timeout_seconds; /* Connection timeout */
    
} connection_manager_t;

/* === CONNECTION LIFECYCLE FUNCTIONS === */

/**
 * Create a new thread-safe client connection
 * @param socket_fd Socket file descriptor (will be managed by connection)
 * @param client_addr Client address information
 * @param api_ctx API context reference
 * @param use_ssl Whether to use SSL for this connection
 * @return New connection or NULL on failure
 */
client_connection_t* client_connection_create(
    int socket_fd,
    struct sockaddr_in *client_addr,
    struct api_context *api_ctx,
    int use_ssl
);

/**
 * Acquire a reference to a connection (thread-safe)
 * @param conn Connection to reference
 * @return Same connection pointer or NULL if connection is being destroyed
 */
client_connection_t* client_connection_acquire(client_connection_t *conn);

/**
 * Release a reference to a connection (thread-safe)
 * When reference count reaches zero, connection is automatically destroyed
 * @param conn Connection to release
 */
void client_connection_release(client_connection_t *conn);

/* === CONNECTION STATE MANAGEMENT === */

/**
 * Thread-safe state transition
 * @param conn Connection
 * @param new_state New state to transition to
 * @return 0 on success, -1 if transition is invalid
 */
int client_connection_set_state(client_connection_t *conn, connection_state_t new_state);

/**
 * Get current connection state (thread-safe)
 * @param conn Connection
 * @return Current state
 */
connection_state_t client_connection_get_state(client_connection_t *conn);

/**
 * Check if connection is in a valid state for operations
 * @param conn Connection
 * @return 1 if active, 0 if not
 */
int client_connection_is_active(client_connection_t *conn);

/* === I/O OPERATIONS === */

/**
 * Thread-safe socket read with timeout and state management
 * @param conn Connection
 * @param buffer Buffer to read into
 * @param buffer_size Size of buffer
 * @param timeout_ms Timeout in milliseconds
 * @return Bytes read, 0 for EOF, -1 for error
 */
ssize_t client_connection_read(client_connection_t *conn, void *buffer, size_t buffer_size, int timeout_ms);

/**
 * Thread-safe socket write with state management
 * @param conn Connection
 * @param data Data to write
 * @param data_size Size of data
 * @return Bytes written, -1 for error
 */
ssize_t client_connection_write(client_connection_t *conn, const void *data, size_t data_size);

/* === ERROR HANDLING === */

/**
 * Set error state on connection (thread-safe)
 * @param conn Connection
 * @param error_code Error code
 * @param error_message Error message (will be copied)
 */
void client_connection_set_error(client_connection_t *conn, int error_code, const char *error_message);

/**
 * Get last error information (thread-safe)
 * @param conn Connection
 * @param error_code Output for error code
 * @param error_message Output buffer for error message
 * @param message_size Size of error message buffer
 * @return 0 if no error, 1 if error information retrieved
 */
int client_connection_get_error(client_connection_t *conn, int *error_code, char *error_message, size_t message_size);

/* === CONNECTION MANAGER === */

/**
 * Initialize the global connection manager
 * @param max_connections Maximum concurrent connections
 * @param max_request_buffer Maximum request buffer size
 * @param max_response_buffer Maximum response buffer size
 * @param timeout_seconds Connection timeout in seconds
 * @return 0 on success, -1 on failure
 */
int connection_manager_init(size_t max_connections, size_t max_request_buffer, 
                           size_t max_response_buffer, uint32_t timeout_seconds);

/**
 * Register a new connection with the manager
 * @param conn Connection to register
 * @return 0 on success, -1 if too many connections
 */
int connection_manager_register(client_connection_t *conn);

/**
 * Unregister a connection from the manager
 * @param conn Connection to unregister
 */
void connection_manager_unregister(client_connection_t *conn);

/**
 * Get current connection statistics
 * @param active_count Output for active connection count
 * @param total_created Output for total connections created
 */
void connection_manager_get_stats(size_t *active_count, uint64_t *total_created);

/**
 * Cleanup expired connections (call periodically)
 * @return Number of connections cleaned up
 */
int connection_manager_cleanup_expired(void);

/**
 * Shutdown the connection manager and cleanup all connections
 */
void connection_manager_shutdown(void);

/* === UTILITY MACROS === */

/**
 * Safe connection operation macro - ensures connection is valid and acquires reference
 */
#define WITH_CONNECTION(conn, block) do { \
    client_connection_t *_safe_conn = client_connection_acquire(conn); \
    if (_safe_conn && client_connection_is_active(_safe_conn)) { \
        block \
    } \
    if (_safe_conn) client_connection_release(_safe_conn); \
} while(0)

/**
 * Connection state lock macro for critical sections
 */
#define WITH_CONNECTION_LOCK(conn, block) do { \
    if (conn && pthread_mutex_lock(&(conn)->state_mutex) == 0) { \
        block \
        pthread_mutex_unlock(&(conn)->state_mutex); \
    } \
} while(0)

#endif /* CLIENT_CONNECTION_H */