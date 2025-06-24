#include "init.h"
#include "core/server.h"
#include "utils/ssl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <sys/types.h>

/* Initialize server socket - CLEAN IMPLEMENTATION based on proven approach */
init_status_t init_socket(server_config_t* config) {
  if (!config) {
    INIT_LOG_FAILURE("SOCKET", "NULL server configuration");
    return INIT_SOCKET_ERROR;
  }

  /* Log current process context */
  pid_t process_pid = getpid();
  INIT_LOG_DEBUG("SOCKET", "Socket initialization started in PID %d", process_pid);

  /* Create socket in blocking mode */
  INIT_LOG_PROGRESS("SOCKET", "Creating socket on %s:%d",
          config->host ? config->host : "0.0.0.0", config->port);
          
  int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    INIT_LOG_FAILURE("SOCKET", "Failed to create socket: %s (errno=%d)", 
            strerror(errno), errno);
    return INIT_SOCKET_ERROR;
  }

  INIT_LOG_SUCCESS("SOCKET", "Socket created (fd=%d)", socket_fd);

  /* Set socket options */
  int reuse = 1;
  if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    INIT_LOG_FAILURE("SOCKET", "Failed to set SO_REUSEADDR: %s", strerror(errno));
    /* Continue anyway, this is not fatal */
  } else {
    INIT_LOG_SUCCESS("SOCKET", "Socket option SO_REUSEADDR set");
  }

  /* Set socket keep-alive if enabled */
  if (config->socket_keepalive) {
    int keepalive = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) < 0) {
      INIT_LOG_FAILURE("SOCKET", "Failed to set SO_KEEPALIVE: %s", strerror(errno));
      /* Continue anyway, this is not fatal */
    } else {
      INIT_LOG_SUCCESS("SOCKET", "Socket option SO_KEEPALIVE set");
    }
  }

  /* Set socket reuse port if enabled */
  if (config->socket_reuseport) {
#ifdef SO_REUSEPORT
    int reuseport = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &reuseport, sizeof(reuseport)) < 0) {
      INIT_LOG_FAILURE("SOCKET", "Failed to set SO_REUSEPORT: %s", strerror(errno));
      /* Continue anyway, this is not fatal */
    } else {
      INIT_LOG_SUCCESS("SOCKET", "Socket option SO_REUSEPORT set");
    }
#else
    INIT_LOG_WARNING("SOCKET", "SO_REUSEPORT not supported on this platform");
