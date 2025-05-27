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

/* Server configuration */
#define DEFAULT_PORT 5000
#define MAX_CONNECTIONS 100
#define BUFFER_SIZE 4096
#define ADMIN_FILES_DIR DEFAULT_ADMIN_FILES_DIR

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

    /* SSL settings */
    int use_ssl;
    char* cert_path;
    char* key_path;

    /* File paths */
    char* db_path;
    char* rbac_path;
    char* pid_file;
    char* log_file;
    char* web_root;                 /* Path to web admin interface files */
    char* validators_dir;           /* Path to validators directory */
    char* transforms_dir;           /* Path to transforms directory */
    char* metrics_dir;              /* Path to metrics directory */
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

    /* References */
    struct api_context* api_ctx;  /* API context */
    struct metrics_registry* metrics; /* Metrics registry */
    thread_pool_t* thread_pool;   /* Thread pool for handling client connections */

    /* Status */
    int error;                   /* Error code */
} server_config_t;

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
} http_request_t;

/* HTTP response struct */
typedef struct {
    http_status_t status;
    char* body;
    char* content_type;
    size_t content_length;
    char** headers;        /* Additional headers */
    size_t num_headers;    /* Number of additional headers */
} http_response_t;

/* Client connection info */
typedef struct {
    int client_fd;
    struct sockaddr_in address;
    struct api_context *api_ctx;  /* Reference to the API context */
} client_conn_t;

/* Server initialization and control functions */

/**
 * Initialize and run the server with the thread pool implementation
 *
 * This is the main entry point for starting the JSONdb server.
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
void* handle_client(void* client_data);
http_request_t* parse_http_request(const char* request_str);
void free_http_request(http_request_t* request);
http_response_t* create_http_response(http_status_t status, const char* body, const char* content_type);
http_response_t* http_response_error(const char* message, int status_code);
http_response_t* http_response_json(json_value_t* json, int status_code);
char* serialize_http_response(http_response_t* response);
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
#define ADMIN_AUTH_COOKIE_NAME "jsondb_admin_auth"
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

#endif /* SERVER_H */