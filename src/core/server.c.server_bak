#include "jsondb/core/server.h"
#include "jsondb/api/api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
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
    if (n < 0 || n >= remaining) {
        free(response_str);
        return NULL;
    }
    written += n;
    remaining -= n;

    /* Headers */
    n = snprintf(response_str + written, remaining, "Content-Type: %s\r\n", response->content_type);
    if (n < 0 || n >= remaining) {
        free(response_str);
        return NULL;
    }
    written += n;
    remaining -= n;

    n = snprintf(response_str + written, remaining, "Content-Length: %zu\r\n", response->content_length);
    if (n < 0 || n >= remaining) {
        free(response_str);
        return NULL;
    }
    written += n;
    remaining -= n;

    n = snprintf(response_str + written, remaining, "Connection: close\r\n");
    if (n < 0 || n >= remaining) {
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
            if (n < 0 || n >= remaining) {
                free(response_str);
                return NULL;
            }
            written += n;
            remaining -= n;
        }
    }

    /* End of headers */
    n = snprintf(response_str + written, remaining, "\r\n");
    if (n < 0 || n >= remaining) {
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

    /* Create Set-Cookie header */
    char cookie_header[512];
    snprintf(cookie_header, sizeof(cookie_header),
             "Set-Cookie: %s=%s; Path=/; Max-Age=%d; HttpOnly",
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

        /* Always serve static files without authentication for simplicity */
        response = serve_admin_file(request->path);
        printf("File served: %s\n", response ? "yes" : "no");

        /* Print debug info about file path if response wasn't generated */
        if (!response) {
            /* Check if ADMIN_FILES_DIR exists */
            printf("Admin files directory: %s\n", ADMIN_FILES_DIR);
            char filepath[512] = {0};

            if (strcmp(request->path, "/") == 0) {
                sprintf(filepath, "%s/index.html", ADMIN_FILES_DIR);
                printf("Attempting to serve index.html from: %s\n", filepath);
            } else if (strcmp(request->path, "/login") == 0) {
                sprintf(filepath, "%s/login.html", ADMIN_FILES_DIR);
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
    if (!config) {
        fprintf(stderr, "Error: Null server configuration passed to server_init\n");
        return SERVER_ERROR;
    }

    /* Ensure max_connections is valid */
    if (config->max_connections <= 0) {
        fprintf(stderr, "Error: Invalid max_connections value: %d\n", config->max_connections);
        config->max_connections = MAX_CONNECTIONS; /* Set to default */
    }

    /* Create socket */
    config->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (config->socket_fd < 0) {
        perror("Socket creation failed");
        return SERVER_SOCKET_ERROR;
    }

    /* Set socket options */
    int opt = 1;
    if (setsockopt(config->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(config->socket_fd);
        config->socket_fd = 0;
        return SERVER_SOCKET_ERROR;
    }

    /* Initialize client connections array if not already initialized */
    if (clients != NULL) {
        fprintf(stderr, "Warning: clients array already initialized. Freeing existing array.\n");
        free(clients);
    }

    clients = (client_conn_t*)calloc(config->max_connections, sizeof(client_conn_t));
    if (!clients) {
        fprintf(stderr, "Error: Failed to allocate clients array\n");
        close(config->socket_fd);
        config->socket_fd = 0;
        return SERVER_ERROR;
    }

    /* Initialize mutex */
    pthread_mutex_init(&clients_mutex, NULL);

    return SERVER_OK;
}

/* Start server */
server_status_t server_start(server_config_t* config) {
    if (!config) {
        fprintf(stderr, "Error: Null server configuration passed to server_start\n");
        return SERVER_ERROR;
    }

    /* Verify essential configuration */
    if (config->socket_fd <= 0) {
        fprintf(stderr, "Error: Invalid socket file descriptor (%d)\n", config->socket_fd);
        return SERVER_ERROR;
    }

    if (config->port <= 0) {
        fprintf(stderr, "Error: Invalid port number (%d)\n", config->port);
        return SERVER_ERROR;
    }

    /* Create address structure */
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;

    /* Bind to any address for best compatibility */
    address.sin_addr.s_addr = INADDR_ANY;

    /* Log the host we're advertising as */
    if (config->host != NULL) {
        printf("Server will be accessible at %s:%d\n", config->host, config->port);
    } else {
        printf("Server will be accessible at localhost:%d\n", config->port);
    }

    address.sin_port = htons(config->port);
    
    /* Bind socket */
    if (bind(config->socket_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        return SERVER_BIND_ERROR;
    }
    
    /* Listen for connections */
    if (listen(config->socket_fd, config->max_connections) < 0) {
        return SERVER_LISTEN_ERROR;
    }
    
    /* Set server running flag */
    server_running = 1;
    
    /* Accept connections in a loop */
    pthread_t accept_thread;
    if (pthread_create(&accept_thread, NULL, server_accept_loop, (void*)config) != 0) {
        return SERVER_THREAD_ERROR;
    }
    
    pthread_detach(accept_thread);
    
    return SERVER_OK;
}

/* Accept connections loop */
void* server_accept_loop(void* config_ptr) {
    server_config_t* config = (server_config_t*)config_ptr;
    
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    while (server_running) {
        /* Accept connection */
        int client_fd = accept(config->socket_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            /* Accept failed, try again */
            usleep(10000);  /* Sleep for 10ms */
            continue;
        }
        
        /* Find free client slot */
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
            /* No free slots, close connection */
            close(client_fd);
            continue;
        }
        
        /* Create thread to handle client */
        if (pthread_create(&clients[slot].thread, NULL, handle_client, &clients[slot]) != 0) {
            pthread_mutex_lock(&clients_mutex);
            clients[slot].client_fd = 0;
            pthread_mutex_unlock(&clients_mutex);
            close(client_fd);
            continue;
        }
        
        /* Detach thread */
        pthread_detach(clients[slot].thread);
    }
    
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
    /* Default path (root) to index.html */
    char filepath[512] = {0};
    
    if (strcmp(path, "/") == 0) {
        sprintf(filepath, "%s/index.html", ADMIN_FILES_DIR);
    } else if (strcmp(path, "/login") == 0) {
        sprintf(filepath, "%s/login.html", ADMIN_FILES_DIR);
    } else if (strncmp(path, "/admin", 6) == 0) {
        /* Handle /admin prefix redirects */
        if (strcmp(path, "/admin") == 0 || strcmp(path, "/admin/") == 0) {
            sprintf(filepath, "%s/index.html", ADMIN_FILES_DIR);
        } else {
            sprintf(filepath, "%s%s", ADMIN_FILES_DIR, path + 6);
        }
    } else {
        /* Handle CSS, JS, and other assets */
        sprintf(filepath, "%s%s", ADMIN_FILES_DIR, path);
    }
    
    /* Check if file exists */
    struct stat file_stat;
    if (stat(filepath, &file_stat) != 0 || !S_ISREG(file_stat.st_mode)) {
        /* File not found, try index.html for SPA routing */
        if (strncmp(path, "/admin", 6) == 0 || 
            strchr(path + 1, '.') == NULL) { /* If no file extension, likely a route */
            sprintf(filepath, "%s/index.html", ADMIN_FILES_DIR);
            
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