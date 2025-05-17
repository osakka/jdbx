#include "core/server.h"
#include "api/api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctype.h>

/* Forward declaration for accept loop */
void* server_accept_loop(void* config_ptr);

/* External API context declaration */
#ifndef TOOLS_BUILD
extern api_context_t* g_api_ctx;
#else
api_context_t* g_api_ctx = NULL;
#endif

/* Global server configuration */
/* g_server_config is now defined in config_loader.c */
#ifdef TOOLS_BUILD
/* Only define this if building tools */
server_config_t* g_server_config = NULL;
#endif

/* Thread pool for handling client connections */
static client_conn_t* clients = NULL;
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Flag for server running state */
static int server_running = 0;

/* HTTP method string to enum conversion */
http_method_t parse_http_method(const char* method_str) {
    if (strcmp(method_str, "GET") == 0) return HTTP_GET;
    if (strcmp(method_str, "POST") == 0) return HTTP_POST;
    if (strcmp(method_str, "PUT") == 0) return HTTP_PUT;
    if (strcmp(method_str, "DELETE") == 0) return HTTP_DELETE;
    if (strcmp(method_str, "PATCH") == 0) return HTTP_PATCH;
    return HTTP_UNKNOWN;
}

/* HTTP status code to string conversion */
const char* http_status_string(http_status_t status) {
    switch (status) {
        case HTTP_OK:
            return "200 OK";
        case HTTP_CREATED:
            return "201 Created";
        case HTTP_NO_CONTENT:
            return "204 No Content";
        case HTTP_FOUND:
            return "302 Found";
        case HTTP_BAD_REQUEST:
            return "400 Bad Request";
        case HTTP_UNAUTHORIZED:
            return "401 Unauthorized";
        case HTTP_FORBIDDEN:
            return "403 Forbidden";
        case HTTP_NOT_FOUND:
            return "404 Not Found";
        case HTTP_METHOD_NOT_ALLOWED:
            return "405 Method Not Allowed";
        case HTTP_INTERNAL_SERVER_ERROR:
            return "500 Internal Server Error";
        default:
            return "500 Internal Server Error";
    }
}

/* Parse cookies from Cookie header */
void parse_cookies(http_request_t* request) {
    if (!request || !request->cookie_header) {
        return;
    }
    
    char* cookie_str = strdup(request->cookie_header);
    if (!cookie_str) {
        return;
    }
    
    /* Split cookie string by '; ' */
    char* saveptr;
    char* cookie_pair = strtok_r(cookie_str, ";", &saveptr);
    
    while (cookie_pair) {
        /* Trim leading whitespace */
        while (*cookie_pair == ' ') {
            cookie_pair++;
        }
        
        /* Find equals sign separating name and value */
        char* equals = strchr(cookie_pair, '=');
        if (equals) {
            /* Create cookie */
            cookie_t* cookie = (cookie_t*)malloc(sizeof(cookie_t));
            if (cookie) {
                /* Get name and value */
                *equals = '\0';
                cookie->name = strdup(cookie_pair);
                cookie->value = strdup(equals + 1);
                cookie->next = NULL;
                
                /* Add to cookies list */
                if (!request->cookies) {
                    request->cookies = cookie;
                } else {
                    cookie_t* last = request->cookies;
                    while (last->next) {
                        last = last->next;
                    }
                    last->next = cookie;
                }
            }
        }
        
        cookie_pair = strtok_r(NULL, ";", &saveptr);
    }
    
    free(cookie_str);
}

/* Get cookie value by name */
char* get_cookie_value(http_request_t* request, const char* name) {
    if (!request || !request->cookies || !name) {
        return NULL;
    }
    
    cookie_t* cookie = request->cookies;
    while (cookie) {
        if (cookie->name && strcmp(cookie->name, name) == 0) {
            return cookie->value;
        }
        cookie = cookie->next;
    }
    
    return NULL;
}

/* Parse HTTP request from string */
http_request_t* parse_http_request(const char* request_str) {
    if (!request_str) {
        return NULL;
    }
    
    http_request_t* request = (http_request_t*)malloc(sizeof(http_request_t));
    if (!request) {
        return NULL;
    }
    
    /* Initialize request fields */
    request->method = HTTP_UNKNOWN;
    request->path = NULL;
    request->query = NULL;
    request->body = NULL;
    request->content_type = NULL;
    request->authorization = NULL;
    request->cookie_header = NULL;
    request->cookies = NULL;
    request->origin = NULL;
    request->content_length = 0;
    
    /* Parse request line and headers */
    char* request_copy = strdup(request_str);
    char* line = strtok(request_copy, "\r\n");
    
    if (line) {
        /* Parse request line */
        char method_str[16] = {0};
        char url[2048] = {0};
        sscanf(line, "%15s %2047s", method_str, url);
        
        /* Set method */
        request->method = parse_http_method(method_str);
        
        /* Parse URL (path and query) */
        char* query_start = strchr(url, '?');
        if (query_start) {
            /* Split path and query */
            *query_start = '\0';
            query_start++;
            request->path = strdup(url);
            request->query = strdup(query_start);
        } else {
            /* No query part */
            request->path = strdup(url);
            request->query = NULL;
        }
        
        /* Parse headers */
        line = strtok(NULL, "\r\n");
        while (line && *line) {
            /* Content-Type header */
            if (strncasecmp(line, "Content-Type:", 13) == 0) {
                request->content_type = strdup(line + 14);
                /* Trim leading/trailing whitespace */
                while (*request->content_type == ' ') {
                    request->content_type++;
                }
            }
            
            /* Content-Length header */
            else if (strncasecmp(line, "Content-Length:", 15) == 0) {
                request->content_length = atoi(line + 16);
            }
            
            /* Authorization header */
            else if (strncasecmp(line, "Authorization:", 14) == 0) {
                request->authorization = strdup(line + 15);
                /* Trim leading/trailing whitespace */
                while (*request->authorization == ' ') {
                    request->authorization++;
                }
            }
            
            /* Cookie header */
            else if (strncasecmp(line, "Cookie:", 7) == 0) {
                request->cookie_header = strdup(line + 8);
                /* Trim leading/trailing whitespace */
                while (*request->cookie_header == ' ') {
                    request->cookie_header++;
                }
            }
            
            /* Origin header */
            else if (strncasecmp(line, "Origin:", 7) == 0) {
                request->origin = strdup(line + 8);
                /* Trim leading/trailing whitespace */
                while (*request->origin == ' ') {
                    request->origin++;
                }
            }
            
            line = strtok(NULL, "\r\n");
        }
        
        /* Parse body if present */
        if (request->content_length > 0) {
            /* Find the body start (after the double CRLF) */
            const char* body_start = strstr(request_str, "\r\n\r\n");
            if (body_start) {
                body_start += 4;  /* Skip the double CRLF */
                request->body = strdup(body_start);
            }
        }
        
        /* Parse cookies if present */
        if (request->cookie_header) {
            parse_cookies(request);
        }
    }
    
    free(request_copy);
    return request;
}

/* Free HTTP request */
void free_http_request(http_request_t* request) {
    if (request) {
        if (request->path) free(request->path);
        if (request->query) free(request->query);
        if (request->body) free(request->body);
        if (request->content_type) free(request->content_type);
        if (request->authorization) free(request->authorization);
        if (request->cookie_header) free(request->cookie_header);
        if (request->origin) free(request->origin);
        
        /* Free cookies */
        cookie_t* cookie = request->cookies;
        while (cookie) {
            cookie_t* next = cookie->next;
            if (cookie->name) free(cookie->name);
            if (cookie->value) free(cookie->value);
            free(cookie);
            cookie = next;
        }
        
        free(request);
    }
}

/* Create HTTP response */
http_response_t* create_http_response(http_status_t status, const char* body, const char* content_type) {
    http_response_t* response = (http_response_t*)malloc(sizeof(http_response_t));
    if (!response) {
        return NULL;
    }
    
    response->status = status;
    response->body = body ? strdup(body) : NULL;
    response->content_type = content_type ? strdup(content_type) : strdup("application/json");
    response->content_length = response->body ? strlen(response->body) : 0;
    response->headers = NULL;
    response->num_headers = 0;
    
    return response;
}

/* Create an error response */
http_response_t* http_response_error(const char* message, int status_code) {
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "{\"error\":\"%s\"}", message);
    return create_http_response((http_status_t)status_code, buffer, "application/json");
}

/* Create a JSON response */
http_response_t* http_response_json(json_value_t* json, int status_code) {
    if (!json) {
        return http_response_error("Invalid JSON data", 500);
    }
    
    char* json_str = json_stringify(json);
    if (!json_str) {
        return http_response_error("Failed to stringify JSON", 500);
    }
    
    http_response_t* response = create_http_response((http_status_t)status_code, json_str, "application/json");
    free(json_str);
    
    return response;
}

