#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "utils/json.h"
#include "utils/logger.h"
#include "utils/config_defaults.h"
#include "core/thread_pool.h"

/* Forward declarations to avoid circular dependencies */
typedef struct api_context api_context_t;
typedef struct ssl_context_t ssl_context_t;

/* JDBX file path utilities */
char* jdbx_generate_db_path(const char* basename);
char* jdbx_generate_wal_path(const char* basename);

/* Server configuration */
#define DEFAULT_PORT 5000
#define MAX_CONNECTIONS 100
#define BUFFER_SIZE 4096
/* ADMIN_FILES_DIR is now dynamically determined from web_root config */

/* Server status codes */
typedef enum {
    SERVER_OK = 0,
    SERVER_ERROR,
    SERVER_SOCKET_ERROR,
    SERVER_BIND_ERROR,
    SERVER_LISTEN_ERROR,
    SERVER_THREAD_ERROR
} server_status_t;

/* HTTP response codes */
typedef enum {
    HTTP_OK = 200,
    HTTP_CREATED = 201,
    HTTP_NO_CONTENT = 204,
    HTTP_FOUND = 302,
    HTTP_BAD_REQUEST = 400,
    HTTP_UNAUTHORIZED = 401,
    HTTP_FORBIDDEN = 403,
    HTTP_NOT_FOUND = 404,
    HTTP_METHOD_NOT_ALLOWED = 405,
    HTTP_CONFLICT = 409,
    HTTP_INTERNAL_SERVER_ERROR = 500
} http_status_t;

/* HTTP methods */
typedef enum {
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE,
    HTTP_PATCH,
    HTTP_OPTIONS,
    HTTP_HEAD,
    HTTP_UNKNOWN
} http_method_t;

/* CORS configuration */
typedef struct {
    int enabled;                  /* Enable/disable CORS */
    char** allowed_origins;       /* List of allowed origins */
    int allowed_origins_count;    /* Number of allowed origins */
    char** allowed_methods;       /* List of allowed methods */
    int allowed_methods_count;    /* Number of allowed methods */
    char** allowed_headers;       /* List of allowed headers */
    int allowed_headers_count;    /* Number of allowed headers */
    int allow_credentials;        /* Allow credentials */
    int max_age;                  /* Max age of preflight requests */
} cors_config_t;

/* Forward declarations */
struct api_context;
struct metrics_registry;

/* Server configuration struct */
typedef struct {
    /* Server settings */
    int port;
    int max_connections;
    int socket_fd;
    char* host;                  /* Binding host address */

    /* File paths */
    char* db_file;                      /* Database file basename (JDBX auto-generates .jdbx and .wal) */
    /* JDBX is the ONLY storage - no backend selection needed */
    size_t jdbx_initial_size;           /* JDBX initial file size */
    size_t jdbx_wal_size;               /* JDBX WAL size */
    /* char* rbac_path; */         /* DEPRECATED: RBAC is database-backed */
    char* pid_file;
    char* log_file;
    char* web_root;                 /* Path to web admin interface files */
    /* char* validators_dir; */     /* DEPRECATED: JS functions embedded in documents */
    /* char* transforms_dir; */     /* DEPRECATED: JS functions embedded in documents */
    /* char* metrics_dir; */        /* DEPRECATED: Metrics stored in database */
    char* metrics_export_path;      /* Path for metrics auto-export */
    int metrics_export_interval;    /* Metrics export interval in seconds */

    /* Runtime settings */
    log_level_t log_level;       /* Logging level */
    int verbose_mode;            /* Enable verbose logging */
    int js_enabled;              /* Enable JavaScript engine */
    char* jwt_secret;            /* JWT secret key */

    /* Configuration */
    cors_config_t cors;          /* CORS configuration */
    char* config_file;           /* Config file path if loaded from a file */

    /* Thread pool configuration */
    int thread_pool_min;         /* Minimum number of threads */
    int thread_pool_max;         /* Maximum number of threads */
    int thread_pool_queue_size;  /* Thread pool queue size */
    int thread_pool_idle_timeout;/* Thread idle timeout in seconds */

    /* References */
    struct api_context* api_ctx;  /* API context */
    struct metrics_registry* metrics; /* Metrics registry */
    thread_pool_t* thread_pool;   /* Thread pool for handling client connections */

    /* SSL settings - ADDED AT END TO AVOID FIELD MISALIGNMENT */
    int use_ssl;
    char* cert_path;
    char* key_path;
    ssl_context_t* ssl_context; /* SSL context for TLS connections */

    /* Cache configuration */
    int cache_enabled;           /* Enable/disable cache */
    size_t cache_max_size;       /* Maximum cache size in bytes */
    int cache_ttl;               /* Cache TTL in seconds */

    /* Metrics configuration */
    int metrics_enabled;         /* Enable/disable metrics */
    int metrics_retention;       /* Number of data points to retain */

    /* Adaptive indexing configuration */
    int index_query_threshold;        /* Query count threshold for indexing */
    int index_time_threshold;         /* Time threshold for indexing (ms) */
    int index_query_threshold_system; /* Query threshold for system collections */
    int index_time_threshold_system;  /* Time threshold for system collections (ms) */
    int index_startup_delay;          /* Indexing startup delay (seconds) */
    int index_check_interval;         /* Indexing check interval (seconds) */

    /* Status */
    int error;                   /* Error code */
} server_config_t;

