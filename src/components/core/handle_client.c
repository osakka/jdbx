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
    return NULL;
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
    return NULL;
  }

  /* Verify API context is available */
  if (!client->api_ctx) {
    fprintf(stderr, "Error: NULL API context in client handler\n");
    close(client_fd);
    client->client_fd = 0;
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
  tv.tv_sec = 5; /* 5 seconds timeout */
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
    return NULL;
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
    return NULL;
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
  if (request->method == HTTP_GET && is_admin_route(request->path)) {
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
    if (g_logger) LOG_DEBUG("File served: %s", response ? "yes" : "no");

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
  
  /* Decrement active connections */
  if (active_connections) {
    metrics_gauge_dec(active_connections, 1.0);
  }
  
  return NULL;
}