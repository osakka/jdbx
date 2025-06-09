#include "core/server.h"
#include "api/api.h"
#include "utils/metrics.h"
#include "utils/ssl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

/* Forward declarations for SSL support */
static int client_read_data(client_conn_t* client, char* buffer, size_t buffer_size);
static int client_write_data(client_conn_t* client, const char* data, size_t data_len);
static int client_setup_ssl(client_conn_t* client);
static void client_cleanup_ssl(client_conn_t* client);

/**
 * Read data from client connection (SSL or plain socket)
 * @param client Client connection
 * @param buffer Buffer to store data
 * @param buffer_size Size of buffer
 * @return Number of bytes read, or -1 on error
 */
static int client_read_data(client_conn_t* client, char* buffer, size_t buffer_size) {
  if (!client || !buffer || buffer_size == 0) {
    return -1;
  }
  
  if (client->use_ssl && client->ssl_conn) {
    /* SSL read */
    size_t bytes_read = 0;
    ssl_error_t error = ssl_read(client->ssl_conn, buffer, buffer_size - 1, &bytes_read);
    
    if (error != SSL_SUCCESS) {
      if (g_logger) {
        LOG_ERROR("SSL read failed: %s", ssl_error_string(error));
      }
      return -1;
    }
    
    return (int)bytes_read;
  } else {
    /* Plain socket read */
    return read(client->client_fd, buffer, buffer_size - 1);
  }
}

/**
 * Write data to client connection (SSL or plain socket)
 * @param client Client connection
 * @param data Data to write
 * @param data_len Length of data
 * @return Number of bytes written, or -1 on error
 */
static int client_write_data(client_conn_t* client, const char* data, size_t data_len) {
  if (!client || !data || data_len == 0) {
    return -1;
  }
  
  if (client->use_ssl && client->ssl_conn) {
    /* SSL write */
    size_t bytes_written = 0;
    ssl_error_t error = ssl_write(client->ssl_conn, data, data_len, &bytes_written);
    
    if (error != SSL_SUCCESS) {
      if (g_logger) {
        LOG_ERROR("SSL write failed: %s", ssl_error_string(error));
      }
      return -1;
    }
    
    return (int)bytes_written;
  } else {
    /* Plain socket write */
    return write(client->client_fd, data, data_len);
  }
}

/**
 * Set up SSL connection for client
 * @param client Client connection
 * @return 0 on success, -1 on failure
 */
static int client_setup_ssl(client_conn_t* client) {
  if (!client) {
    return -1;
  }
  
  /* Get SSL context from server */
  ssl_context_t* ssl_ctx = server_get_ssl_context();
  if (!ssl_ctx) {
    if (g_logger) {
      LOG_ERROR("No SSL context available for client connection.");
    }
    return -1;
  }
  
  /* Create SSL connection */
  ssl_error_t error = ssl_connection_create(ssl_ctx, client->client_fd, &client->ssl_conn);
  if (error != SSL_SUCCESS) {
    if (g_logger) {
      LOG_ERROR("Cannot create SSL connection: %s", ssl_error_string(error));
    }
    return -1;
  }
  
  /* Perform SSL handshake */
  error = ssl_handshake(client->ssl_conn);
  if (error != SSL_SUCCESS) {
    if (g_logger) {
      LOG_ERROR("SSL handshake failed: %s", ssl_error_string(error));
    }
    ssl_connection_free(client->ssl_conn);
    client->ssl_conn = NULL;
    return -1;
  }
  
  if (g_logger) {
    LOG_DEBUG("SSL connection established for client fd=%d", client->client_fd);
  }
  
  return 0;
}

/**
 * Clean up SSL connection for client
 * @param client Client connection
 */
static void client_cleanup_ssl(client_conn_t* client) {
  if (client && client->ssl_conn) {
    if (g_logger) {
      LOG_DEBUG("Cleaning up SSL connection for client fd=%d", client->client_fd);
    }
    ssl_connection_free(client->ssl_conn);
    client->ssl_conn = NULL;
  }
}