/* Compile-time checks to prevent field misalignment issues */
#include <stddef.h>
/* Verify critical field offsets to catch struct corruption early */
/* _Static_assert(offsetof(server_config_t, pid_file) != offsetof(server_config_t, rbac_path), 
               "pid_file and rbac_path fields must have different offsets"); */ /* DEPRECATED */
_Static_assert(offsetof(server_config_t, log_file) != offsetof(server_config_t, pid_file), 
               "log_file and pid_file fields must have different offsets");

/* Cookie struct */
typedef struct cookie {
    char* name;
    char* value;
    struct cookie* next;
} cookie_t;

/* HTTP request struct */
typedef struct {
    http_method_t method;
    char* path;
    char* query;
    char* body;
    char* content_type;
    char* authorization;
    char* cookie_header;   /* Raw Cookie header */
    cookie_t* cookies;     /* Parsed cookies list */
    char* origin;          /* Origin header value */
    size_t content_length;
    char* user_agent;      /* User-Agent header */
    char* remote_addr;     /* Client IP address */
    int keep_alive;        /* Keep-Alive header flag (1 = keep-alive, 0 = close) */
} http_request_t;

/* HTTP response struct */
typedef struct {
    http_status_t status;
    char* body;
    char* content_type;
    size_t content_length;
    char** headers;        /* Additional headers */
    size_t num_headers;    /* Number of additional headers */
    int keep_alive;        /* Keep-Alive flag (1 = keep-alive, 0 = close) */
} http_response_t;

/* Client connection info */
typedef struct {
    int client_fd;
    struct sockaddr_in address;
    struct api_context *api_ctx;  /* Reference to the API context */
    
    /* SSL connection information */
    int use_ssl;                   /* Whether this connection uses SSL */
    struct ssl_connection_t *ssl_conn;  /* SSL connection if SSL is enabled */
} client_conn_t;

/* Server initialization and control functions */

/**
 * Initialize and run the server with the thread pool implementation
 *
 * This is the main entry point for starting the JDBX server.
 * It handles signal setup, socket initialization, thread pool creation,
 * and runs the server in the current thread.
 *
 * @param config Server configuration
 * @param api_ctx API context for request handling
 * @return Server status code
 */
server_status_t server_initialize_and_run(server_config_t* config, struct api_context* api_ctx);

/**
 * Request a graceful server shutdown
 * 
 * This function can be called from any thread to initiate a server shutdown
 */
void server_request_shutdown(void);

/* Deprecated functions - maintained for backward compatibility */
server_status_t server_init(server_config_t* config, struct api_context* api_ctx) __attribute__((deprecated));
server_status_t server_start(server_config_t* config) __attribute__((deprecated));
void* server_accept_loop(void* config_ptr) __attribute__((deprecated));
void server_stop(server_config_t* config) __attribute__((deprecated));
void server_request_shutdown(void);
void handle_client(void* client_data);
http_request_t* parse_http_request(const char* request_str);
void free_http_request(http_request_t* request);
http_response_t* create_http_response(http_status_t status, const char* body, const char* content_type);
http_response_t* create_http_response_binary(http_status_t status, const char* body, size_t body_size, const char* content_type);
http_response_t* http_response_error(const char* message, int status_code);
http_response_t* http_response_json(json_value_t* json, int status_code);
char* serialize_http_response(http_response_t* response);
char* serialize_http_response_with_length(http_response_t* response, size_t* length);
char* serialize_http_response_keep_alive(http_response_t* response, int keep_alive);
char* serialize_http_response_keep_alive_with_length(http_response_t* response, int keep_alive, size_t* length);
void free_http_response(http_response_t* response);
int add_response_header(http_response_t* response, const char* header);
http_method_t parse_http_method(const char* method_str);
const char* http_status_string(http_status_t status);

/* Admin file serving functions */
http_response_t* serve_admin_file(const char* path);
int is_admin_route(const char* path);
const char* get_file_extension(const char* filename);
const char* get_content_type_from_extension(const char* extension);
char* read_file_content(const char* filepath, size_t* size);

/* Admin authentication settings */
#define ADMIN_AUTH_COOKIE_NAME "jdbx_admin_auth"
#define ADMIN_AUTH_COOKIE_TTL 3600  /* 1 hour in seconds */

/* Admin authentication functions */
int check_admin_auth(http_request_t* request);
char* generate_admin_auth_token(const char* username);
http_response_t* create_auth_response(http_response_t* response, const char* token);
void parse_cookies(http_request_t* request);
char* get_cookie_value(http_request_t* request, const char* name);

/* CORS functions */
void init_cors_config(cors_config_t* cors);
void free_cors_config(cors_config_t* cors);
int add_cors_allowed_origin(cors_config_t* cors, const char* origin);
int add_cors_allowed_method(cors_config_t* cors, const char* method);
int add_cors_allowed_header(cors_config_t* cors, const char* header);
int is_cors_allowed_origin(cors_config_t* cors, const char* origin);
http_response_t* apply_cors_headers(http_response_t* response, 
                                   cors_config_t* cors, 
                                   const char* origin);

/* Enhanced server modes for performance optimization */
server_status_t run_server_auto(server_config_t* config, api_context_t* api_ctx);
server_status_t run_server_epoll(server_config_t* config, api_context_t* api_ctx);
server_status_t run_server_standard(server_config_t* config, api_context_t* api_ctx);

/* SSL functions */
struct ssl_context_t; /* Forward declaration */
struct ssl_context_t* server_get_ssl_context(void);

#endif /* SERVER_H */