#endif
  }

  /* Prepare address structure */
  struct sockaddr_in address;
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_port = htons(config->port);

  /* Handle host address - improved approach with proper error handling */
  if (!config->host || strlen(config->host) == 0 || strcmp(config->host, "0.0.0.0") == 0) {
    /* Bind to any address */
    address.sin_addr.s_addr = INADDR_ANY;
    INIT_LOG_PROGRESS("SOCKET", "Using INADDR_ANY (0.0.0.0) for binding");
  } else if (strcmp(config->host, "127.0.0.1") == 0 || strcmp(config->host, "localhost") == 0) {
    /* Bind to localhost */
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    INIT_LOG_PROGRESS("SOCKET", "Using localhost (127.0.0.1) for binding");
  } else if (inet_addr(config->host) != INADDR_NONE) {
    /* It's a valid IP address */
    address.sin_addr.s_addr = inet_addr(config->host);
    INIT_LOG_PROGRESS("SOCKET", "Using IP address %s for binding", config->host);
  } else {
    /* It's a hostname that needs to be resolved - this might be problematic */
    if (g_logger) {
      LOG_WARNING("Hostname '%s' provided, using INADDR_ANY (0.0.0.0) for binding", 
            config->host);
    } else {
      fprintf(stderr, "Socket initialization warning: hostname '%s' provided, using INADDR_ANY (0.0.0.0) for binding\n",
          config->host);
    }
    address.sin_addr.s_addr = INADDR_ANY;
  }

  /* Bind socket */
  INIT_LOG_PROGRESS("SOCKET", "Binding socket to %s:%d", 
          config->host ? config->host : "0.0.0.0", config->port);
          
  if (bind(socket_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
    INIT_LOG_FAILURE("SOCKET", "Failed to bind socket: %s (errno=%d)", 
            strerror(errno), errno);
    close(socket_fd);
    return INIT_SOCKET_ERROR;
  }

  INIT_LOG_SUCCESS("SOCKET", "Socket bound successfully");

  /* Set up to listen for connections with configurable backlog */
  INIT_LOG_PROGRESS("SOCKET", "Setting socket to listen state with backlog=%d", config->socket_backlog);
  
  if (listen(socket_fd, config->socket_backlog) < 0) {
    INIT_LOG_FAILURE("SOCKET", "Failed to listen on socket: %s (errno=%d)", 
            strerror(errno), errno);
    close(socket_fd);
    return INIT_SOCKET_ERROR;
  }

  INIT_LOG_SUCCESS("SOCKET", "Socket listening successfully (backlog=%d)", config->socket_backlog);

  /* Verify socket state */
  int acceptconn = 0;
  socklen_t acceptconn_len = sizeof(acceptconn);
  
  /* Print detailed debugging info */
  LOG_DEBUG("Before check - socket_fd=%d, PID=%d", socket_fd, getpid());
  LOG_DEBUG("Host=%s, Port=%d", 
      config->host ? config->host : "0.0.0.0", config->port);
      
  /* Check if socket is actually bound correctly */
  struct sockaddr_in actual_addr;
  socklen_t actual_len = sizeof(actual_addr);
  if (getsockname(socket_fd, (struct sockaddr*)&actual_addr, &actual_len) < 0) {
    LOG_DEBUG("Failed to get socket name: %s (errno=%d)", 
        strerror(errno), errno);
  } else {
    LOG_DEBUG("Socket is bound to %s:%d", 
        inet_ntoa(actual_addr.sin_addr), ntohs(actual_addr.sin_port));
  }
  
  if (getsockopt(socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &acceptconn, &acceptconn_len) < 0) {
    if (g_logger) {
      LOG_WARNING("Failed to check SO_ACCEPTCONN: %s", strerror(errno));
    } else {
      fprintf(stderr, "Socket initialization warning: failed to check SO_ACCEPTCONN: %s\n", strerror(errno));
    }
  } else {
    LOG_INFO("Socket listening state: %s", 
        acceptconn ? "LISTENING" : "NOT LISTENING");
    
    LOG_DEBUG("Socket listening state: %s", 
        acceptconn ? "LISTENING" : "NOT LISTENING");
    
    if (!acceptconn) {
      INIT_LOG_FAILURE("SOCKET", "Socket is not in listening state despite successful listen() call");
      close(socket_fd);
      return INIT_SOCKET_ERROR;
    }
  }
  
  /* Add a short diagnostic test to verify socket is working */
  int client_temp = socket(AF_INET, SOCK_STREAM, 0);
  if (client_temp >= 0) {
    struct sockaddr_in test_addr;
    memset(&test_addr, 0, sizeof(test_addr));
    test_addr.sin_family = AF_INET;
    test_addr.sin_port = htons(config->port);
    test_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    /* Try to connect non-blocking with timeout */
    int flags = fcntl(client_temp, F_GETFL, 0);
    fcntl(client_temp, F_SETFL, flags | O_NONBLOCK);
    
    if (connect(client_temp, (struct sockaddr*)&test_addr, sizeof(test_addr)) < 0) {
      if (errno == EINPROGRESS) {
        fd_set writefds;
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        FD_ZERO(&writefds);
        FD_SET(client_temp, &writefds);
        
        if (select(client_temp + 1, NULL, &writefds, NULL, &tv) > 0) {
          int optval;
          socklen_t optlen = sizeof(optval);
          if (getsockopt(client_temp, SOL_SOCKET, SO_ERROR, &optval, &optlen) == 0) {
            if (optval == 0) {
              INIT_LOG_SUCCESS("SOCKET", "Socket connect test successful - listener is working");
            } else {
              if (g_logger) {
                LOG_WARNING("Socket connect test failed with error: %s", strerror(optval));
              } else {
                EARLY_LOG_WARNING("SOCKET", "Socket connect test failed with error: %s", strerror(optval));
              }
            }
          }
        } else {
          if (g_logger) {
            LOG_WARNING("Socket connect test timed out - listener might not be working.");
          } else {
            EARLY_LOG_WARNING("SOCKET", "Socket connect test timed out - listener might not be working");
          }
        }
      } else {
        if (g_logger) {
          LOG_WARNING("Socket connect test failed immediately: %s", strerror(errno));
        } else {
          EARLY_LOG_WARNING("SOCKET", "Socket connect test failed immediately: %s", strerror(errno));
        }
      }
    } else {
      INIT_LOG_SUCCESS("SOCKET", "Socket connect test successful - listener is working");
    }
    
    close(client_temp);
  }

  /* Initialize SSL context if SSL is enabled */
  if (config->use_ssl) {
    INIT_LOG_PROGRESS("SOCKET", "SSL enabled, initializing SSL context");
    
    /* Initialize SSL library if not already done */
    ssl_error_t ssl_result = ssl_library_init();
    if (ssl_result != SSL_SUCCESS) {
      INIT_LOG_FAILURE("SOCKET", "Failed to initialize SSL library");
      close(socket_fd);
      return INIT_SOCKET_ERROR;
    }
    
    /* Create SSL configuration */
    ssl_config_t ssl_config = {0};
    ssl_config.cert_file = config->cert_path;
    ssl_config.key_file = config->key_path;
    ssl_config.verify_peer = 0;  /* Default to no client verification */
    ssl_config.verify_depth = 0; /* Default depth */
    ssl_config.ignore_unexpected_eof = config->ssl_ignore_unexpected_eof; /* OpenSSL 3.x compatibility */
    
    INIT_LOG_PROGRESS("SOCKET", "SSL configuration: server_config->ssl_ignore_unexpected_eof=%d", 
                      config->ssl_ignore_unexpected_eof);
    INIT_LOG_PROGRESS("SOCKET", "SSL configuration: ssl_config.ignore_unexpected_eof=%d", 
                      ssl_config.ignore_unexpected_eof);
    
    /* Create SSL context */
    ssl_result = ssl_context_create(&ssl_config, &config->ssl_context);
    if (ssl_result != SSL_SUCCESS) {
      INIT_LOG_FAILURE("SOCKET", "Failed to create SSL context (cert: %s, key: %s)", 
              config->cert_path ? config->cert_path : "none",
              config->key_path ? config->key_path : "none");
      close(socket_fd);
      return INIT_SOCKET_ERROR;
    }
    
    INIT_LOG_SUCCESS("SOCKET", "SSL context created successfully");
    INIT_LOG_SUCCESS("SOCKET", "Using SSL certificate: %s", config->cert_path);
    INIT_LOG_SUCCESS("SOCKET", "Using SSL private key: %s", config->key_path);
  } else {
    INIT_LOG_SUCCESS("SOCKET", "SSL disabled, running in non-SSL mode");
    config->ssl_context = NULL;
  }

  /* Store socket descriptor in config */
  config->socket_fd = socket_fd;

  INIT_LOG_SUCCESS("SOCKET", "Socket initialization complete (socket_fd=%d, port=%d, SSL=%s)", 
          socket_fd, config->port, config->use_ssl ? "enabled" : "disabled");

  return INIT_OK;
}