/* Handle client connection */
void handle_client(void* client_data) {
  /* Ultra-early crash detection logging */
  if (g_logger) {
    TRACE_NET("HANDLE_CLIENT_ENTRY: client_data=%p", client_data);
  }
  
  /* Get start time for performance tracking */
  struct timespec start_time, end_time;
  if (g_logger) {
    TRACE_NET("HANDLE_CLIENT_CLOCK_START.");
  }
  clock_gettime(CLOCK_MONOTONIC, &start_time);
  if (g_logger) {
    TRACE_NET("HANDLE_CLIENT_CLOCK_SUCCESS.");
  }
  
  /* Start request timer for metrics */
  timer_context_t* request_timer = NULL;
  metric_t* request_duration_metric = get_server_request_duration_metric();
  if (request_duration_metric) {
    request_timer = metrics_timer_start(request_duration_metric);
  }
  
  /* Increment active connections */
  metric_t* active_connections = get_active_connections_metric();
  if (active_connections) {
    metrics_gauge_inc(active_connections, 1.0);
  }
  
  /* Get thread ID for logging */
  pthread_t tid = pthread_self();
  pid_t system_tid = (pid_t)syscall(SYS_gettid);
  
  /* Validate client data */
  if (!client_data) {
    if (g_logger) {
      LOG_ERROR("Null client data passed to handle_client (thread=%lu, tid=%d)", 
           (unsigned long)tid, system_tid);
    } else {
      fprintf(stderr, "Error: Null client data passed to handle_client\n");
    }
    goto cleanup;
  }

  client_conn_t* client = (client_conn_t*)client_data;
  int client_fd = client->client_fd;
  
  /* Get client address for detailed logging */
  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  char client_ip[INET_ADDRSTRLEN] = "unknown";
  int client_port = 0;
  
  if (getpeername(client_fd, (struct sockaddr*)&client_addr, &client_addr_len) == 0) {
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    client_port = ntohs(client_addr.sin_port);
  }

  /* Comprehensive connection lifecycle logging */
  if (g_logger) {
    LOG_INFO("Connection started - fd=%d, thread=%lu, tid=%d, client=%s:%d, ssl=%s", 
        client_fd, (unsigned long)tid, system_tid, client_ip, client_port,
        client->use_ssl ? "enabled" : "disabled");
    TRACE_NET("CONNECTION_DETAILS: api_ctx=%p, client_struct=%p", 
        client->api_ctx, (void*)client);
  } else {
    printf("Thread %lu (tid=%d) handling client connection (fd=%d, %s:%d)\n", 
       (unsigned long)tid, system_tid, client_fd, client_ip, client_port);
  }

  /* Check for valid file descriptor */
  if (client_fd <= 0) {
    fprintf(stderr, "Error: Invalid client file descriptor: %d\n", client_fd);
    goto cleanup;
  }

  /* Verify API context is available */
  if (!client->api_ctx) {
    fprintf(stderr, "Error: NULL API context in client handler\n");
    close(client_fd);
    client->client_fd = 0;
    goto cleanup;
  }

  char buffer[BUFFER_SIZE] = {0};
  
  /* Set up SSL connection if needed */
  if (client->use_ssl) {
    if (g_logger) {
      LOG_DEBUG("Setting up SSL connection for client fd=%d", client_fd);
    }
    
    if (client_setup_ssl(client) != 0) {
      if (g_logger) {
        LOG_ERROR("SSL handshake failed for client fd=%d - rejecting connection", client_fd);
      }
      /* Send HTTP error response before closing */
      const char* ssl_required_response = 
        "HTTP/1.1 400 Bad Request\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: close\r\n"
        "Content-Length: 47\r\n"
        "\r\n"
        "SSL/TLS required. Please use HTTPS connection.\n";
      send(client_fd, ssl_required_response, strlen(ssl_required_response), MSG_NOSIGNAL);
      close(client_fd);
      client->client_fd = 0;
      goto cleanup;
    }
    
    if (g_logger) {
      LOG_INFO("SSL connection established for client fd=%d", client_fd);
    }
  }

  /* Set socket to non-blocking */
  int flags = fcntl(client_fd, F_GETFL);
  if (flags < 0) {
    perror("fcntl get flags failed");
    close(client_fd);
    client->client_fd = 0; /* Clear FD in client struct */
    goto cleanup;
  }

  if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    perror("fcntl set flags failed");
    close(client_fd);
    client->client_fd = 0;
    return;
  }

  /* Set timeout */
  struct timeval tv;
  tv.tv_sec = 5; /* 5 seconds timeout */
  tv.tv_usec = 0;
  if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) < 0) {
    perror("setsockopt failed");
    close(client_fd);
    client->client_fd = 0;
    goto cleanup;
  }

  /* Read request with enhanced error tracking */
  if (g_logger) {
    TRACE_NET("CONNECTION_READ_START: fd=%d, attempting to read %d bytes", 
        client_fd, BUFFER_SIZE);
  }
  
  int bytes_read = client_read_data(client, buffer, BUFFER_SIZE);
  if (bytes_read <= 0) {
    const char* error_reason = "unknown";
    if (bytes_read == 0) {
      error_reason = "connection_closed_by_client";
    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
      error_reason = "timeout_or_would_block";
    } else if (errno == ECONNRESET) {
      error_reason = "connection_reset";
    } else if (errno == EPIPE) {
      error_reason = "broken_pipe";
    } else {
      error_reason = "read_error";
    }
    
    if (g_logger) {
      LOG_DEBUG("Connection read failed - fd=%d, bytes=%d, errno=%d, reason=%s, client=%s:%d", 
          client_fd, bytes_read, errno, error_reason, client_ip, client_port);
    } else {
      perror("read failed");
    }
    
    close(client_fd);
    client->client_fd = 0;
    goto cleanup;
  }
  
  if (g_logger) {
    TRACE_NET("CONNECTION_READ_SUCCESS: fd=%d, bytes_read=%d", client_fd, bytes_read);
  }

  /* Null-terminate buffer */
  buffer[bytes_read] = '\0';

  /* Parse HTTP request */
  http_request_t* request = parse_http_request(buffer);
  if (!request) {
    /* Increment error counter */
    metric_t* api_errors = get_api_errors_metric();
    if (api_errors) {
      metrics_counter_inc(api_errors, 1);
    }
    
    /* Send 400 Bad Request */
    http_response_t* response = create_http_response(HTTP_BAD_REQUEST,
      "{\"error\":\"Invalid request\"}", "application/json");

    if (response) {
      char* response_str = serialize_http_response(response);

      if (response_str) {
        size_t response_len = strlen(response_str); /* Safe now with null-termination */
        if (client_write_data(client, response_str, response_len) < 0) {
          perror("write failed");
        }
        free(response_str);
      }

      free_http_response(response);
    }

    close(client_fd);
    client->client_fd = 0;
    goto cleanup;
  }
  
  /* Set client IP address */
  if (client && request) {
    char ip_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &(client->address.sin_addr), ip_str, INET_ADDRSTRLEN)) {
      request->remote_addr = strdup(ip_str);
    }
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
      client_write_data(client, response_str, response_len);
      free(response_str);
    }

    free_http_response(response);
    free_http_request(request);
    close(client_fd);
    goto cleanup;
  }
  
  /* Increment request counter */
  metric_t* request_counter = get_server_requests_metric();
  if (request_counter) {
    metrics_counter_inc(request_counter, 1);
  }
  
  /* Debug request */
  if (g_logger) {
    TRACE_NET("REQUEST_RECEIVED: method=%d (%s), path='%s', content_length=%zu", 
        request->method,
        request->method == HTTP_GET ? "GET" :
        request->method == HTTP_POST ? "POST" :
        request->method == HTTP_PUT ? "PUT" :
        request->method == HTTP_DELETE ? "DELETE" : "UNKNOWN",
        request->path ? request->path : "NULL",
        request->content_length);
    
    LOG_DEBUG("Received request: %s %s",
       request->method == HTTP_GET ? "GET" :
       request->method == HTTP_POST ? "POST" :
       request->method == HTTP_PUT ? "PUT" :
       request->method == HTTP_DELETE ? "DELETE" : "UNKNOWN",
       request->path);
    
    if (request->origin) {
      LOG_DEBUG("Origin: %s", request->origin);
    }
    
    if (request->content_type) {
      LOG_DEBUG("Content-Type: %s", request->content_type);
    }
    
    if (request->body) {
      TRACE_NET("REQUEST_BODY: %.200s", request->body);
    }
  }

  /* Check if this is an admin interface route */
  if ((request->method == HTTP_GET || request->method == HTTP_HEAD) && is_admin_route(request->path)) {
    http_response_t* response = NULL;

    if (g_logger) {
      LOG_DEBUG("Handling admin route: %s", request->path);
    }

    /* Get proper web root directory from server config */
    const char* web_root = ADMIN_FILES_DIR;
    extern server_config_t* g_server_config;
    if (g_server_config && g_server_config->web_root) {
      web_root = g_server_config->web_root;
      if (g_logger) LOG_DEBUG("Using configured web root: %s", web_root);
    } else {
      if (g_logger) LOG_DEBUG("Using default web root: %s", web_root);
    }
    
    /* Always serve static files without authentication for simplicity */
    response = serve_admin_file(request->path);
    if (g_logger) {
      if (response) {
        LOG_DEBUG("[FILE_SERVING] File %s served successfully: status=%d, content_length=%zu, content_type=%s", 
                  request->path, response->status, response->content_length, response->content_type ? response->content_type : "null");
      } else {
        LOG_DEBUG("[FILE_SERVING] File %s NOT served (response is NULL)", request->path);
      }
    }

    /* Print debug info about file path if response wasn't generated */
    if (!response) {
      /* Check if web root directory exists */
      if (g_logger) LOG_DEBUG("Admin files directory: %s", web_root);
      char filepath[512] = {0};

      if (strcmp(request->path, "/") == 0) {
        sprintf(filepath, "%s/index.html", web_root);
        if (g_logger) LOG_DEBUG("Attempting to serve index.html from: %s", filepath);
      } else if (strcmp(request->path, "/login") == 0) {
        sprintf(filepath, "%s/login.html", web_root);
        if (g_logger) LOG_DEBUG("Attempting to serve login.html from: %s", filepath);
      }
    }
    
    /* Apply CORS headers to response */
    extern server_config_t* g_server_config;
    if (g_server_config) {
      response = apply_cors_headers(response, &g_server_config->cors, request->origin);
    }
    
    /* For HEAD requests, clear the body but keep headers including Content-Length */
    if (request->method == HTTP_HEAD && response->body) {
      size_t original_length = response->content_length;
      free(response->body);
      response->body = NULL;
      /* Keep the original content length for HEAD responses */
      response->content_length = original_length;
    }
    
    /* Serialize and send response */
    size_t response_len = 0;
    char* response_str = serialize_http_response_with_length(response, &response_len);
    if (g_logger) {
      LOG_DEBUG("[FILE_SERVING] Serializing response for %s: content_length=%zu, total_response_len=%zu, header_bytes=%zu", 
                request->path, response->content_length, response_len, response_len - response->content_length);
    }
    
    if (response_str) {
      /* Check socket state before writing */
      int socket_error = 0;
      socklen_t error_len = sizeof(socket_error);
      if (getsockopt(client_fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_len) == 0) {
        if (socket_error != 0) {
          LOG_ERROR("[FILE_SERVING] Socket error detected before write for %s: %s", request->path, strerror(socket_error));
        }
      }
      
      /* Check if socket is still connected */
      struct sockaddr_in peer_addr;
      socklen_t peer_len = sizeof(peer_addr);
      if (getpeername(client_fd, (struct sockaddr*)&peer_addr, &peer_len) < 0) {
        LOG_ERROR("[FILE_SERVING] Socket not connected for %s: %s", request->path, strerror(errno));
      }
      
      /* Send response, handling partial writes and errors */
      size_t bytes_sent = 0;
      if (g_logger) LOG_DEBUG("[FILE_SERVING] Starting to send %zu bytes for %s", response_len, request->path);
      while (bytes_sent < response_len) {
        /* Check for socket readiness before each write */
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(client_fd, &write_fds);
        struct timeval timeout = {5, 0}; // 5 second timeout
        
        int ready = select(client_fd + 1, NULL, &write_fds, NULL, &timeout);
        if (ready <= 0) {
          LOG_ERROR("[FILE_SERVING] Socket not ready for write for %s (ready=%d, errno=%s)", 
                    request->path, ready, ready < 0 ? strerror(errno) : "timeout");
          break;
        }
        
        ssize_t result = client_write_data(client, response_str + bytes_sent, response_len - bytes_sent);
        if (result < 0) {
          if (errno == EINTR) {
            /* Interrupted by signal, retry */
            if (g_logger) LOG_DEBUG("[FILE_SERVING] Write interrupted by signal, retrying for %s", request->path);
            continue;
          } else if (errno == EPIPE || errno == ECONNRESET) {
            /* Connection closed by client */
            LOG_ERROR("[FILE_SERVING] Client closed connection during write for %s (sent %zu/%zu bytes)", request->path, bytes_sent, response_len);
            break;
          } else {
            /* Other error */
            LOG_ERROR("[FILE_SERVING] Write error for %s: %s (sent %zu/%zu bytes)", request->path, strerror(errno), bytes_sent, response_len);
            break;
          }
        } else if (result == 0) {
          /* No bytes written, connection may be closed */
          LOG_ERROR("[FILE_SERVING] No bytes written for %s, connection may be closed (sent %zu/%zu bytes)", request->path, bytes_sent, response_len);
          break;
        } else {
          bytes_sent += result;
          if (g_logger && (bytes_sent % 16384 == 0 || bytes_sent == response_len)) {
            LOG_DEBUG("[FILE_SERVING] Progress for %s: %zu/%zu bytes sent", request->path, bytes_sent, response_len);
          }
        }
      }
      if (g_logger) LOG_DEBUG("[FILE_SERVING] Completed sending %s: %zu/%zu bytes sent", request->path, bytes_sent, response_len);
      
      free(response_str);
    }

    /* Cleanup */
    free_http_response(response);
    free_http_request(request);
    
    /* Ensure all data is sent before closing */
    shutdown(client_fd, SHUT_WR);
    close(client_fd);

    goto cleanup;
  }
  
  /* Dispatch request to API handler */
  if (g_logger) {
    TRACE_NET("API_DISPATCH: Dispatching %s %s to API handler", 
        request->method == HTTP_GET ? "GET" :
        request->method == HTTP_POST ? "POST" :
        request->method == HTTP_PUT ? "PUT" :
        request->method == HTTP_DELETE ? "DELETE" : "UNKNOWN",
        request->path ? request->path : "NULL");
  }
  
  http_response_t* response = api_dispatch_request(client->api_ctx, request);
  
  if (g_logger) {
    TRACE_NET("API_DISPATCH_RESULT: response=%p, status=%d", 
        (void*)response, response ? (int)response->status : -1);
  }
  
  /* If no response from API handler, return 404 */
  if (!response) {
    if (g_logger) {
      TRACE_NET("API_DISPATCH: No response from API handler, returning 404.");
    }
    response = create_http_response(HTTP_NOT_FOUND, 
      "{\"error\":\"Not found\"}", "application/json");
  }
  
  /* Apply CORS headers to response */
  extern server_config_t* g_server_config;
  if (g_server_config) {
    response = apply_cors_headers(response, &g_server_config->cors, request->origin);
  }
  
  /* Serialize and send response */
  size_t response_len = 0;
  char* response_str = serialize_http_response_with_length(response, &response_len);
  if (response_str) {
    /* Send response, handling partial writes and errors */
    size_t bytes_sent = 0;
    while (bytes_sent < response_len) {
      ssize_t result = client_write_data(client, response_str + bytes_sent, response_len - bytes_sent);
      if (result < 0) {
        if (errno == EINTR) {
          /* Interrupted by signal, retry */
          continue;
        } else if (errno == EPIPE || errno == ECONNRESET) {
          /* Connection closed by client */
          LOG_DEBUG("Client closed connection during write.");
          break;
        } else {
          /* Other error */
          LOG_ERROR("Write operation failed: %s", strerror(errno));
          break;
        }
      } else if (result == 0) {
        /* No bytes written, connection may be closed */
        break;
      } else {
        bytes_sent += result;
      }
    }
    free(response_str);
  }

  /* Cleanup */
  free_http_response(response);
  free_http_request(request);
  
  /* Clean up SSL connection if needed */
  if (client->use_ssl) {
    client_cleanup_ssl(client);
  }
  
  /* Ensure all data is sent before closing */
  shutdown(client_fd, SHUT_WR);
  close(client_fd);
  
  /* Free client data with safety checks */
  if (client) {
    if (g_logger) {
      TRACE_NET("CONNECTION_NORMAL_CLEANUP: client=%p, performing safe cleanup", (void*)client);
    }
    
    /* Basic pointer validation before normal cleanup */
    if ((uintptr_t)client >= 0x1000 && (uintptr_t)client <= 0x7fffffffffff) {
      /* Zero out critical fields before freeing */
      client->client_fd = -1;
      client->api_ctx = NULL;
      client->ssl_conn = NULL;
      
      free(client);
      client = NULL;
      
      if (g_logger) {
        TRACE_NET("CONNECTION_NORMAL_FREED: client structure freed successfully.");
      }
    } else {
      if (g_logger) {
        LOG_DEBUG("Invalid client pointer %p, skipping free", (void*)client);
      }
    }
  }
  
  /* Calculate execution time */
  clock_gettime(CLOCK_MONOTONIC, &end_time);
  double execution_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0;
  execution_time += (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;
  
  /* Log completion */
  if (g_logger) {
    LOG_INFO("Completed request handling in %.2f ms (thread=%lu, tid=%d)", 
        execution_time, (unsigned long)tid, system_tid);
  } else {
    printf("Thread %lu completed request in %.2f ms\n", 
       (unsigned long)tid, execution_time);
  }
  
  /* Stop request timer */
  if (request_timer) {
    metrics_timer_stop(request_timer);
    request_timer = NULL; /* Prevent double-free in cleanup */
  }
  
cleanup:
  /* Enhanced connection cleanup with comprehensive tracking */
  clock_gettime(CLOCK_MONOTONIC, &end_time);
  double connection_duration = (end_time.tv_sec - start_time.tv_sec) + 
                              (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
  
  if (g_logger) {
    LOG_INFO("Connection ended - fd=%d, thread=%lu, tid=%d, client=%s:%d, duration=%.3fs", 
        client_fd, (unsigned long)tid, system_tid, client_ip, client_port, connection_duration);
    
    /* Log detailed connection state for debugging */
    if (client_fd > 0) {
      /* Check if socket is still valid */
      int socket_error = 0;
      socklen_t len = sizeof(socket_error);
      int gso_result = getsockopt(client_fd, SOL_SOCKET, SO_ERROR, &socket_error, &len);
      
      TRACE_NET("CONNECTION_STATE: fd=%d, getsockopt_result=%d, socket_error=%d, ssl_cleanup=%s", 
          client_fd, gso_result, socket_error, 
          (client && client->ssl_conn) ? "required" : "not_needed");
    }
  }
  
  /* Cleanup SSL connection if active */
  if (client && client->ssl_conn) {
    if (g_logger) {
      TRACE_NET("CONNECTION_SSL_CLEANUP: fd=%d, cleaning up SSL connection", client_fd);
    }
    client_cleanup_ssl(client);
  }
  
  /* Close file descriptor if still open */
  if (client_fd > 0) {
    if (g_logger) {
      TRACE_NET("CONNECTION_FD_CLOSE: fd=%d, closing file descriptor", client_fd);
    }
    close(client_fd);
    if (client) {
      client->client_fd = 0; /* Clear FD in client struct */
    }
  }
  
  /* Enhanced client structure cleanup with memory safety checks */
  if (client) {
    if (g_logger) {
      TRACE_NET("CONNECTION_STRUCT_CLEANUP_START: client=%p, performing safety checks", (void*)client);
    }
    
    /* Memory safety validation before cleanup */
    int cleanup_safe = 1;
    
    /* Check for obvious pointer corruption */
    if ((uintptr_t)client < 0x1000 || (uintptr_t)client > 0x7fffffffffff) {
      if (g_logger) {
        LOG_ERROR("Invalid client pointer detected: %p", (void*)client);
      }
      cleanup_safe = 0;
    }
    
    /* Validate client structure fields if pointer looks valid */
    if (cleanup_safe) {
      /* Check file descriptor validity */
      if (client->client_fd < 0 || client->client_fd > 65535) {
        if (g_logger) {
          LOG_WARNING("Invalid client_fd=%d in structure %p", 
               client->client_fd, (void*)client);
        }
      }
      
      /* Check API context pointer */
      if (client->api_ctx) {
        if ((uintptr_t)client->api_ctx < 0x1000 || (uintptr_t)client->api_ctx > 0x7fffffffffff) {
          if (g_logger) {
            LOG_WARNING("Invalid api_ctx pointer=%p in client %p", 
                 (void*)client->api_ctx, (void*)client);
          }
        }
      }
      
      if (g_logger) {
        TRACE_NET("CONNECTION_STRUCT_VALIDATED: client=%p, fd=%d, api_ctx=%p, ssl_conn=%p", 
             (void*)client, client->client_fd, (void*)client->api_ctx, (void*)client->ssl_conn);
      }
    }
    
    /* Perform safe cleanup if validation passed */
    if (cleanup_safe) {
      if (g_logger) {
        TRACE_NET("CONNECTION_STRUCT_FREE: client=%p, freeing client structure", (void*)client);
      }
      
      /* Zero out critical fields before freeing to detect use-after-free */
      client->client_fd = -1;
      client->api_ctx = NULL;
      client->ssl_conn = NULL;
      
      /* Free the structure */
      free(client);
      
      /* Set client pointer to NULL to prevent double-free (if passed by reference) */
      /* Note: This only protects the local variable, but adds logging clarity */
      client = NULL;
      
      if (g_logger) {
        TRACE_NET("CONNECTION_STRUCT_FREED: structure freed successfully.");
      }
    } else {
      if (g_logger) {
        LOG_WARNING("Unsafe client structure %p not freed to prevent crash", 
             (void*)client);
      }
    }
  } else {
    if (g_logger) {
      TRACE_NET("CONNECTION_STRUCT_NULL: client structure already NULL, no cleanup needed.");
    }
  }
  
  /* Decrement active connections */
  if (g_logger) {
    TRACE_NET("METRICS_CLEANUP_START: active_connections=%p, request_timer=%p", 
        (void*)active_connections, (void*)request_timer);
  }
  
  if (active_connections) {
    if (g_logger) {
      TRACE_NET("METRICS_GAUGE_DEC_START: active_connections=%p", (void*)active_connections);
    }
    metrics_gauge_dec(active_connections, 1.0);
    if (g_logger) {
      TRACE_NET("METRICS_GAUGE_DEC_SUCCESS.");
    }
  }
  
  /* Stop request timer if active */
  if (request_timer) {
    if (g_logger) {
      TRACE_NET("METRICS_TIMER_STOP_START: request_timer=%p", (void*)request_timer);
    }
    metrics_timer_stop(request_timer);
    if (g_logger) {
      TRACE_NET("METRICS_TIMER_STOP_SUCCESS.");
    }
  }
  
  if (g_logger) {
    TRACE_NET("CONNECTION_CLEANUP_COMPLETE: thread=%lu, tid=%d", 
        (unsigned long)tid, system_tid);
  }
}