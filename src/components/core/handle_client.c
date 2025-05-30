#include "core/server.h"
#include "api/api.h"
#include "utils/metrics.h"
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

/* Handle client connection */
void* handle_client(void* client_data) {
  /* Get start time for performance tracking */
  struct timespec start_time, end_time;
  clock_gettime(CLOCK_MONOTONIC, &start_time);
  
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
  
  /* Log client connection info */
  if (g_logger) {
    LOG_INFO("Handling client connection (fd=%d, thread=%lu, tid=%d)", 
        client_fd, (unsigned long)tid, system_tid);
  } else {
    printf("Thread %lu (tid=%d) handling client connection (fd=%d)\n", 
       (unsigned long)tid, system_tid, client_fd);
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
    return NULL;
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

  /* Read request */
  int bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
  if (bytes_read <= 0) {
    if (bytes_read < 0) {
      perror("read failed");
    }
    close(client_fd);
    client->client_fd = 0;
    goto cleanup;
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
        if (write(client_fd, response_str, response_len) < 0) {
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
      write(client_fd, response_str, response_len);
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
        
        ssize_t result = write(client_fd, response_str + bytes_sent, response_len - bytes_sent);
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
  http_response_t* response = api_dispatch_request(client->api_ctx, request);
  
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
  size_t response_len = 0;
  char* response_str = serialize_http_response_with_length(response, &response_len);
  if (response_str) {
    /* Send response, handling partial writes and errors */
    size_t bytes_sent = 0;
    while (bytes_sent < response_len) {
      ssize_t result = write(client_fd, response_str + bytes_sent, response_len - bytes_sent);
      if (result < 0) {
        if (errno == EINTR) {
          /* Interrupted by signal, retry */
          continue;
        } else if (errno == EPIPE || errno == ECONNRESET) {
          /* Connection closed by client */
          LOG_DEBUG("Client closed connection during write");
          break;
        } else {
          /* Other error */
          LOG_ERROR("Write error: %s", strerror(errno));
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
  
  /* Ensure all data is sent before closing */
  shutdown(client_fd, SHUT_WR);
  close(client_fd);
  
  /* Free client data */
  free(client);
  
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
  }
  
cleanup:
  /* Decrement active connections */
  if (active_connections) {
    metrics_gauge_dec(active_connections, 1.0);
  }
  
  return NULL;
}