/* Serialize HTTP response to string */
char* serialize_http_response(http_response_t* response) {
    if (!response) {
        return NULL;
    }

    /* Calculate response size with a larger base size and safety margin */
    int response_size = 1024;  /* Increased base size for headers */
    if (response->body) {
        response_size += response->content_length;
    }

    /* Add space for additional headers with larger allocation per header */
    response_size += response->num_headers * 256;

    /* Add safety margin to prevent buffer overflows */
    response_size += 1024;

    /* Allocate response string */
    char* response_str = (char*)malloc(response_size);
    if (!response_str) {
        return NULL;
    }
    
    /* Create response */
    size_t written = 0;
    size_t remaining = response_size;

    /* Status line */
    int n = snprintf(response_str + written, remaining, "HTTP/1.1 %s\r\n", http_status_string(response->status));
    if (n < 0 || (size_t)n >= remaining) {
        free(response_str);
        return NULL;
    }
    written += n;
    remaining -= n;

    /* Headers */
    n = snprintf(response_str + written, remaining, "Content-Type: %s\r\n", response->content_type);
    if (n < 0 || (size_t)n >= remaining) {
        free(response_str);
        return NULL;
    }
    written += n;
    remaining -= n;

    n = snprintf(response_str + written, remaining, "Content-Length: %zu\r\n", response->content_length);
    if (n < 0 || (size_t)n >= remaining) {
        free(response_str);
        return NULL;
    }
    written += n;
    remaining -= n;

    n = snprintf(response_str + written, remaining, "Connection: close\r\n");
    if (n < 0 || (size_t)n >= remaining) {
        free(response_str);
        return NULL;
    }
    written += n;
    remaining -= n;

    /* Default CORS headers are now added by the apply_cors_headers function */

    /* Additional headers */
    for (size_t i = 0; i < response->num_headers; i++) {
        if (response->headers[i]) {
            n = snprintf(response_str + written, remaining, "%s\r\n", response->headers[i]);
            if (n < 0 || (size_t)n >= remaining) {
                free(response_str);
                return NULL;
            }
            written += n;
            remaining -= n;
        }
    }

    /* End of headers */
    n = snprintf(response_str + written, remaining, "\r\n");
    if (n < 0 || (size_t)n >= remaining) {
        free(response_str);
        return NULL;
    }
    written += n;
    remaining -= n;

    /* Body */
    if (response->body && response->content_length > 0) {
        if (response->content_length + 1 > remaining) { /* +1 for null terminator */
            free(response_str);
            return NULL;
        }
        memcpy(response_str + written, response->body, response->content_length);
        written += response->content_length;
    }

    /* Ensure null-termination */
    if (remaining > 0) {
        response_str[written] = '\0';
    } else {
        free(response_str);
        return NULL;
    }
    
    return response_str;
}

/* Add header to HTTP response */
int add_response_header(http_response_t* response, const char* header) {
    if (!response || !header) {
        return 0;
    }
    
    /* Allocate or reallocate headers array */
    char** new_headers = (char**)realloc(response->headers, 
                                        (response->num_headers + 1) * sizeof(char*));
    if (!new_headers) {
        return 0;
    }
    
    response->headers = new_headers;
    response->headers[response->num_headers] = strdup(header);
    response->num_headers++;
    
    return 1;
}

/* Create authentication response with cookie */
http_response_t* create_auth_response(http_response_t* response, const char* token) {
    if (!response || !token) {
        return response;
    }

    /* Create Set-Cookie header with SameSite attribute */
    char cookie_header[512];
    snprintf(cookie_header, sizeof(cookie_header),
             "Set-Cookie: %s=%s; Path=/; Max-Age=%d; HttpOnly; SameSite=Lax",
             ADMIN_AUTH_COOKIE_NAME, token, ADMIN_AUTH_COOKIE_TTL);

    /* Add header to response */
    if (!add_response_header(response, cookie_header)) {
        printf("Failed to add cookie header to response\n");
    }

    return response;
}

/* Free HTTP response */
void free_http_response(http_response_t* response) {
    if (response) {
        if (response->body) free(response->body);
        if (response->content_type) free(response->content_type);
        
        /* Free headers */
        for (size_t i = 0; i < response->num_headers; i++) {
            if (response->headers[i]) {
                free(response->headers[i]);
            }
        }
        
        if (response->headers) {
            free(response->headers);
        }
        
        free(response);
    }
}

/* Handle client connection */
void* handle_client(void* client_data) {
    /* Validate client data */
    if (!client_data) {
        fprintf(stderr, "Error: Null client data passed to handle_client\n");
        return NULL;
    }

    client_conn_t* client = (client_conn_t*)client_data;
    int client_fd = client->client_fd;

    /* Check for valid file descriptor */
    if (client_fd <= 0) {
        fprintf(stderr, "Error: Invalid client file descriptor: %d\n", client_fd);
        return NULL;
    }

    char buffer[BUFFER_SIZE] = {0};

    /* Set socket to non-blocking */
    int flags = fcntl(client_fd, F_GETFL);
    if (flags < 0) {
        perror("fcntl get flags failed");
        close(client_fd);
        client->client_fd = 0; /* Clear FD in client struct */
        return NULL;
    }

    if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl set flags failed");
        close(client_fd);
        client->client_fd = 0;
        return NULL;
    }

    /* Set timeout */
    struct timeval tv;
    tv.tv_sec = 5;  /* 5 seconds timeout */
    tv.tv_usec = 0;
    if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) < 0) {
        perror("setsockopt failed");
        close(client_fd);
        client->client_fd = 0;
        return NULL;
    }

    /* Read request */
    int bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
    if (bytes_read <= 0) {
        if (bytes_read < 0) {
            perror("read failed");
        }
        close(client_fd);
        client->client_fd = 0;
        return NULL;
    }

    /* Null-terminate buffer */
    buffer[bytes_read] = '\0';

    /* Parse HTTP request */
    http_request_t* request = parse_http_request(buffer);
    if (!request) {
        /* Send 400 Bad Request */
        http_response_t* response = create_http_response(HTTP_BAD_REQUEST,
            "{\"error\":\"Invalid request\"}", "application/json");

        if (response) {
            char* response_str = serialize_http_response(response);

            if (response_str) {
                size_t response_len = strlen(response_str); /* Safe now with null-termination */
                if (write(client_fd, response_str, response_len) < 0) {
                    perror("write failed");
                }
                free(response_str);
            }

            free_http_response(response);
        }

        close(client_fd);
        client->client_fd = 0;
        return NULL;
    }
    
    /* Handle OPTIONS requests for CORS */
    if (request->method == HTTP_UNKNOWN && strncasecmp(buffer, "OPTIONS", 7) == 0) {
        http_response_t* response = create_http_response(HTTP_OK, NULL, "application/json");
        
        /* Apply CORS headers to OPTIONS response */
        printf("CORS: Processing OPTIONS preflight request\n");
        extern server_config_t* g_server_config;
        if (g_server_config) {
            response = apply_cors_headers(response, &g_server_config->cors, request->origin);
            
            /* Add explicit preflight headers in case we're using credentials */
            add_response_header(response, "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS");
            add_response_header(response, "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With, Accept");
            if (request->origin) {
                char origin_header[512];
                snprintf(origin_header, sizeof(origin_header), "Access-Control-Allow-Origin: %s", request->origin);
                add_response_header(response, origin_header);
                add_response_header(response, "Vary: Origin");
                add_response_header(response, "Access-Control-Allow-Credentials: true");
            } else {
                add_response_header(response, "Access-Control-Allow-Origin: *");
            }
        }
        
        char* response_str = serialize_http_response(response);
        if (response_str) {
            size_t response_len = strlen(response_str); /* Safe now with null-termination */
            write(client_fd, response_str, response_len);
            free(response_str);
        }

        free_http_response(response);
        free_http_request(request);
        close(client_fd);
        return NULL;
    }
    
    /* Debug request */
    printf("Received request: %s %s\n",
          request->method == HTTP_GET ? "GET" :
          request->method == HTTP_POST ? "POST" :
          request->method == HTTP_PUT ? "PUT" :
          request->method == HTTP_DELETE ? "DELETE" : "UNKNOWN",
          request->path);

    if (request->origin) {
        printf("Origin: %s\n", request->origin);
    }

    if (request->content_type) {
        printf("Content-Type: %s\n", request->content_type);
    }

    /* Check if this is an admin interface route */
    if (request->method == HTTP_GET && is_admin_route(request->path)) {
        http_response_t* response = NULL;

        printf("Handling admin route: %s\n", request->path);

        /* Get proper web root directory from server config */
        const char* web_root = ADMIN_FILES_DIR;
        extern server_config_t* g_server_config;
        if (g_server_config && g_server_config->web_root) {
            web_root = g_server_config->web_root;
            printf("Using configured web root: %s\n", web_root);
        } else {
            printf("Using default web root: %s\n", web_root);
        }
        
        /* Always serve static files without authentication for simplicity */
        response = serve_admin_file(request->path);
        printf("File served: %s\n", response ? "yes" : "no");

        /* Print debug info about file path if response wasn't generated */
        if (!response) {
            /* Check if web root directory exists */
            printf("Admin files directory: %s\n", web_root);
            char filepath[512] = {0};

            if (strcmp(request->path, "/") == 0) {
                sprintf(filepath, "%s/index.html", web_root);
                printf("Attempting to serve index.html from: %s\n", filepath);
            } else if (strcmp(request->path, "/login") == 0) {
                sprintf(filepath, "%s/login.html", web_root);
                printf("Attempting to serve login.html from: %s\n", filepath);
            }
        }
        
        /* Apply CORS headers to response */
        extern server_config_t* g_server_config;
        if (g_server_config) {
            response = apply_cors_headers(response, &g_server_config->cors, request->origin);
        }
        
        /* Serialize and send response */
        char* response_str = serialize_http_response(response);
        if (response_str) {
            size_t response_len = strlen(response_str); /* Safe now with null-termination */
            write(client_fd, response_str, response_len);
            free(response_str);
        }

        /* Cleanup */
        free_http_response(response);
        free_http_request(request);
        close(client_fd);

        return NULL;
    }
    
    /* Dispatch request to API handler */
    http_response_t* response = api_dispatch_request(g_api_ctx, request);
    
    /* If no response from API handler, return 404 */
    if (!response) {
        response = create_http_response(HTTP_NOT_FOUND, 
            "{\"error\":\"Not found\"}", "application/json");
    }
    
    /* Apply CORS headers to response */
    extern server_config_t* g_server_config;
    if (g_server_config) {
        response = apply_cors_headers(response, &g_server_config->cors, request->origin);
    }
    
    /* Serialize and send response */
    char* response_str = serialize_http_response(response);
    if (response_str) {
        size_t response_len = strlen(response_str); /* Safe now with null-termination */
        write(client_fd, response_str, response_len);
        free(response_str);
    }

    /* Cleanup */
    free_http_response(response);
    free_http_request(request);
    close(client_fd);
    
    return NULL;
}

