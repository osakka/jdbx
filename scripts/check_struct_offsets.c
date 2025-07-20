#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <stdatomic.h>
#include <stdbool.h>

/* Copy the client_connection_t struct definition */
typedef enum {
    CONN_STATE_INITIALIZING = 0,
    CONN_STATE_IDLE,
    CONN_STATE_ACTIVE,
    CONN_STATE_READING,
    CONN_STATE_PROCESSING,
    CONN_STATE_WRITING,
    CONN_STATE_CLOSING,
    CONN_STATE_CLOSED,
    CONN_STATE_ERROR
} connection_state_t;

typedef struct client_connection {
    uint64_t connection_id;
    struct sockaddr_in client_address;
    void *api_ctx;
    struct timespec created_at;
    
    atomic_int ref_count;
    
    pthread_mutex_t state_mutex;
    
    connection_state_t state;
    int socket_fd;
    int use_ssl;
    void *ssl_conn;
    
    char *request_buffer;
    size_t request_buffer_size;
    size_t request_bytes_received;
    
    char *response_buffer;
    size_t response_buffer_size;
    size_t response_bytes_sent;
    
    struct timespec last_activity;
    struct timespec processing_start;
    uint32_t requests_handled;
    
    int error_code;
    char error_message[256];
    
    pthread_cond_t cleanup_complete;
    atomic_bool cleanup_in_progress;
    atomic_bool is_destroyed;
    
} client_connection_t;

int main() {
    printf("=== CLIENT CONNECTION STRUCT OFFSETS ===\n");
    printf("Total size: %zu bytes\n", sizeof(client_connection_t));
    printf("\n");
    
    printf("Field offsets:\n");
    printf("connection_id: %zu\n", offsetof(client_connection_t, connection_id));
    printf("client_address: %zu\n", offsetof(client_connection_t, client_address));
    printf("api_ctx: %zu\n", offsetof(client_connection_t, api_ctx));
    printf("created_at: %zu\n", offsetof(client_connection_t, created_at));
    printf("ref_count: %zu\n", offsetof(client_connection_t, ref_count));
    printf("state_mutex: %zu\n", offsetof(client_connection_t, state_mutex));
    printf("state: %zu\n", offsetof(client_connection_t, state));
    printf("socket_fd: %zu\n", offsetof(client_connection_t, socket_fd));
    printf("use_ssl: %zu\n", offsetof(client_connection_t, use_ssl));
    printf("ssl_conn: %zu\n", offsetof(client_connection_t, ssl_conn));
    printf("request_buffer: %zu\n", offsetof(client_connection_t, request_buffer));
    printf("request_buffer_size: %zu\n", offsetof(client_connection_t, request_buffer_size));
    printf("request_bytes_received: %zu\n", offsetof(client_connection_t, request_bytes_received));
    printf("response_buffer: %zu\n", offsetof(client_connection_t, response_buffer));
    printf("response_buffer_size: %zu\n", offsetof(client_connection_t, response_buffer_size));
    printf("response_bytes_sent: %zu\n", offsetof(client_connection_t, response_bytes_sent));
    printf("last_activity: %zu\n", offsetof(client_connection_t, last_activity));
    printf("processing_start: %zu\n", offsetof(client_connection_t, processing_start));
    printf("requests_handled: %zu\n", offsetof(client_connection_t, requests_handled));
    printf("error_code: %zu\n", offsetof(client_connection_t, error_code));
    printf("error_message: %zu\n", offsetof(client_connection_t, error_message));
    printf("cleanup_complete: %zu\n", offsetof(client_connection_t, cleanup_complete));
    printf("cleanup_in_progress: %zu\n", offsetof(client_connection_t, cleanup_in_progress));
    printf("is_destroyed: %zu\n", offsetof(client_connection_t, is_destroyed));
    
    printf("\n=== CHECKING SEGFAULT ADDRESSES ===\n");
    printf("Segfault at offset 64: checking what field this is...\n");
    printf("Segfault at offset 121: checking what field this is...\n");
    
    return 0;
}