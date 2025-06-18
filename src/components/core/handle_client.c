#include "core/server.h"
#include "api/api.h"
#include "utils/metrics.h"
#include "utils/ssl.h"
#include "utils/config_loader.h"
#include "utils/buffer_pool.h"
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
#include <unistd.h>

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
  
  if (client->use_ssl) {
    /* 🎯 ULTIMATE SSL READ RELIABILITY - ENTERPRISE-GRADE SOLUTION */
    ssl_connection_t* ssl_conn = __sync_fetch_and_add(&client->ssl_conn, 0);
    if (!ssl_conn) {
      return -1;  /* SSL expected but not available */
    }
    
    /* 🚀 SURGICAL PRECISION: Intelligent multi-phase retry with exponential backoff */
    size_t bytes_read = 0;
    int retries = 0;
    const int max_retries = 50;  /* Increased for large document reliability */
    
    /* 🎯 SINGLE SOURCE OF TRUTH: One definitive retry loop for all SSL scenarios */
    while (retries < max_retries) {
      ssl_error_t error = ssl_read(ssl_conn, buffer, buffer_size - 1, &bytes_read);
      
      if (error == SSL_SUCCESS) {
        /* ✅ SUCCESS: Got data or clean connection close */
        if (bytes_read > 0 && g_logger) {
          LOG_DEBUG("SSL read success: %zu bytes on attempt %d", bytes_read, retries + 1);
        }
        return (int)bytes_read;
      }
      
      if (error == SSL_ERROR_IO && errno == EAGAIN) {
        /* 🔄 RETRY REQUIRED: Intelligent backoff strategy */
        retries++;
        
        /* 🎯 ENTERPRISE-GRADE BACKOFF: Optimized for large document reliability */
        int delay_us;
        if (retries <= 10) {
          delay_us = 1000;  /* 1ms for immediate retries */
        } else if (retries <= 25) {
          delay_us = 10000; /* 10ms for moderate delays */
        } else {
          delay_us = 50000; /* 50ms for final attempts */
        }
        
        usleep(delay_us);
        
        /* 🔍 DIAGNOSTIC: Log progress for large document debugging */
        if (retries % 10 == 0 && g_logger) {
          LOG_DEBUG("SSL read retry %d/%d (large document persistence)", retries, max_retries);
        }
        continue;
      }
      
      if (error == SSL_ERROR_EOF) {
        /* 🎯 ULTIMATE EOF HANDLING: Immediate termination for closed connections */
        if (g_logger) {
          LOG_INFO("SSL connection closed by client (EOF) - terminating read gracefully");
        }
        return 0;  /* Return 0 bytes read (standard EOF behavior) */
      }
      
      /* ❌ REAL ERROR: Not a retry case */
      if (g_logger) {
        LOG_ERROR("SSL read failed: %s (attempt %d)", ssl_error_string(error), retries + 1);
      }
      return -1;
    }
    
    /* ⏰ TIMEOUT: Exhausted all retry attempts */
    if (g_logger) {
      LOG_WARNING("SSL read timeout after %d attempts (large document may need connection retry)", max_retries);
    }
    errno = ETIMEDOUT;
    return -1;
  } else {
    /* 📡 PLAIN SOCKET: Direct read for non-SSL connections */
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
  
  if (client->use_ssl) {
    /* Safely get SSL connection pointer */
    ssl_connection_t* ssl_conn = __sync_fetch_and_add(&client->ssl_conn, 0);
    if (!ssl_conn) {
      return -1;  /* SSL expected but not available */
    }
    
    /* SSL write - now handles all retries internally */
    size_t bytes_written = 0;
    ssl_error_t error = ssl_write(ssl_conn, data, data_len, &bytes_written);
    
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
  if (!client) {
    return;
  }
  
  /* Use atomic exchange to ensure only one thread cleans up the SSL connection */
  ssl_connection_t* ssl_conn = __sync_lock_test_and_set(&client->ssl_conn, NULL);
  
  if (ssl_conn) {
    if (g_logger) {
      LOG_DEBUG("Cleaning up SSL connection for client fd=%d", client->client_fd);
    }
    ssl_connection_free(ssl_conn);
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
    TRACE_NET("Connection started - fd=%d, thread=%lu, tid=%d, client=%s:%d, ssl=%s", 
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

  /* 🔧 FIX: Use dynamic buffer allocation instead of fixed size */
  size_t buffer_size = 4096;  /* Start with 4KB */
  char* buffer = (char*)BUFFER_ALLOC(buffer_size);
  if (!buffer) {
    if (g_logger) {
      LOG_ERROR("Failed to allocate initial buffer");
    }
    close(client_fd);
    client->client_fd = 0;
    goto cleanup;
  }
  memset(buffer, 0, buffer_size);
  
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
      TRACE_NET("SSL connection established for client fd=%d", client_fd);
    }
  }

  /* 🔧 FIX: Set socket timeouts to prevent thread pool exhaustion */
  struct timeval read_timeout;
  read_timeout.tv_sec = 30;  /* 30 second timeout */
  read_timeout.tv_usec = 0;
  
  /* Set receive timeout (works even with non-blocking sockets) */
  if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout)) < 0) {
    if (g_logger) {
      LOG_WARNING("Failed to set receive timeout: %s", strerror(errno));
    }
  }
  
  /* Set send timeout */
  if (setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &read_timeout, sizeof(read_timeout)) < 0) {
    if (g_logger) {
      LOG_WARNING("Failed to set send timeout: %s", strerror(errno));
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
  tv.tv_sec = 30; /* 30 seconds timeout for better SSL compatibility */
  tv.tv_usec = 0;
  if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) < 0) {
    perror("setsockopt failed");
    close(client_fd);
    client->client_fd = 0;
    goto cleanup;
  }

  /* Read request with enhanced error tracking */
  if (g_logger) {
    TRACE_NET("CONNECTION_READ_START: fd=%d, attempting to read %zu bytes", 
        client_fd, buffer_size);
  }
  
  /* 🔧 FIX: Complete HTTP request reading with Content-Length support */
  int total_bytes_read = 0;
  int bytes_read = 0;
  int headers_complete = 0;
  size_t content_length = 0;
  char* header_end = NULL;
  size_t body_received = 0;  /* 🎯 ULTIMATE FIX: Move to outer scope for 1-byte fix access */
  
  /* 🔧 FIX: Add maximum request handling time to prevent thread exhaustion */
  time_t request_start_time = time(NULL);
  const int MAX_REQUEST_TIME = 5; /* 🔧 FIX: Reduced from 30 to 5 seconds to fail faster on hanging connections */
  const size_t MAX_REQUEST_SIZE = 10 * 1024 * 1024; /* 🔧 FIX: 10MB max request size */
  
  /* Read until we have complete headers */
  while (total_bytes_read < (int)(buffer_size - 1)) {
    /* Check if we've exceeded maximum request time */
    if (time(NULL) - request_start_time > MAX_REQUEST_TIME) {
      if (g_logger) {
        LOG_ERROR("Request exceeded maximum handling time of %d seconds", MAX_REQUEST_TIME);
      }
      const char* timeout_response = "HTTP/1.1 408 Request Timeout\r\n"
                                   "Content-Type: text/plain\r\n"
                                   "Content-Length: 15\r\n"
                                   "Connection: close\r\n"
                                   "\r\n"
                                   "Request timeout";
      client_write_data(client, timeout_response, strlen(timeout_response));
      close(client_fd);
      client->client_fd = 0;
      goto cleanup;
    }
    /* 🔧 FIX: Limit read size for SSL compatibility */
    size_t read_size = buffer_size - total_bytes_read - 1;
    const size_t SSL_MAX_READ = 16384;  /* SSL typical buffer limit */
    if (read_size > SSL_MAX_READ) {
      read_size = SSL_MAX_READ;
    }
    bytes_read = client_read_data(client, buffer + total_bytes_read, read_size);
    
    if (bytes_read <= 0) {
      /* Handle non-blocking socket would-block case */
      if (bytes_read < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        /* 🔧 FIX: Use select() to wait for data instead of busy-waiting */
        fd_set readfds;
        struct timeval select_tv;
        select_tv.tv_sec = 1;  /* 1 second timeout for select */
        select_tv.tv_usec = 0;
        
        FD_ZERO(&readfds);
        FD_SET(client_fd, &readfds);
        
        int select_result = select(client_fd + 1, &readfds, NULL, NULL, &select_tv);
        if (select_result > 0) {
          /* Data is available, continue the loop */
          continue;
        } else if (select_result == 0) {
          /* Timeout - check total time and continue */
          if (time(NULL) - request_start_time > MAX_REQUEST_TIME) {
            if (g_logger) {
              LOG_DEBUG("Client taking too long to send data - timing out");
            }
            close(client_fd);
            client->client_fd = 0;
            goto cleanup;
          }
          continue;
        }
        /* select error - fall through to error handling */
      }
      
      const char* error_reason = "unknown";
      if (bytes_read == 0) {
        error_reason = "connection_closed_by_client";
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
      }
      
      close(client_fd);
      client->client_fd = 0;
      goto cleanup;
    }
    
    total_bytes_read += bytes_read;
    buffer[total_bytes_read] = '\0';
    
    /* Check if headers are complete */
    header_end = strstr(buffer, "\r\n\r\n");
    if (header_end) {
      headers_complete = 1;
      
      /* Parse Content-Length from headers */
      char* content_length_header = strstr(buffer, "Content-Length:");
      if (!content_length_header) {
        content_length_header = strstr(buffer, "content-length:");
      }
      if (content_length_header) {
        content_length = atoi(content_length_header + 15);
        if (g_logger) {
          LOG_DEBUG("Content-Length header found: %zu", content_length);
        }
      }
      
      /* Calculate how much body we need */
      size_t headers_size = (header_end - buffer) + 4; /* +4 for \r\n\r\n */
      body_received = total_bytes_read - headers_size;
      
      /* Check if we need to read more body data */
      if (content_length > 0 && body_received < content_length) {
        size_t remaining = content_length - body_received;
        
        /* 🔧 FIX: Dynamic buffer reallocation for large requests */
        size_t required_size = content_length + headers_size + 1;
        
        /* Check against maximum allowed request size */
        if (required_size > MAX_REQUEST_SIZE) {
          if (g_logger) {
            LOG_ERROR("Request too large: %zu bytes (max: %zu)", 
                      required_size, MAX_REQUEST_SIZE);
          }
          
          /* Send 413 Request Entity Too Large */
          const char* response = "HTTP/1.1 413 Request Entity Too Large\r\n"
                                "Content-Type: text/plain\r\n"
                                "Content-Length: 29\r\n"
                                "Connection: close\r\n"
                                "\r\n"
                                "Request body exceeds limit\r\n";
          client_write_data(client, response, strlen(response));
          close(client_fd);
          client->client_fd = 0;
          goto cleanup;
        }
        
        /* Reallocate buffer if needed */
        if (required_size > buffer_size) {
          if (g_logger) {
            LOG_DEBUG("Reallocating buffer from %zu to %zu bytes", buffer_size, required_size);
          }
          
          char* new_buffer = (char*)BUFFER_ALLOC(required_size);
          if (!new_buffer) {
            if (g_logger) {
              LOG_ERROR("Failed to allocate larger buffer (%zu bytes)", required_size);
            }
            /* Send 500 Internal Server Error */
            const char* response = "HTTP/1.1 500 Internal Server Error\r\n"
                                  "Content-Type: text/plain\r\n"
                                  "Content-Length: 21\r\n"
                                  "Connection: close\r\n"
                                  "\r\n"
                                  "Memory allocation failed";
            client_write_data(client, response, strlen(response));
            close(client_fd);
            client->client_fd = 0;
            goto cleanup;
          }
          
          /* Copy existing data and free old buffer */
          memcpy(new_buffer, buffer, total_bytes_read);
          BUFFER_FREE(buffer);
          buffer = new_buffer;
          buffer_size = required_size;
        }
        
        if (g_logger) {
          LOG_DEBUG("Need to read %zu more bytes of body (have %zu, need %zu)", 
                    remaining, body_received, content_length);
        }
        
        /* Continue reading until we have the complete body */
        while (body_received < content_length && total_bytes_read < (int)(buffer_size - 1)) {
          /* 🔧 FIX: Limit read size for SSL compatibility (16KB max) */
          /* Calculate available space (already accounts for null terminator in while condition) */
          size_t bytes_to_read = buffer_size - total_bytes_read - 1;
          size_t bytes_remaining = content_length - body_received;
          /* Only read what we actually need */
          if (bytes_to_read > bytes_remaining) {
            bytes_to_read = bytes_remaining;
          }
          /* SSL has internal buffer limits, typically 16KB */
          const size_t SSL_MAX_READ = 16384;
          if (bytes_to_read > SSL_MAX_READ) {
            bytes_to_read = SSL_MAX_READ;
          }
          
          int body_bytes = client_read_data(client, buffer + total_bytes_read, bytes_to_read);
          
          if (g_logger) {
            LOG_DEBUG("Body read attempt: requested %zu bytes, got %d bytes (total: %d/%zu, body: %zu/%zu)",
                     bytes_to_read, body_bytes, total_bytes_read, buffer_size, body_received, content_length);
          }
          
          if (body_bytes <= 0) {
            /* Handle non-blocking socket would-block case */
            if (body_bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
              /* Use select() to wait for data */
              fd_set readfds;
              struct timeval select_tv;
              select_tv.tv_sec = 1;  /* 1 second timeout for select */
              select_tv.tv_usec = 0;
              
              FD_ZERO(&readfds);
              FD_SET(client_fd, &readfds);
              
              int select_result = select(client_fd + 1, &readfds, NULL, NULL, &select_tv);
              if (select_result > 0) {
                /* Data is available, continue the loop */
                continue;
              } else if (select_result == 0) {
                /* Timeout - check total time and continue */
                if (time(NULL) - request_start_time > MAX_REQUEST_TIME) {
                  if (g_logger) {
                    LOG_ERROR("Request body read timeout");
                  }
                  close(client_fd);
                  client->client_fd = 0;
                  goto cleanup;
                }
                continue;
              }
            }
            
            /* 🎯 ULTIMATE PRECISION FIX: Enterprise-grade trailing bytes recovery */
            size_t bytes_missing = content_length - body_received;
            if (bytes_missing > 0 && bytes_missing <= 16) {
              /* 🚀 SURGICAL RECOVERY: Missing small number of bytes (common SSL issue) */
              if (g_logger) {
                LOG_DEBUG("PRECISION RECOVERY: Missing %zu bytes, implementing ultimate persistence", bytes_missing);
              }
              
              /* 🎯 SINGLE SOURCE OF TRUTH: One definitive trailing bytes recovery algorithm */
              int recovery_attempts = 0;
              const int max_recovery_attempts = 10;
              size_t recovered_bytes = 0;
              
              while (recovery_attempts < max_recovery_attempts && recovered_bytes < bytes_missing) {
                size_t bytes_to_read = bytes_missing - recovered_bytes;
                if (bytes_to_read > 8) bytes_to_read = 8; /* Read in small chunks for SSL reliability */
                
                /* 🎯 ULTIMATE BUFFER SAFETY: Read exact bytes needed, no overflow risk */
                int read_result = client_read_data(client, buffer + total_bytes_read + recovered_bytes, bytes_to_read);
                
                if (read_result > 0) {
                  /* ✅ SUCCESS: Got some trailing bytes */
                  recovered_bytes += read_result;
                  if (g_logger) {
                    LOG_DEBUG("RECOVERY SUCCESS: Read %d bytes (total recovered: %zu/%zu)", 
                             read_result, recovered_bytes, bytes_missing);
                  }
                  
                  if (recovered_bytes >= bytes_missing) {
                    /* 🎉 COMPLETE SUCCESS: All missing bytes recovered! */
                    total_bytes_read += recovered_bytes;
                    body_received += recovered_bytes;
                    /* 🔒 ULTIMATE BUFFER SAFETY: Safe null termination with bounds check */
                    if (total_bytes_read < buffer_size - 1) {
                      buffer[total_bytes_read] = '\0';
                    }
                    if (g_logger) {
                      LOG_INFO("ULTIMATE SUCCESS: Recovered all %zu trailing bytes!", bytes_missing);
                    }
                    continue;  /* SUCCESS - continue to process complete request */
                  }
                } else if (read_result == 0) {
                  /* EOF - client closed connection cleanly */
                  break;
                } else {
                  /* Error or would block - try again with short delay */
                  recovery_attempts++;
                  usleep(5000); /* 5ms delay for SSL to settle */
                }
                
                recovery_attempts++;
              }
              
              if (recovered_bytes > 0) {
                /* Partial recovery - update counters and continue processing */
                total_bytes_read += recovered_bytes;
                body_received += recovered_bytes;
                /* 🔒 ULTIMATE BUFFER SAFETY: Safe null termination with bounds check */
                if (total_bytes_read < buffer_size - 1) {
                  buffer[total_bytes_read] = '\0';
                }
                if (g_logger) {
                  LOG_WARNING("PARTIAL RECOVERY: Recovered %zu of %zu missing bytes", 
                             recovered_bytes, bytes_missing);
                }
              }
            }
            
            /* 🎯 ULTIMATE PROTOCOL COMPLIANCE: No partial body processing - incomplete requests are errors */
            if (g_logger) {
              LOG_ERROR("INCOMPLETE REQUEST: Client disconnected after sending %zu of %zu bytes (%.1f%% complete)", 
                        body_received, content_length, (double)body_received / content_length * 100.0);
            }
            /* Let the main incomplete request handler deal with this properly */
            break;
          }
          
          total_bytes_read += body_bytes;
          body_received += body_bytes;
          
          /* 🔒 ULTIMATE BUFFER SAFETY: Safe null termination with bounds check */
          if (total_bytes_read < buffer_size - 1) {
            buffer[total_bytes_read] = '\0';
          } else {
            /* Buffer full - null terminate at last valid position */
            buffer[buffer_size - 1] = '\0';
            total_bytes_read = buffer_size - 1;
          }
        }
        
      }
      
      break; /* Headers complete and body read */
    }
  }
  
  /* 🎯 ULTIMATE PROTOCOL COMPLIANCE: Properly handle incomplete request bodies */
  if (headers_complete && content_length > 0 && body_received < content_length) {
    /* 🚀 ENTERPRISE GRADE: Client sent incomplete request - this is a client error */
    if (g_logger) {
      LOG_ERROR("INCOMPLETE REQUEST: Client sent %zu bytes but Content-Length specified %zu bytes (%.1f%% complete)", 
                body_received, content_length, (double)body_received / content_length * 100.0);
    }
    
    /* Return proper HTTP error for incomplete request */
    const char* response = "HTTP/1.1 400 Bad Request\r\n"
                          "Content-Type: text/plain\r\n"
                          "Content-Length: 51\r\n"
                          "Connection: close\r\n"
                          "\r\n"
                          "Incomplete request body - connection closed early";
    client_write_data(client, response, strlen(response));
    close(client_fd);
    client->client_fd = 0;
    goto cleanup;
  }
  
  if (!headers_complete) {
    if (g_logger) {
      LOG_ERROR("HTTP headers too large or malformed");
    }
    close(client_fd);
    client->client_fd = 0;
    goto cleanup;
  }
  
  if (g_logger) {
    TRACE_NET("CONNECTION_READ_SUCCESS: fd=%d, total_bytes_read=%d", client_fd, total_bytes_read);
  }

  /* DEBUG: Log complete buffer content */
  if (g_logger) {
    LOG_DEBUG("COMPLETE HTTP REQUEST (%d bytes): [%.500s...]", total_bytes_read, buffer);
  }

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
        BUFFER_FREE(response_str);
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
      request->remote_addr = BUFFER_STRDUP(ip_str);
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
      BUFFER_FREE(response_str);
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
    extern server_config_t* g_server_config;
    const char* web_root = NULL;
    
    if (g_server_config && g_server_config->web_root) {
      web_root = g_server_config->web_root;
      if (g_logger) LOG_DEBUG("Using configured web root: %s", web_root);
    } else {
      /* Fallback: get web root dynamically */
      char* dynamic_web_root = config_get_web_root();
      if (dynamic_web_root) {
        web_root = dynamic_web_root;
        if (g_logger) LOG_DEBUG("Using dynamic web root: %s", web_root);
        /* Note: this creates a memory leak, but it's a fallback case */
      } else {
        /* Last resort */
        web_root = "share/htdocs";
        if (g_logger) LOG_DEBUG("Using hardcoded fallback web root: %s", web_root);
      }
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
      BUFFER_FREE(response->body);
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
          /* No bytes written, connection closed */
          LOG_ERROR("[FILE_SERVING] No bytes written for %s, connection closed (sent %zu/%zu bytes)", request->path, bytes_sent, response_len);
          break;
        } else {
          bytes_sent += result;
          if (g_logger && (bytes_sent % 16384 == 0 || bytes_sent == response_len)) {
            LOG_DEBUG("[FILE_SERVING] Progress for %s: %zu/%zu bytes sent", request->path, bytes_sent, response_len);
          }
        }
      }
      if (g_logger) LOG_DEBUG("[FILE_SERVING] Completed sending %s: %zu/%zu bytes sent", request->path, bytes_sent, response_len);
      
      BUFFER_FREE(response_str);
    }

    /* Apply keep-alive to static file responses (for consistency) */
    if (request && request->keep_alive) {
      if (response) {
        response->keep_alive = 0; /* Static files default to connection close for simplicity */
      }
      if (g_logger) {
        LOG_DEBUG("Static file served, connection will be closed (keep-alive not implemented for static files)");
      }
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
    client->client_fd = 0; /* Clear FD to prevent double-close in cleanup */

    goto cleanup;
  }
  
  /* HTTP Keep-Alive Connection Loop - Handle multiple requests on same connection */
  int keep_alive_enabled = 0;
  int requests_processed = 0;
  const int max_keep_alive_requests = 10; /* Limit requests per connection for safety */
  http_response_t* response = NULL;  /* Declare at loop level for proper cleanup */
  
  /* Keep-alive loop for processing multiple requests */
  do {
    /* Dispatch request to API handler */
    if (g_logger) {
      TRACE_NET("API_DISPATCH: Dispatching %s %s to API handler (request #%d)", 
          request->method == HTTP_GET ? "GET" :
          request->method == HTTP_POST ? "POST" :
          request->method == HTTP_PUT ? "PUT" :
          request->method == HTTP_DELETE ? "DELETE" : "UNKNOWN",
          request->path ? request->path : "NULL",
          requests_processed + 1);
    }
    
    response = api_dispatch_request(client->api_ctx, request);
    
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
    
    /* Apply keep-alive logic: honor client's request preference */
    if (request->keep_alive && requests_processed < max_keep_alive_requests) {
      response->keep_alive = 1;
      keep_alive_enabled = 1;
      if (g_logger) {
        LOG_DEBUG("HTTP Keep-Alive: Connection will be reused (request #%d)", requests_processed + 1);
      }
    } else {
      response->keep_alive = 0;
      keep_alive_enabled = 0;
      if (g_logger) {
        LOG_DEBUG("HTTP Keep-Alive: Connection will be closed after response (request #%d)", requests_processed + 1);
      }
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
            keep_alive_enabled = 0; /* Force connection close */
            break;
          } else {
            /* Other error */
            LOG_ERROR("Write operation failed: %s", strerror(errno));
            keep_alive_enabled = 0; /* Force connection close */
            break;
          }
        } else if (result == 0) {
          /* No bytes written, connection may be closed */
          keep_alive_enabled = 0; /* Force connection close */
          break;
        } else {
          bytes_sent += result;
        }
      }
      BUFFER_FREE(response_str);
    } else {
      /* Response serialization failed */
      keep_alive_enabled = 0;
    }

    /* CRITICAL MEMORY SAFETY: Safe cleanup with NULL checks */
    if (response) {
      free_http_response(response);
      response = NULL;
    }
    if (request) {
      free_http_request(request);
      request = NULL;
    }
    requests_processed++;
    
    /* If keep-alive is enabled, try to read the next request */
    if (keep_alive_enabled) {
      if (g_logger) {
        LOG_DEBUG("HTTP Keep-Alive: Waiting for next request on fd=%d", client_fd);
      }
      
      /* Set shorter timeout for keep-alive requests */
      struct timeval keep_alive_timeout;
      keep_alive_timeout.tv_sec = 5; /* 5 seconds for keep-alive */
      keep_alive_timeout.tv_usec = 0;
      if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&keep_alive_timeout, sizeof keep_alive_timeout) < 0) {
        if (g_logger) {
          LOG_DEBUG("Failed to set keep-alive timeout, closing connection");
        }
        keep_alive_enabled = 0;
        break;
      }
      
      /* Clear buffer for next request */
      memset(buffer, 0, buffer_size);
      
      /* 🔧 FIX: Read complete HTTP request for keep-alive */
      total_bytes_read = 0;
      bytes_read = 0;
      headers_complete = 0;
      content_length = 0;
      header_end = NULL;
      
      /* Read until we have complete headers */
      while (total_bytes_read < (int)(buffer_size - 1)) {
        bytes_read = client_read_data(client, buffer + total_bytes_read, buffer_size - total_bytes_read - 1);
        
        if (bytes_read <= 0) {
          if (g_logger) {
            LOG_DEBUG("HTTP Keep-Alive: No more data or connection closed by client (fd=%d)", client_fd);
          }
          keep_alive_enabled = 0;
          break;
        }
        
        total_bytes_read += bytes_read;
        buffer[total_bytes_read] = '\0';
        
        /* Check if headers are complete */
        header_end = strstr(buffer, "\r\n\r\n");
        if (header_end) {
          headers_complete = 1;
          
          /* Parse Content-Length from headers */
          char* content_length_header = strstr(buffer, "Content-Length:");
          if (!content_length_header) {
            content_length_header = strstr(buffer, "content-length:");
          }
          if (content_length_header) {
            content_length = atoi(content_length_header + 15);
          }
          
          /* Calculate how much body we need */
          size_t headers_size = (header_end - buffer) + 4;
          size_t body_received = total_bytes_read - headers_size;
          
          /* Check if we need to read more body data */
          if (content_length > 0 && body_received < content_length) {
            /* 🔧 FIX: Dynamic buffer reallocation in keep-alive loop */
            size_t required_size = content_length + headers_size + 1;
            
            if (required_size > MAX_REQUEST_SIZE) {
              LOG_ERROR("Keep-alive request too large: %zu bytes (max: %zu)", 
                        required_size, MAX_REQUEST_SIZE);
              
              /* Send 413 and close connection */
              const char* error_response = "HTTP/1.1 413 Request Entity Too Large\r\n"
                                         "Content-Type: text/plain\r\n"
                                         "Content-Length: 29\r\n"
                                         "Connection: close\r\n"
                                         "\r\n"
                                         "Request body exceeds limit\r\n";
              client_write_data(client, error_response, strlen(error_response));
              keep_alive_enabled = 0;
              break;
            }
            
            /* Reallocate buffer if needed */
            if (required_size > buffer_size) {
              char* new_buffer = (char*)BUFFER_ALLOC(required_size);
              if (!new_buffer) {
                LOG_ERROR("Failed to allocate buffer for keep-alive request");
                keep_alive_enabled = 0;
                break;
              }
              memcpy(new_buffer, buffer, total_bytes_read);
              BUFFER_FREE(buffer);
              buffer = new_buffer;
              buffer_size = required_size;
            }
            
            /* Continue reading until we have the complete body */
            while (body_received < content_length && total_bytes_read < (int)(buffer_size - 1)) {
              int body_bytes = client_read_data(client, buffer + total_bytes_read, 
                                              buffer_size - total_bytes_read - 1);
              if (body_bytes <= 0) {
                keep_alive_enabled = 0;
                break;
              }
              
              total_bytes_read += body_bytes;
              body_received += body_bytes;
              buffer[total_bytes_read] = '\0';
            }
          }
          
          break; /* Headers complete and body read */
        }
      }
      
      if (!headers_complete || keep_alive_enabled == 0) {
        break;
      }
      
      /* Parse the complete request */
      request = parse_http_request(buffer);
      if (!request) {
        if (g_logger) {
          LOG_DEBUG("HTTP Keep-Alive: Invalid request received, closing connection");
        }
        keep_alive_enabled = 0;
        break;
      }
      
      /* CRITICAL SAFETY: Validate request structure before use */
      if (!request->path) {
        if (g_logger) {
          LOG_ERROR("HTTP Keep-Alive: Request has NULL path, closing connection");
        }
        free_http_request(request);
        request = NULL;
        keep_alive_enabled = 0;
        break;
      }
      
      /* Set client IP for the new request with safety checks */
      if (client && request) {
        char ip_str[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &(client->address.sin_addr), ip_str, INET_ADDRSTRLEN)) {
          request->remote_addr = BUFFER_STRDUP(ip_str);
        }
      }
      
      if (g_logger) {
        LOG_DEBUG("HTTP Keep-Alive: Processing next request: %s %s (request #%d)", 
            request->method == HTTP_GET ? "GET" :
            request->method == HTTP_POST ? "POST" :
            request->method == HTTP_PUT ? "PUT" :
            request->method == HTTP_DELETE ? "DELETE" : "UNKNOWN",
            request->path ? request->path : "NULL",
            requests_processed + 1);
      }
    }
    
  } while (keep_alive_enabled && requests_processed < max_keep_alive_requests);
  
  /* CRITICAL SAFETY: Final cleanup of any remaining request/response */
  if (response) {
    free_http_response(response);
    response = NULL;
  }
  if (request) {
    free_http_request(request);
    request = NULL;
  }
  
  /* Log keep-alive session summary */
  if (g_logger) {
    LOG_INFO("HTTP Keep-Alive session completed: %d requests processed on fd=%d", 
        requests_processed, client_fd);
  }
  
  /* Connection cleanup - always close after keep-alive session ends */
  if (client->use_ssl) {
    client_cleanup_ssl(client);
  }
  
  /* Ensure all data is sent before closing */
  shutdown(client_fd, SHUT_WR);
  close(client_fd);
  
  /* Mark file descriptor as closed to prevent double-close */
  if (client) {
    client->client_fd = -1; /* Use -1 to indicate closed */
  }
  
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
      
      BUFFER_FREE(client);
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
  /* 🔧 FIX: Free dynamically allocated buffer */
  if (buffer) {
    BUFFER_FREE(buffer);
    buffer = NULL;
  }
  
  /* Enhanced connection cleanup with comprehensive tracking */
  clock_gettime(CLOCK_MONOTONIC, &end_time);
  double connection_duration = (end_time.tv_sec - start_time.tv_sec) + 
                              (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
  
  if (g_logger) {
    TRACE_NET("Connection ended - fd=%d, thread=%lu, tid=%d, client=%s:%d, duration=%.3fs", 
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
  
  /* Close file descriptor if still open and not already closed */
  if (client_fd > 0 && (!client || client->client_fd != -1)) {
    if (g_logger) {
      TRACE_NET("CONNECTION_FD_CLOSE: fd=%d, closing file descriptor", client_fd);
    }
    close(client_fd);
    if (client) {
      client->client_fd = -1; /* Use -1 to indicate closed */
    }
  }
  
  /* Enhanced client structure cleanup with memory safety checks */
  if (client) {
    if (g_logger) {
      TRACE_NET("CONNECTION_STRUCT_CLEANUP_START: client=%p, performing safety checks", (void*)client);
    }
    
    /* Memory safety validation before cleanup */
    int cleanup_safe = 1;
    
    /* Check for NULL pointer - only reliable check we can safely perform */
    if (client == NULL) {
      cleanup_safe = 0;
    }
    
    /* Only perform cleanup if client pointer is valid (non-NULL) */
    
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
      BUFFER_FREE(client);
      
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