/* Initialize server */
server_status_t server_init(server_config_t* config) {
    extern logger_config_t* g_logger;
    
    if (!config) {
        fprintf(stderr, "Error: Null server configuration passed to server_init\n");
        if (g_logger) {
            LOG_ERROR("Null server configuration passed to server_init");
        }
        return SERVER_ERROR;
    }

    if (g_logger) {
        LOG_INFO("Initializing server socket and resources");
    }

    /* Ensure max_connections is valid */
    if (config->max_connections <= 0) {
        fprintf(stderr, "Error: Invalid max_connections value: %d\n", config->max_connections);
        if (g_logger) {
            LOG_WARNING("Invalid max_connections value: %d, using default: %d", 
                       config->max_connections, MAX_CONNECTIONS);
        }
        config->max_connections = MAX_CONNECTIONS; /* Set to default */
    }

    /* Create socket with detailed logging */
    if (g_logger) {
        LOG_DEBUG("Creating server socket with family=AF_INET, type=SOCK_STREAM");
    }
    
    config->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (config->socket_fd < 0) {
        perror("Socket creation failed");
        if (g_logger) {
            LOG_ERROR("Socket creation failed: %s (errno=%d)", strerror(errno), errno);
        }
        return SERVER_SOCKET_ERROR;
    }
    
    if (g_logger) {
        LOG_DEBUG("Socket created successfully with fd=%d", config->socket_fd);
    }

    /* Set socket options with detailed logging */
    int opt = 1;
    int socket_error = 0;
    
    if (g_logger) {
        LOG_DEBUG("Setting socket options on fd=%d", config->socket_fd);
    }
    
    /* Set SO_REUSEADDR - critical for quick restarts */
    if (g_logger) {
        LOG_DEBUG("Setting SO_REUSEADDR socket option on fd=%d", config->socket_fd);
    }
    
    if (setsockopt(config->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        socket_error = errno;
        perror("setsockopt SO_REUSEADDR failed");
        if (g_logger) {
            LOG_ERROR("Failed to set SO_REUSEADDR: %s (errno=%d)", strerror(socket_error), socket_error);
        }
        close(config->socket_fd);
        config->socket_fd = 0;
        return SERVER_SOCKET_ERROR;
    }
    
    /* Set SO_REUSEPORT if available - important for containerized environments */
    #ifdef SO_REUSEPORT
    if (g_logger) {
        LOG_DEBUG("Setting SO_REUSEPORT socket option on fd=%d", config->socket_fd);
    }
    
    if (setsockopt(config->socket_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        socket_error = errno;
        if (g_logger) {
            LOG_WARNING("Failed to set SO_REUSEPORT: %s (errno=%d) - continuing anyway", 
                       strerror(socket_error), socket_error);
        }
        /* Non-fatal - continue execution */
    } else if (g_logger) {
        LOG_DEBUG("SO_REUSEPORT set successfully");
    }
    #endif
    
    /* Set TCP keepalive if supported */
    #ifdef SO_KEEPALIVE
    if (g_logger) {
        LOG_DEBUG("Setting SO_KEEPALIVE socket option on fd=%d", config->socket_fd);
    }
    
    if (setsockopt(config->socket_fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) < 0) {
        socket_error = errno;
        if (g_logger) {
            LOG_WARNING("Failed to set SO_KEEPALIVE: %s (errno=%d) - continuing anyway", 
                       strerror(socket_error), socket_error);
        }
        /* Non-fatal - continue execution */
    } else if (g_logger) {
        LOG_DEBUG("SO_KEEPALIVE set successfully");
    }
    #endif

    /* Try to set socket non-blocking for accept */
    #ifdef O_NONBLOCK
    if (g_logger) {
        LOG_DEBUG("Setting socket to non-blocking mode");
    }
    
    int flags = fcntl(config->socket_fd, F_GETFL, 0);
    if (flags >= 0) {
        if (fcntl(config->socket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            if (g_logger) {
                LOG_WARNING("Failed to set socket non-blocking: %s (errno=%d)", 
                           strerror(errno), errno);
            }
        } else if (g_logger) {
            LOG_DEBUG("Successfully set socket non-blocking");
        }
    }
    #endif

    /* Initialize client connections array if not already initialized */
    if (clients != NULL) {
        fprintf(stderr, "Warning: clients array already initialized. Freeing existing array.\n");
        if (g_logger) {
            LOG_WARNING("clients array already initialized. Freeing existing array.");
        }
        free(clients);
    }

    if (g_logger) {
        LOG_DEBUG("Allocating clients array for %d max connections", config->max_connections);
    }
    
    clients = (client_conn_t*)calloc(config->max_connections, sizeof(client_conn_t));
    if (!clients) {
        fprintf(stderr, "Error: Failed to allocate clients array\n");
        if (g_logger) {
            LOG_ERROR("Failed to allocate clients array: %s (errno=%d)", 
                     strerror(errno), errno);
        }
        close(config->socket_fd);
        config->socket_fd = 0;
        return SERVER_ERROR;
    }

    /* Initialize mutex */
    if (g_logger) {
        LOG_DEBUG("Initializing clients mutex");
    }
    
    int mutex_result = pthread_mutex_init(&clients_mutex, NULL);
    if (mutex_result != 0) {
        if (g_logger) {
            LOG_WARNING("Mutex initialization returned: %d (%s)", 
                       mutex_result, strerror(mutex_result));
        }
    }

    if (g_logger) {
        LOG_INFO("Server socket initialized successfully (fd=%d)", config->socket_fd);
    }
    
    return SERVER_OK;
}

/* Start server */
server_status_t server_start(server_config_t* config) {
    extern logger_config_t* g_logger;
    
    if (!config) {
        fprintf(stderr, "Error: Null server configuration passed to server_start\n");
        if (g_logger) {
            LOG_ERROR("Null server configuration passed to server_start");
        }
        return SERVER_ERROR;
    }

    /* Verify essential configuration with detailed logging */
    if (g_logger) {
        LOG_INFO("Starting server with socket FD: %d, port: %d", config->socket_fd, config->port);
    }
    
    if (config->socket_fd <= 0) {
        fprintf(stderr, "Error: Invalid socket file descriptor (%d)\n", config->socket_fd);
        if (g_logger) {
            LOG_ERROR("Invalid socket file descriptor (%d)", config->socket_fd);
        }
        return SERVER_ERROR;
    }

    if (config->port <= 0 || config->port > 65535) {
        fprintf(stderr, "Error: Invalid port number (%d)\n", config->port);
        if (g_logger) {
            LOG_ERROR("Invalid port number (%d)", config->port);
        }
        return SERVER_ERROR;
    }

    /* Create address structure */
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;

    /* Always bind to all interfaces for listening, regardless of hostname setting */
    address.sin_addr.s_addr = INADDR_ANY;

    /* Log binding information */
    if (g_logger) {
        LOG_INFO("Binding to all interfaces (0.0.0.0) regardless of hostname setting");
        if (config->host != NULL) {
            LOG_INFO("Server will be advertised as: %s", config->host);
        }
    }
    
    printf("Binding to all interfaces (0.0.0.0)\n");
    if (config->host != NULL) {
        printf("Server will be advertised as: %s\n", config->host);
    }

    /* Log the host we're advertising as */
    if (g_logger) {
        LOG_INFO("Server will be accessible at %s:%d", 
                config->host ? config->host : "0.0.0.0", config->port);
    }
    
    if (config->host != NULL) {
        printf("Server will be accessible at %s:%d\n", config->host, config->port);
    } else {
        printf("Server will be accessible at 0.0.0.0:%d\n", config->port);
    }

    /* Use the configured port from config->port */
    printf("Setting port in sin_port to %d\n", config->port);
    address.sin_port = htons(config->port);
    printf("Port set to %d in sin_port\n", config->port);
    
    /* Log socket descriptor and address information */
    if (g_logger) {
        LOG_DEBUG("Socket FD: %d, Port: %d, Address: %s", 
                 config->socket_fd, config->port, config->host ? config->host : "0.0.0.0");
        LOG_DEBUG("Address details: size=%zu, family=%d, port=%d, ip=%s",
                 sizeof(address), address.sin_family, ntohs(address.sin_port), 
                 inet_ntoa(address.sin_addr));
    }

    /* Try setting SO_REUSEADDR and SO_REUSEPORT socket options again to ensure they're set */
    int reuse = 1;
    if (g_logger) {
        LOG_DEBUG("Confirming SO_REUSEADDR socket option is set");
    }
    
    if (setsockopt(config->socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        if (g_logger) {
            LOG_WARNING("Failed to set SO_REUSEADDR: %s (errno=%d)", strerror(errno), errno);
        }
    } else if (g_logger) {
        LOG_DEBUG("Set SO_REUSEADDR successfully");
    }
    
    #ifdef SO_REUSEPORT
    if (g_logger) {
        LOG_DEBUG("Setting SO_REUSEPORT socket option");
    }
    
    if (setsockopt(config->socket_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
        if (g_logger) {
            LOG_WARNING("Failed to set SO_REUSEPORT: %s (errno=%d)", strerror(errno), errno);
        }
    } else if (g_logger) {
        LOG_DEBUG("Set SO_REUSEPORT successfully");
    }
    #endif

    /* Check if socket exists and is valid */
    if (g_logger) {
        LOG_DEBUG("Validating socket before bind operation");
    }
    
    int socket_error = 0;
    socklen_t error_len = sizeof(socket_error);
    if (getsockopt(config->socket_fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_len) < 0) {
        if (g_logger) {
            LOG_WARNING("Socket validation failed: %s (errno=%d)", strerror(errno), errno);
        }
    } else if (socket_error != 0) {
        if (g_logger) {
            LOG_WARNING("Socket has error state: %s (error=%d)", 
                       strerror(socket_error), socket_error);
        }
    } else if (g_logger) {
        LOG_DEBUG("Socket is valid before bind");
    }
    
    /* Bind socket with detailed logging and automatic recovery options */
    if (g_logger) {
        LOG_INFO("Binding socket %d to port %d", config->socket_fd, config->port);
    }
    
    /* First attempt to bind with configured port */
    int bind_result = bind(config->socket_fd, (struct sockaddr*)&address, sizeof(address));
    int bind_errno = errno;  /* Save original errno */
    
    if (g_logger) {
        LOG_DEBUG("Initial bind result: %d", bind_result);
    }
    
    /* If binding fails, try alternative approaches */
    if (bind_result < 0) {
        if (g_logger) {
            LOG_WARNING("Failed to bind socket to port %d on address %s: %s (errno=%d)", 
                     config->port, config->host ? config->host : "0.0.0.0", 
                     strerror(bind_errno), bind_errno);
        }
        
        /* Try diagnostics to understand why binding failed */
        if (bind_errno == EADDRINUSE) {
            /* Port is already in use - try to identify what's using it */
            if (g_logger) {
                LOG_WARNING("Port %d is already in use", config->port);
            }
            
            /* Try to get information about what's using the port */
            char cmd[256];
            char output[1024] = {0};
            FILE *fp;
            
            snprintf(cmd, sizeof(cmd), 
                    "ss -tuln | grep %d 2>&1 || netstat -tuln | grep %d 2>&1", 
                    config->port, config->port);
            fp = popen(cmd, "r");
            if (fp) {
                if (fread(output, 1, sizeof(output) - 1, fp) > 0) {
                    output[sizeof(output) - 1] = '\0';
                    if (g_logger) {
                        LOG_WARNING("Process using port %d: %s", config->port, output);
                    }
                    fprintf(stderr, "Port %d is already in use: %s\n", config->port, output);
                } else {
                    if (g_logger) {
                        LOG_WARNING("No process found using port %d with ss/netstat", config->port);
                    }
                    
                    /* Try lsof as well */
                    snprintf(cmd, sizeof(cmd), "lsof -i :%d 2>&1", config->port);
                    fp = popen(cmd, "r");
                    if (fp) {
                        if (fread(output, 1, sizeof(output) - 1, fp) > 0) {
                            output[sizeof(output) - 1] = '\0';
                            if (g_logger) {
                                LOG_WARNING("Process using port %d (lsof): %s", config->port, output);
                            }
                            fprintf(stderr, "Port %d is already in use (lsof): %s\n", config->port, output);
                        }
                        pclose(fp);
                    }
                }
                pclose(fp);
            }
            
            /* Try with a different port if original port is in use */
            int alternate_port = config->port + 1;
            if (alternate_port > 65535) {
                alternate_port = 8080; /* Fallback to 8080 */
            }
            
            if (g_logger) {
                LOG_INFO("Trying alternate port %d", alternate_port);
            }
            fprintf(stderr, "Trying alternate port %d...\n", alternate_port);
            
            /* Close and recreate socket to ensure clean state */
            close(config->socket_fd);
            
            config->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (config->socket_fd < 0) {
                if (g_logger) {
                    LOG_ERROR("Failed to recreate socket: %s (errno=%d)", strerror(errno), errno);
                }
                fprintf(stderr, "Error: Failed to recreate socket: %s (errno=%d)\n", strerror(errno), errno);
                return SERVER_SOCKET_ERROR;
            }
            
            /* Set socket options again */
            int socket_opt = 1;
            if (setsockopt(config->socket_fd, SOL_SOCKET, SO_REUSEADDR, &socket_opt, sizeof(socket_opt)) < 0) {
                if (g_logger) {
                    LOG_ERROR("Failed to set SO_REUSEADDR on new socket: %s (errno=%d)", 
                             strerror(errno), errno);
                }
                close(config->socket_fd);
                config->socket_fd = 0;
                return SERVER_SOCKET_ERROR;
            }
            
            #ifdef SO_REUSEPORT
            setsockopt(config->socket_fd, SOL_SOCKET, SO_REUSEPORT, &socket_opt, sizeof(socket_opt));
            #endif
            
            /* Update port in address structure */
            address.sin_port = htons(alternate_port);
            config->port = alternate_port;
            
            /* Try binding with alternate port */
            bind_result = bind(config->socket_fd, (struct sockaddr*)&address, sizeof(address));
            if (bind_result < 0) {
                if (g_logger) {
                    LOG_ERROR("Failed to bind socket to alternate port %d: %s (errno=%d)", 
                             alternate_port, strerror(errno), errno);
                }
                fprintf(stderr, "Error: Failed to bind socket to alternate port %d: %s (errno=%d)\n", 
                       alternate_port, strerror(errno), errno);
                close(config->socket_fd);
                config->socket_fd = 0;
                return SERVER_BIND_ERROR;
            }
            
            if (g_logger) {
                LOG_INFO("Successfully bound to alternate port %d", alternate_port);
            }
            fprintf(stderr, "Successfully bound to alternate port %d\n", alternate_port);
            
        } else if (bind_errno == EACCES) {
            /* Permission denied - likely trying to use a privileged port */
            if (g_logger) {
                LOG_ERROR("Permission denied binding to port %d. Need privileged access for ports < 1024.", 
                         config->port);
            }
            
            /* Try with a non-privileged port if original port is privileged */
            if (config->port < 1024) {
                int alternate_port = 8080; /* Default fallback port */
                
                if (g_logger) {
                    LOG_INFO("Trying non-privileged port %d", alternate_port);
                }
                fprintf(stderr, "Permission denied for port %d. Trying non-privileged port %d...\n", 
                       config->port, alternate_port);
                
                /* Close and recreate socket to ensure clean state */
                close(config->socket_fd);
                
                config->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
                if (config->socket_fd < 0) {
                    if (g_logger) {
                        LOG_ERROR("Failed to recreate socket: %s (errno=%d)", strerror(errno), errno);
                    }
                    fprintf(stderr, "Error: Failed to recreate socket: %s (errno=%d)\n", strerror(errno), errno);
                    return SERVER_SOCKET_ERROR;
                }
                
                /* Set socket options again */
                int socket_opt = 1;
                if (setsockopt(config->socket_fd, SOL_SOCKET, SO_REUSEADDR, &socket_opt, sizeof(socket_opt)) < 0) {
                    if (g_logger) {
                        LOG_ERROR("Failed to set SO_REUSEADDR on new socket: %s (errno=%d)", 
                                 strerror(errno), errno);
                    }
                    close(config->socket_fd);
                    config->socket_fd = 0;
                    return SERVER_SOCKET_ERROR;
                }
                
                #ifdef SO_REUSEPORT
                setsockopt(config->socket_fd, SOL_SOCKET, SO_REUSEPORT, &socket_opt, sizeof(socket_opt));
                #endif
                
                /* Update port in address structure */
                address.sin_port = htons(alternate_port);
                config->port = alternate_port;
                
                /* Try binding with non-privileged port */
                bind_result = bind(config->socket_fd, (struct sockaddr*)&address, sizeof(address));
                if (bind_result < 0) {
                    if (g_logger) {
                        LOG_ERROR("Failed to bind socket to non-privileged port %d: %s (errno=%d)", 
                                 alternate_port, strerror(errno), errno);
                    }
                    fprintf(stderr, "Error: Failed to bind socket to non-privileged port %d: %s (errno=%d)\n", 
                           alternate_port, strerror(errno), errno);
                    close(config->socket_fd);
                    config->socket_fd = 0;
                    return SERVER_BIND_ERROR;
                }
                
                if (g_logger) {
                    LOG_INFO("Successfully bound to non-privileged port %d", alternate_port);
                }
                fprintf(stderr, "Successfully bound to non-privileged port %d\n", alternate_port);
            } else {
                /* Not a privileged port but still getting EACCES - container restrictions likely */
                if (g_logger) {
                    LOG_ERROR("Permission denied binding to port %d despite being non-privileged. "
                             "This may indicate container/VM restrictions.", config->port);
                }
                fprintf(stderr, "Error: Permission denied binding to port %d despite being non-privileged. "
                       "This may indicate container/VM restrictions.\n", config->port);
                close(config->socket_fd);
                config->socket_fd = 0;
                return SERVER_BIND_ERROR;
            }
        } else {
            /* Other binding error - print and return error */
            if (g_logger) {
                LOG_ERROR("Failed to bind socket to port %d: %s (errno=%d)", 
                         config->port, strerror(bind_errno), bind_errno);
            }
            fprintf(stderr, "Error: Failed to bind socket to port %d: %s (errno=%d)\n", 
                   config->port, strerror(bind_errno), bind_errno);
            close(config->socket_fd);
            config->socket_fd = 0;
            return SERVER_BIND_ERROR;
        }
    } else {
        /* Binding successful on first attempt */
        if (g_logger) {
            LOG_INFO("Successfully bound to port %d on first attempt", config->port);
        }
    }
    
    /* Check socket state after bind */
    if (g_logger) {
        LOG_DEBUG("Validating socket after bind operation");
    }
    
    int socket_error_after_bind = 0;
    socklen_t error_len_after_bind = sizeof(socket_error_after_bind);
    if (getsockopt(config->socket_fd, SOL_SOCKET, SO_ERROR, &socket_error_after_bind, &error_len_after_bind) < 0) {
        if (g_logger) {
            LOG_WARNING("Socket validation after bind failed: %s (errno=%d)", 
                       strerror(errno), errno);
        }
    } else if (socket_error_after_bind != 0) {
        if (g_logger) {
            LOG_WARNING("Socket error after bind: %s (error=%d)", 
                       strerror(socket_error_after_bind), socket_error_after_bind);
        }
    } else if (g_logger) {
        LOG_DEBUG("Socket is valid after bind");
    }

    /* Listen for connections with detailed logging and enhanced error recovery */
    if (g_logger) {
        LOG_INFO("Setting socket %d to listen with backlog=%d", 
                config->socket_fd, config->max_connections);
    }
    
    /* Ensure max_connections is reasonable */
    int backlog = config->max_connections;
    if (backlog <= 0) {
        backlog = SOMAXCONN; /* Use system maximum if config value is invalid */
        if (g_logger) {
            LOG_WARNING("Invalid max_connections (%d), using system default SOMAXCONN", 
                      config->max_connections);
        }
    } else if (backlog > SOMAXCONN) {
        if (g_logger) {
            LOG_WARNING("Requested backlog (%d) exceeds system maximum, using SOMAXCONN instead", 
                      backlog);
        }
        backlog = SOMAXCONN;
    }
    
    /* Attempt to listen with configured backlog */
    int listen_result = listen(config->socket_fd, backlog);
    int listen_errno = errno; /* Save original errno */
    
    if (g_logger) {
        LOG_DEBUG("Listen result: %d", listen_result);
    }
    
    if (listen_result < 0) {
        /* Listen failed - detailed logging and diagnostics */
        if (g_logger) {
            LOG_ERROR("Failed to listen on socket for port %d: %s (errno=%d)", 
                     config->port, strerror(listen_errno), listen_errno);
        }
        
        fprintf(stderr, "Error: Failed to listen on socket for port %d: %s (errno=%d)\n", 
               config->port, strerror(listen_errno), listen_errno);
        
        /* Try with minimum backlog as fallback */
        if (backlog > 1) {
            if (g_logger) {
                LOG_INFO("Retrying listen with minimum backlog=1");
            }
            fprintf(stderr, "Retrying listen with minimum backlog...\n");
            
            listen_result = listen(config->socket_fd, 1);
            if (listen_result < 0) {
                if (g_logger) {
                    LOG_ERROR("Fallback listen also failed: %s (errno=%d)", 
                             strerror(errno), errno);
                }
                fprintf(stderr, "Error: Fallback listen also failed: %s (errno=%d)\n", 
                       strerror(errno), errno);
                close(config->socket_fd);
                config->socket_fd = 0;
                return SERVER_LISTEN_ERROR;
            }
            
            if (g_logger) {
                LOG_INFO("Fallback listen succeeded with backlog=1");
            }
            fprintf(stderr, "Fallback listen succeeded with minimum backlog\n");
        } else {
            /* Even minimum backlog failed - check if socket is valid and give detailed error */
            if (g_logger) {
                if (listen_errno == EBADF) {
                    LOG_ERROR("Invalid socket descriptor (%d) for listen", config->socket_fd);
                } else if (listen_errno == ENOTSOCK) {
                    LOG_ERROR("File descriptor (%d) is not a socket", config->socket_fd);
                } else if (listen_errno == EOPNOTSUPP) {
                    LOG_ERROR("Socket does not support listen operation (not a SOCK_STREAM?)");
                } else {
                    LOG_ERROR("Unknown listen error: %s (errno=%d)", 
                             strerror(listen_errno), listen_errno);
                }
            }
            
            close(config->socket_fd);
            config->socket_fd = 0;
            return SERVER_LISTEN_ERROR;
        }
    } else {
        /* Listen succeeded on first attempt */
        if (g_logger) {
            LOG_INFO("Successfully set socket %d to listening state with backlog=%d", 
                    config->socket_fd, backlog);
        }
    }
    
    /* Check socket state after listen */
    if (g_logger) {
        LOG_DEBUG("Validating socket after listen operation");
    }
    
    int socket_error_after_listen = 0;
    socklen_t error_len_after_listen = sizeof(socket_error_after_listen);
    if (getsockopt(config->socket_fd, SOL_SOCKET, SO_ERROR, &socket_error_after_listen, &error_len_after_listen) < 0) {
        if (g_logger) {
            LOG_WARNING("Socket validation after listen failed: %s (errno=%d)", 
                       strerror(errno), errno);
        }
    } else if (socket_error_after_listen != 0) {
        if (g_logger) {
            LOG_WARNING("Socket error after listen: %s (error=%d)", 
                       strerror(socket_error_after_listen), socket_error_after_listen);
        }
    } else if (g_logger) {
        LOG_DEBUG("Socket is valid after listen");
    }
    
    /* Verify listening state */
    if (g_logger) {
        LOG_DEBUG("Verifying socket is in listening state");
    }
    
    int acceptconn = 0;
    socklen_t acceptconn_len = sizeof(acceptconn);
    if (getsockopt(config->socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &acceptconn, &acceptconn_len) < 0) {
        if (g_logger) {
            LOG_WARNING("Failed to check SO_ACCEPTCONN: %s (errno=%d)", 
                       strerror(errno), errno);
        }
    } else {
        if (g_logger) {
            LOG_INFO("Socket %d listening state: %s", 
                    config->socket_fd, acceptconn ? "LISTENING" : "NOT LISTENING");
        }
        
        if (!acceptconn) {
            if (g_logger) {
                LOG_ERROR("Socket is NOT in listening state after listen() call succeeded");
            }
        }
    }
    
    /* Check if socket is visible in netstat */
    if (g_logger) {
        LOG_DEBUG("Checking if socket is visible in netstat/ss");
        
        char cmd[256];
        char output[1024] = {0};
        FILE *fp;
        
        snprintf(cmd, sizeof(cmd), 
                 "netstat -tuln | grep %d 2>/dev/null || ss -tuln | grep %d 2>/dev/null", 
                 config->port, config->port);
        
        fp = popen(cmd, "r");
        if (fp) {
            if (fread(output, 1, sizeof(output) - 1, fp) > 0) {
                output[sizeof(output) - 1] = '\0';
                LOG_DEBUG("Socket found in netstat/ss: %s", output);
            } else {
                LOG_WARNING("Socket NOT found in netstat/ss after listen");
            }
            pclose(fp);
        }
    }
    
    /* Log success message */
    if (g_logger) {
        LOG_INFO("Successfully bound to port %d and listening for connections", config->port);
    }
    printf("Successfully bound to port %d and listening for connections\n", config->port);
    
    /* Set server running flag */
    server_running = 1;
    
    /* Save socket descriptor to a static variable to prevent issues */
    static int global_socket_fd = -1;
    
    /* Save the socket descriptor to the global variable */
    global_socket_fd = config->socket_fd;
    if (g_logger) {
        LOG_DEBUG("Saved socket FD %d to global variable for persistent access",
                 config->socket_fd);
    }
           
    /* Update the socket FD in the config to ensure it's using the most current one */
    config->socket_fd = global_socket_fd;

    /* Start accept thread with detailed logging */
    if (g_logger) {
        LOG_INFO("Preparing to start server accept thread for socket %d", config->socket_fd);
    }
    
    pthread_t accept_thread;
    pthread_attr_t thread_attr;
    
    /* Initialize thread attributes with extra logging */
    if (g_logger) {
        LOG_DEBUG("Initializing thread attributes");
    }
    
    int attr_init_result = pthread_attr_init(&thread_attr);
    if (attr_init_result != 0) {
        if (g_logger) {
            LOG_ERROR("Thread attributes initialization failed: %s (errno=%d)",
                     strerror(attr_init_result), attr_init_result);
        }
    }
    
    /* Keep thread joinable for better error detection */
    if (g_logger) {
        LOG_DEBUG("Using joinable thread for better error detection");
    }
    
    /* Create a heap-allocated copy of the config for the thread */
    if (g_logger) {
        LOG_DEBUG("Allocating thread configuration");
    }
    
    server_config_t* thread_config = malloc(sizeof(server_config_t));
    if (!thread_config) {
        perror("Failed to allocate memory for thread config");
        if (g_logger) {
            LOG_ERROR("Failed to allocate memory for thread config: %s (errno=%d)",
                     strerror(errno), errno);
        }
        pthread_attr_destroy(&thread_attr);
        return SERVER_ERROR;
    }
    
    /* Copy the config with detailed logging */
    if (g_logger) {
        LOG_DEBUG("Copying server configuration for thread");
    }
    
    memcpy(thread_config, config, sizeof(server_config_t));
    
    /* Ensure the socket FD is correctly set and valid */
    thread_config->socket_fd = global_socket_fd;
    
    /* Verify socket validity before passing to thread */
    if (thread_config->socket_fd <= 0) {
        if (g_logger) {
            LOG_ERROR("Invalid socket descriptor (%d) for accept thread", thread_config->socket_fd);
        }
        fprintf(stderr, "Error: Invalid socket descriptor (%d) for accept thread\n", 
                thread_config->socket_fd);
        free(thread_config);
        pthread_attr_destroy(&thread_attr);
        return SERVER_SOCKET_ERROR;
    }
    
    /* Double-check socket state */
    int thread_socket_error = 0;
    socklen_t thread_error_len = sizeof(thread_socket_error);
    if (getsockopt(thread_config->socket_fd, SOL_SOCKET, SO_ERROR, &thread_socket_error, &thread_error_len) < 0) {
        if (g_logger) {
            LOG_ERROR("Failed to check socket state before thread creation: %s (errno=%d)",
                     strerror(errno), errno);
        }
        fprintf(stderr, "Error: Failed to check socket state before thread creation: %s (errno=%d)\n",
               strerror(errno), errno);
        free(thread_config);
        pthread_attr_destroy(&thread_attr);
        return SERVER_SOCKET_ERROR;
    }
    
    if (thread_socket_error != 0) {
        if (g_logger) {
            LOG_ERROR("Socket has error state before thread creation: %s (error=%d)",
                     strerror(thread_socket_error), thread_socket_error);
        }
        fprintf(stderr, "Error: Socket has error state before thread creation: %s (error=%d)\n",
               strerror(thread_socket_error), thread_socket_error);
        free(thread_config);
        pthread_attr_destroy(&thread_attr);
        return SERVER_SOCKET_ERROR;
    }
    
    /* Verify the socket is in listening state */
    int thread_acceptconn = 0;
    socklen_t thread_acceptconn_len = sizeof(thread_acceptconn);
    if (getsockopt(thread_config->socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &thread_acceptconn, &thread_acceptconn_len) >= 0) {
        if (thread_acceptconn == 0) {
            if (g_logger) {
                LOG_ERROR("Socket is not in listening state before thread creation");
            }
            fprintf(stderr, "Error: Socket is not in listening state before thread creation\n");
            free(thread_config);
            pthread_attr_destroy(&thread_attr);
            return SERVER_SOCKET_ERROR;
        }
    }
    
    if (g_logger) {
        LOG_DEBUG("Thread configuration prepared with socket FD: %d", thread_config->socket_fd);
    }
    
    /* Log thread creation attempt */
    if (g_logger) {
        LOG_INFO("Creating server accept thread with socket FD: %d", thread_config->socket_fd);
    }
    printf("Creating server accept thread with socket FD: %d\n", thread_config->socket_fd);
    
    /* Try creating thread with retry logic */
    int max_retries = 3;
    int retry_count = 0;
    int thread_result = -1;
    
    while (retry_count < max_retries) {
        /* Create the thread with detailed logging */
        thread_result = pthread_create(&accept_thread, &thread_attr, server_accept_loop, (void*)thread_config);
        
        if (thread_result == 0) {
            /* Thread created successfully */
            break;
        }
        
        /* Thread creation failed - log and retry */
        retry_count++;
        if (g_logger) {
            LOG_WARNING("Failed to create accept thread (attempt %d of %d): %s (errno=%d)",
                       retry_count, max_retries, strerror(thread_result), thread_result);
        }
        
        /* Small delay before retry */
        usleep(100000); /* 100ms */
    }
    
    /* Clean up thread attribute */
    pthread_attr_destroy(&thread_attr);
    
    /* Check if thread creation succeeded after retries */
    if (thread_result != 0) {
        perror("Failed to create accept thread after retries");
        fprintf(stderr, "Error creating server accept thread after %d attempts: %s (errno=%d)\n", 
               max_retries, strerror(thread_result), thread_result);
        
        if (g_logger) {
            LOG_ERROR("Failed to create server accept thread after %d attempts: %s (errno=%d)", 
                     max_retries, strerror(thread_result), thread_result);
            
            /* Try to get system resource information */
            char cmd[256];
            char output[1024] = {0};
            FILE *fp;
            
            LOG_DEBUG("Checking system resource limits");
            snprintf(cmd, sizeof(cmd), "ulimit -a 2>&1");
            fp = popen(cmd, "r");
            if (fp) {
                if (fread(output, 1, sizeof(output) - 1, fp) > 0) {
                    output[sizeof(output) - 1] = '\0';
                    LOG_DEBUG("System resource limits: %s", output);
                }
                pclose(fp);
            }
            
            /* Check thread count */
            snprintf(cmd, sizeof(cmd), "ps -eLf | wc -l");
            fp = popen(cmd, "r");
            if (fp) {
                if (fread(output, 1, sizeof(output) - 1, fp) > 0) {
                    output[sizeof(output) - 1] = '\0';
                    LOG_DEBUG("Current thread count: %s", output);
                }
                pclose(fp);
            }
        }
        
        free(thread_config);
        return SERVER_THREAD_ERROR;
    }
    
    /* Detach thread explicitly with detailed logging */
    if (g_logger) {
        LOG_DEBUG("Detaching server accept thread (tid: %lu)", (unsigned long)accept_thread);
    }
    
    int detach_result = pthread_detach(accept_thread);
    if (detach_result != 0) {
        if (g_logger) {
            LOG_WARNING("Failed to detach thread: %s (errno=%d)", 
                       strerror(detach_result), detach_result);
        }
    }
    
    /* Check if thread is running by sleeping briefly */
    if (g_logger) {
        LOG_DEBUG("Waiting for thread to initialize (sleeping 100ms)");
    }
    
    usleep(100000); /* 100ms sleep to let thread start */
    
    if (!server_running) {
        if (g_logger) {
            LOG_ERROR("Server thread created but server_running flag not set. Thread may have exited.");
        }
        fprintf(stderr, "Error: Server thread created but exited immediately\n");
        return SERVER_THREAD_ERROR;
    }
    
    /* Verify thread is running */
    if (g_logger) {
        LOG_INFO("Server accept thread (tid: %lu) created and running successfully", 
                (unsigned long)accept_thread);
    }
    printf("Server accept thread created successfully (tid: %lu)\n", (unsigned long)accept_thread);
    
    return SERVER_OK;
}

/* Accept connections loop */
void* server_accept_loop(void* config_ptr) {
    extern logger_config_t* g_logger;
    
    if (g_logger) {
        LOG_INFO("Starting server accept thread");
    }
    
    server_config_t* config = (server_config_t*)config_ptr;
    
    /* Check arguments with detailed logging */
    if (!config) {
        fprintf(stderr, "Error: NULL server configuration passed to accept loop\n");
        if (g_logger) {
            LOG_ERROR("NULL server configuration passed to accept loop");
        }
        server_running = 0; /* Clear running flag so main thread can detect failure */
        return NULL;
    }
    
    /* Verify socket with detailed logging */
    if (config->socket_fd <= 0) {
        fprintf(stderr, "Error: Invalid socket file descriptor (%d) in accept loop\n", config->socket_fd);
        if (g_logger) {
            LOG_ERROR("Invalid socket file descriptor (%d) in accept loop", config->socket_fd);
        }
        server_running = 0; /* Clear running flag so main thread can detect failure */
        return NULL;
    }
    
    /* Set server running flag to indicate thread has started */
    server_running = 1;
    
    if (g_logger) {
        LOG_DEBUG("Server running flag set to indicate thread has started");
    }
    
    /* Check socket state before entering accept loop */
    if (g_logger) {
        LOG_DEBUG("Checking socket state before entering accept loop");
    }
    
    int socket_error = 0;
    socklen_t error_len = sizeof(socket_error);
    if (getsockopt(config->socket_fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_len) < 0) {
        fprintf(stderr, "Error: Failed to check socket state in accept loop: %s (errno=%d)\n", 
               strerror(errno), errno);
        if (g_logger) {
            LOG_ERROR("Failed to check socket state in accept loop: %s (errno=%d)", 
                     strerror(errno), errno);
        }
        server_running = 0;
        return NULL;
    }
    
    if (socket_error != 0) {
        fprintf(stderr, "Error: Socket has error state in accept loop: %s (error=%d)\n", 
               strerror(socket_error), socket_error);
        if (g_logger) {
            LOG_ERROR("Socket has error state in accept loop: %s (error=%d)", 
                     strerror(socket_error), socket_error);
        }
        server_running = 0;
        return NULL;
    }
    
    if (g_logger) {
        LOG_DEBUG("Socket state is valid before entering accept loop");
    }
    
    /* Log thread start with detailed information */
    printf("Starting server accept loop on socket %d for port %d\n", 
           config->socket_fd, config->port);
    
    if (g_logger) {
        LOG_INFO("Starting server accept loop on socket %d for port %d (thread %lu)", 
                config->socket_fd, config->port, (unsigned long)pthread_self());
    }
    
    /* Check if socket is truly listening with detailed logging */
    if (g_logger) {
        LOG_DEBUG("Verifying socket is in LISTENING state");
    }
    
    int socket_status;
    socklen_t status_len = sizeof(socket_status);
    if (getsockopt(config->socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &socket_status, &status_len) < 0) {
        fprintf(stderr, "Error: Failed to check if socket is in listening state: %s (errno=%d)\n", 
               strerror(errno), errno);
        if (g_logger) {
            LOG_ERROR("Failed to check if socket is in listening state: %s (errno=%d)", 
                     strerror(errno), errno);
        }
    } else if (socket_status == 0) {
        fprintf(stderr, "Error: Socket is not in listening state\n");
        if (g_logger) {
            LOG_ERROR("Socket is not in listening state");
        }
        server_running = 0;
        return NULL;
    } else {
        printf("Socket %d is confirmed to be in listening state\n", config->socket_fd);
        if (g_logger) {
            LOG_INFO("Socket %d is confirmed to be in listening state", config->socket_fd);
        }
    }
    
    /* Try to show socket status in netstat with detailed logging */
    if (g_logger) {
        LOG_DEBUG("Checking socket visibility in netstat/ss");
    }
    
    char cmd[256];
    char output[1024] = {0};
    FILE *fp;
    
    printf("Checking if socket is visible in netstat/ss\n");
    snprintf(cmd, sizeof(cmd), 
             "netstat -tuln | grep %d 2>/dev/null || ss -tuln | grep %d 2>/dev/null", 
             config->port, config->port);
    
    fp = popen(cmd, "r");
    if (fp) {
        if (fread(output, 1, sizeof(output) - 1, fp) > 0) {
            output[sizeof(output) - 1] = '\0';
            printf("Socket found in netstat/ss: %s\n", output);
            if (g_logger) {
                LOG_INFO("Socket found in netstat/ss: %s", output);
            }
        } else {
            printf("Socket NOT found in netstat/ss after listen!\n");
            if (g_logger) {
                LOG_WARNING("Socket NOT found in netstat/ss after listen!");
                
                /* Try with lsof as well */
                snprintf(cmd, sizeof(cmd), "lsof -i :%d 2>/dev/null", config->port);
                fp = popen(cmd, "r");
                if (fp) {
                    if (fread(output, 1, sizeof(output) - 1, fp) > 0) {
                        LOG_INFO("Socket found with lsof: %s", output);
                    } else {
                        LOG_WARNING("Socket NOT found with lsof either");
                    }
                    pclose(fp);
                }
            }
        }
        pclose(fp);
    }
    
    /* Initialize client connection variables with detailed logging */
    if (g_logger) {
        LOG_DEBUG("Initializing client connection variables");
    }
    
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int loop_count = 0;
    int total_connections = 0;
    time_t last_activity_check = time(NULL);
    time_t last_health_log = time(NULL);
    
    /* Log that we're entering the accept loop */
    if (g_logger) {
        LOG_INFO("Entering accept loop to wait for connections");
    }
    
    /* Accept connections in a loop with detailed logging */
    while (server_running) {
        /* Periodically log health info (every 5 minutes) */
        time_t current_time = time(NULL);
        if (current_time - last_health_log > 300) { /* 5 minutes */
            if (g_logger) {
                LOG_INFO("Accept loop health check - Socket: %d, Total connections: %d", 
                        config->socket_fd, total_connections);
                
                /* Check if socket is still valid */
                int socket_error = 0;
                socklen_t error_len = sizeof(socket_error);
                if (getsockopt(config->socket_fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_len) < 0) {
                    LOG_WARNING("Socket validation failed in health check: %s (errno=%d)", 
                               strerror(errno), errno);
                } else if (socket_error != 0) {
                    LOG_WARNING("Socket has error state in health check: %s (error=%d)", 
                               strerror(socket_error), socket_error);
                } else {
                    LOG_DEBUG("Socket is valid in health check");
                }
            }
            last_health_log = current_time;
        }
        
        /* Accept connection with explicit timeout */
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(config->socket_fd, &read_fds);
        
        /* Set timeout to 1 second so we can periodically check server_running */
        struct timeval timeout;
        timeout.tv_sec = 1;  /* 1 second timeout */
        timeout.tv_usec = 0;
        
        /* Wait for activity or timeout with detailed logging */
        if (g_logger && (current_time - last_activity_check > 60)) { /* Log once a minute */
            LOG_DEBUG("Waiting for connection activity (select with 1s timeout)");
            last_activity_check = current_time;
        }
        
        int select_result = select(config->socket_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        /* Check for select error with detailed logging */
        if (select_result < 0) {
            if (errno == EINTR) {
                /* Interrupted by signal, just continue */
                if (g_logger) {
                    LOG_DEBUG("Select interrupted by signal, continuing");
                }
                continue;
            }
            
            /* Log other select errors with detailed information */
            fprintf(stderr, "Select failed in accept loop: %s (errno=%d)\n", 
                   strerror(errno), errno);
            if (g_logger) {
                LOG_ERROR("Select failed in accept loop: %s (errno=%d)", 
                         strerror(errno), errno);
            }
            
            /* If socket is invalid, exit thread */
            if (errno == EBADF) {
                fprintf(stderr, "Socket is invalid (bad file descriptor), exiting accept loop\n");
                if (g_logger) {
                    LOG_ERROR("Socket is invalid (bad file descriptor), exiting accept loop");
                }
                server_running = 0;
                break;
            }
            
            /* Otherwise continue and try again */
            continue;
        }
        
        /* Check if timeout (no activity) */
        if (select_result == 0) {
            /* Just timeout, continue with next iteration */
            continue;
        }
        
        /* Have activity, try to accept with detailed logging */
        if (g_logger) {
            LOG_DEBUG("Activity detected on socket %d, calling accept()", config->socket_fd);
        }
        
        int client_fd = accept(config->socket_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            /* Accept failed with detailed logging */
            if (loop_count % 100 == 0) {  /* Log only every 100 iterations to reduce log spam */
                fprintf(stderr, "Accept failed: %s (errno=%d)\n", strerror(errno), errno);
                if (g_logger) {
                    LOG_WARNING("Failed to accept connection: %s (errno=%d)", 
                               strerror(errno), errno);
                }
                
                /* Check if socket is still valid with detailed logging */
                int socket_error = 0;
                socklen_t error_len = sizeof(socket_error);
                if (getsockopt(config->socket_fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_len) < 0) {
                    fprintf(stderr, "Socket validation failed: %s (errno=%d)\n", 
                           strerror(errno), errno);
                    if (g_logger) {
                        LOG_ERROR("Socket validation failed: %s (errno=%d)", 
                                 strerror(errno), errno);
                    }
                } else if (socket_error != 0) {
                    fprintf(stderr, "Socket has error state: %s (error=%d)\n", 
                           strerror(socket_error), socket_error);
                    if (g_logger) {
                        LOG_ERROR("Socket has error state: %s (error=%d)", 
                                 strerror(socket_error), socket_error);
                    }
                }
            }
            
            loop_count++;
            continue;
        }
        
        /* Connection accepted successfully with detailed logging! */
        total_connections++;
        printf("Accepted new client connection on fd %d from %s:%d (total: %d)\n", 
              client_fd, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), 
              total_connections);
        if (g_logger) {
            LOG_INFO("Accepted new client connection on fd %d from %s:%d (total: %d)", 
                    client_fd, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port),
                    total_connections);
        }
        
        /* Reset loop counter when we get a valid connection */
        loop_count = 0;
        
        /* Find free client slot with detailed logging */
        if (g_logger) {
            LOG_DEBUG("Finding free client slot");
        }
        
        int slot = -1;
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < config->max_connections; i++) {
            if (clients[i].client_fd == 0) {
                slot = i;
                clients[slot].client_fd = client_fd;
                clients[slot].address = client_addr;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);
        
        if (slot == -1) {
            /* No free slots, close connection with detailed logging */
            fprintf(stderr, "No free client slots, rejecting connection\n");
            if (g_logger) {
                LOG_WARNING("No free client slots (max=%d), rejecting connection", 
                           config->max_connections);
            }
            close(client_fd);
            continue;
        }
        
        if (g_logger) {
            LOG_DEBUG("Assigned client to slot %d", slot);
        }
        
        /* Create thread to handle client with detailed logging */
        if (g_logger) {
            LOG_DEBUG("Creating client handler thread for connection in slot %d", slot);
        }
        
        int thread_result = pthread_create(&clients[slot].thread, NULL, handle_client, &clients[slot]);
        if (thread_result != 0) {
            fprintf(stderr, "Failed to create client handler thread: %s (errno=%d)\n", 
                   strerror(thread_result), thread_result);
            if (g_logger) {
                LOG_ERROR("Failed to create client handler thread: %s (errno=%d)", 
                         strerror(thread_result), thread_result);
            }
            
            pthread_mutex_lock(&clients_mutex);
            clients[slot].client_fd = 0;
            pthread_mutex_unlock(&clients_mutex);
            close(client_fd);
            continue;
        }
        
        /* Detach thread with detailed logging */
        if (g_logger) {
            LOG_DEBUG("Detaching client handler thread");
        }
        
        int detach_result = pthread_detach(clients[slot].thread);
        if (detach_result != 0) {
            if (g_logger) {
                LOG_WARNING("Failed to detach client thread: %s (errno=%d)", 
                           strerror(detach_result), detach_result);
            }
        }
    }
    
    /* Log thread exit with detailed information */
    printf("Server accept loop exiting\n");
    if (g_logger) {
        LOG_INFO("Server accept loop exiting (handled %d connections total)", 
                total_connections);
    }
    
    /* Free config memory with logging */
    if (g_logger) {
        LOG_DEBUG("Freeing thread configuration memory");
    }
    
    free(config_ptr);
    
    return NULL;
}

/* Stop server */
void server_stop(server_config_t* config) {
    if (!config) {
        fprintf(stderr, "Warning: Attempt to stop NULL server config\n");
        return;
    }

    /* Set server stop flag */
    server_running = 0;

    /* Close server socket */
    if (config->socket_fd > 0) {
        close(config->socket_fd);
        config->socket_fd = 0;
    }

    /* Wait for client threads to finish */
    usleep(100000);  /* Sleep for 100ms */

    /* Close client connections if clients array exists */
    if (clients != NULL) {
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < config->max_connections; i++) {
            if (clients[i].client_fd > 0) {
                close(clients[i].client_fd);
                clients[i].client_fd = 0;
            }
        }
        pthread_mutex_unlock(&clients_mutex);

        /* Free clients array */
        free(clients);
        clients = NULL;
    }
}

/* Check if path is an admin route */
int is_admin_route(const char* path) {
    /* Root path redirects to admin */
    if (strcmp(path, "/") == 0) {
        return 1;
    }
    
    /* Login path is not protected */
    if (strcmp(path, "/login") == 0) {
        return 0;
    }
    
    /* Check for admin paths */
    return (strncmp(path, "/admin", 6) == 0) || 
           (strncmp(path, "/css/", 5) == 0) || 
           (strncmp(path, "/js/", 4) == 0);
}

/* Generate a token for admin authentication */
char* generate_admin_auth_token(const char* username) {
    if (!username) {
        return NULL;
    }
    
    /* Create token with format: username:timestamp:random */
    char token[256];
    snprintf(token, sizeof(token), "%s:%lu:%d", 
             username, (unsigned long)time(NULL), rand() % 10000);
    
    /* Encode token (simple base64 would be better, but we'll use a basic encoding for now) */
    char* encoded = (char*)malloc(strlen(token) * 2 + 1);
    if (!encoded) {
        return NULL;
    }
    
    /* Simple encoding (not secure, but sufficient for demo) */
    for (size_t i = 0; i < strlen(token); i++) {
        sprintf(encoded + (i * 2), "%02x", (unsigned char)token[i]);
    }
    
    return encoded;
}

/* Check if request has valid admin authentication */
int check_admin_auth(http_request_t* request) {
    if (!request) {
        return 0;
    }
    
    /* Check for admin auth cookie */
    char* token = get_cookie_value(request, ADMIN_AUTH_COOKIE_NAME);
    if (!token) {
        return 0;
    }
    
    /* In a real implementation, you would validate the token here */
    /* For demo purposes, just check if the token exists and is not empty */
    return *token != '\0';
}

/* Get file extension from path */
const char* get_file_extension(const char* filename) {
    if (!filename) {
        return "";
    }
    
    const char* dot = strrchr(filename, '.');
    if (!dot || dot == filename) {
        return "";
    }
    
    return dot + 1;
}

/* Get content type from file extension */
const char* get_content_type_from_extension(const char* extension) {
    if (!extension) {
        return "application/octet-stream";
    }
    
    /* Convert to lowercase for comparison */
    char ext_lower[32] = {0};
    size_t i;
    for (i = 0; i < sizeof(ext_lower) - 1 && extension[i]; i++) {
        ext_lower[i] = tolower(extension[i]);
    }
    ext_lower[i] = '\0';
    
    /* Map extensions to content types */
    if (strcmp(ext_lower, "html") == 0) {
        return "text/html";
    } else if (strcmp(ext_lower, "css") == 0) {
        return "text/css";
    } else if (strcmp(ext_lower, "js") == 0) {
        return "application/javascript";
    } else if (strcmp(ext_lower, "json") == 0) {
        return "application/json";
    } else if (strcmp(ext_lower, "png") == 0) {
        return "image/png";
    } else if (strcmp(ext_lower, "jpg") == 0 || strcmp(ext_lower, "jpeg") == 0) {
        return "image/jpeg";
    } else if (strcmp(ext_lower, "gif") == 0) {
        return "image/gif";
    } else if (strcmp(ext_lower, "svg") == 0) {
        return "image/svg+xml";
    } else if (strcmp(ext_lower, "ico") == 0) {
        return "image/x-icon";
    } else {
        return "application/octet-stream";
    }
}

/* Read file content from filesystem */
char* read_file_content(const char* filepath, size_t* size) {
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        if (size) *size = 0;
        return NULL;
    }
    
    /* Get file size */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        if (size) *size = 0;
        return NULL;
    }
    
    /* Allocate buffer */
    char* buffer = (char*)malloc(file_size + 1);
    if (!buffer) {
        fclose(file);
        if (size) *size = 0;
        return NULL;
    }
    
    /* Read file content */
    size_t bytes_read = fread(buffer, 1, file_size, file);
    fclose(file);
    
    if (bytes_read != (size_t)file_size) {
        free(buffer);
        if (size) *size = 0;
        return NULL;
    }
    
    /* Null-terminate buffer (for text files) */
    buffer[file_size] = '\0';
    
    if (size) *size = file_size;
    return buffer;
}

/* Serve admin file from filesystem */
http_response_t* serve_admin_file(const char* path) {
    /* Get proper web root directory from server config */
    const char* web_root = ADMIN_FILES_DIR;
    extern server_config_t* g_server_config;
    if (g_server_config && g_server_config->web_root) {
        web_root = g_server_config->web_root;
    }

    /* Default path (root) to index.html */
    char filepath[512] = {0};
    
    if (strcmp(path, "/") == 0) {
        sprintf(filepath, "%s/index.html", web_root);
    } else if (strcmp(path, "/login") == 0) {
        sprintf(filepath, "%s/login.html", web_root);
    } else if (strncmp(path, "/admin", 6) == 0) {
        /* Handle /admin prefix redirects */
        if (strcmp(path, "/admin") == 0 || strcmp(path, "/admin/") == 0) {
            sprintf(filepath, "%s/index.html", web_root);
        } else {
            sprintf(filepath, "%s%s", web_root, path + 6);
        }
    } else {
        /* Handle CSS, JS, and other assets */
        sprintf(filepath, "%s%s", web_root, path);
    }
    
    /* Check if file exists */
    struct stat file_stat;
    if (stat(filepath, &file_stat) != 0 || !S_ISREG(file_stat.st_mode)) {
        /* File not found, try index.html for SPA routing */
        if (strncmp(path, "/admin", 6) == 0 || 
            strchr(path + 1, '.') == NULL) { /* If no file extension, likely a route */
            sprintf(filepath, "%s/index.html", web_root);
            
            /* Check if index.html exists */
            if (stat(filepath, &file_stat) != 0 || !S_ISREG(file_stat.st_mode)) {
                return create_http_response(HTTP_NOT_FOUND, 
                                          "{\"error\":\"File not found\"}", "application/json");
            }
        } else {
            return create_http_response(HTTP_NOT_FOUND, 
                                       "{\"error\":\"File not found\"}", "application/json");
        }
    }
    
    /* Read file content */
    size_t file_size = 0;
    char* file_content = read_file_content(filepath, &file_size);
    
    if (!file_content) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to read file\"}", "application/json");
    }
    
    /* Get content type from file extension */
    const char* extension = get_file_extension(filepath);
    const char* content_type = get_content_type_from_extension(extension);
    
    /* Create response with file content */
    http_response_t* response = create_http_response(HTTP_OK, file_content, content_type);
    
    /* Clean up */
    free(file_content);
    
    return response;
}