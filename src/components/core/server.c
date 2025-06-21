#include "core/server.h"
#include "core/server_thread_safe.h"
#include "api/api.h"
#include "utils/daemonize.h"
#include "utils/logger.h"
#include "utils/memory_manager.h"
#include "utils/ssl.h"
#include "init.h" /* For init_socket and INIT_OK */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>   /* For getaddrinfo */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include "utils/buffer_pool.h"

/* Graceful shutdown flag */
static volatile int g_shutdown_requested = 0;

/* Signal pipe for safe shutdown */
static int g_signal_pipe[2] = {-1, -1};

/* SSL context is now stored in server config - no global variable needed */

/* Forward declarations */
static int initialize_thread_pool(server_config_t* config);
/* SSL initialization is handled by socket initialization - no duplicate function needed */
/* SSL cleanup is handled by socket cleanup - no separate function needed */
static void* accept_thread_func(void* arg);
static void handle_signals(void);
static void signal_handler(int sig);

/* External functions */
extern init_status_t init_socket(server_config_t* config);
extern void handle_client(void* client_data);

/**
 * Initialize and run the server with improved design
 * 
 * This function implements a better initialization sequence:
 * 1. Set up signal handlers FIRST
 * 2. Initialize socket AFTER daemonization (if applicable)
 * 3. Create thread pool
 * 4. Enter accept loop in main thread
 * 
 * @param config Server configuration
 * @param api_ctx API context
 * @return Server status
 */
server_status_t server_initialize_and_run(server_config_t* config, api_context_t* api_ctx) {
  if (!config || !api_ctx) {
    fprintf(stderr, "Error: Invalid server config or API context\n");
    if (g_logger) {
      LOG_ERROR("Invalid server config (%p) or API context (%p)", (void*)config, (void*)api_ctx);
    }
    return SERVER_ERROR;
  }
  
  /* Store API context in config */
  config->api_ctx = api_ctx;
  
  /* Log configuration details */
  if (g_logger) {
    LOG_INFO("Starting server initialization");
    LOG_INFO("Server configuration: port=%d, host=%s, max_conn=%d, sock_fd=%d", 
        config->port, 
        config->host ? config->host : "(null)", 
        config->max_connections,
        config->socket_fd);
    LOG_DEBUG("Verbose mode: %s, PID file: %s", 
         config->verbose_mode ? "yes" : "no",
         config->pid_file ? config->pid_file : "(null)");
  }
  
  /* Set up signal handling first (before any threads) */
  handle_signals();
  if (g_logger) {
    LOG_DEBUG("Signal handlers installed.");
  }
  
  /* Create signal pipe for safe shutdown */
  if (pipe(g_signal_pipe) < 0) {
    fprintf(stderr, "Error: Failed to create signal pipe: %s\n", strerror(errno));
    if (g_logger) {
      LOG_ERROR("Failed to create signal pipe: %s", strerror(errno));
    }
    return SERVER_ERROR;
  }
  
  if (g_logger) {
    LOG_DEBUG("Signal pipe created [%d, %d]", g_signal_pipe[0], g_signal_pipe[1]);
  }
  
  /* Set pipe to non-blocking */
  for (int i = 0; i < 2; i++) {
    int flags = fcntl(g_signal_pipe[i], F_GETFL, 0);
    if (flags < 0 || fcntl(g_signal_pipe[i], F_SETFL, flags | O_NONBLOCK) < 0) {
      if (g_logger) {
        LOG_WARNING("Cannot set signal pipe to non-blocking: %s", strerror(errno));
      } else {
        fprintf(stderr, "Warning: Failed to set signal pipe to non-blocking: %s\n", strerror(errno));
      }
    }
  }
  
  /* Initialize socket ONLY if not already initialized */
  if (config->socket_fd <= 0) {
    if (g_logger) {
      LOG_INFO("Initializing socket on %s:%d", 
          config->host ? config->host : "0.0.0.0", config->port);
    } else {
      printf("Starting JDBX server on port %d...\n", config->port);
    }
    
    if (init_socket(config) != INIT_OK) {
      if (g_logger) {
        LOG_ERROR("Failed to initialize server socket");
      } else {
        fprintf(stderr, "Error: Failed to initialize server socket\n");
      }
      return SERVER_SOCKET_ERROR;
    }
    
    if (g_logger) {
      LOG_INFO("Socket initialized (fd=%d)", config->socket_fd);
    } else {
      printf("Socket initialization successful (fd=%d)\n", config->socket_fd);
    }
  } else {
    /* Socket already initialized */
    if (g_logger) {
      LOG_INFO("Using pre-initialized socket (fd=%d)", config->socket_fd);
    } else {
      printf("Using pre-initialized socket (fd=%d)\n", config->socket_fd);
    }
    
    /* Verify socket is still valid */
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(config->socket_fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
      if (g_logger) {
        LOG_ERROR("Pre-initialized socket is invalid: %s", strerror(errno));
      } else {
        fprintf(stderr, "Error: Pre-initialized socket is invalid: %s\n", strerror(errno));
      }
      return SERVER_SOCKET_ERROR;
    }
  }
  
  /* Initialize SSL if enabled and not already initialized */
  if (config->use_ssl) {
    if (config->ssl_context) {
      /* SSL context already created by socket initialization */
      if (g_logger) {
        LOG_INFO("Using pre-initialized SSL context from socket initialization.");
      }
    } else {
      /* SSL should have been initialized by socket initialization if enabled */
      if (g_logger) {
        LOG_ERROR("SSL enabled but no SSL context found. Socket initialization may have failed.");
      } else {
        fprintf(stderr, "Error: SSL enabled but no SSL context found\n");
      }
      return SERVER_ERROR;
    }
  } else {
    if (g_logger) {
      LOG_INFO("SSL disabled - running in plain HTTP mode.");
    }
  }
  
  /* Initialize thread pool */
  if (g_logger) {
    LOG_INFO("Initializing thread pool.");
  }
  
  if (initialize_thread_pool(config) != 0) {
    if (g_logger) {
      LOG_ERROR("Failed to initialize thread pool");
    } else {
      fprintf(stderr, "Error: Failed to initialize thread pool\n");
    }
    return SERVER_THREAD_ERROR;
  }
  
  if (g_logger) {
    LOG_INFO("Thread pool initialized.");
  }
  
  /* Start accept loop in the current thread */
  if (g_logger) {
    LOG_INFO("Starting accept loop: socket=%d port=%d", config->socket_fd, config->port);
  } else {
    printf("Starting accept loop on socket %d (port %d)\n", config->socket_fd, config->port);
  }
  
  /* Always use the standard accept loop which supports SSL properly */
  if (g_logger) {
    LOG_INFO("Starting accept loop with SSL support enabled=%d", config->use_ssl);
  }
  accept_thread_func(config);
  
  /* Clean up resources */
  if (g_logger) {
    LOG_INFO("Shutting down server...");
  } else {
    printf("Shutting down server...\n");
  }
  
  /* Close socket */
  if (config->socket_fd > 0) {
    if (g_logger) {
      LOG_DEBUG("Closing server socket (fd=%d)", config->socket_fd);
    }
    close(config->socket_fd);
    config->socket_fd = 0;
  }
  
  /* Destroy thread pool */
  if (config->thread_pool) {
    if (g_logger) {
      LOG_DEBUG("Destroying thread pool.");
    }
    thread_pool_destroy(config->thread_pool);
    config->thread_pool = NULL;
  }
  
  /* Clean up SSL context */
  if (config->use_ssl && config->ssl_context) {
    if (g_logger) {
      LOG_DEBUG("Cleaning up SSL context.");
    }
    ssl_context_free(config->ssl_context);
    config->ssl_context = NULL;
    ssl_library_cleanup();
  }
  
  /* Close signal pipe */
  for (int i = 0; i < 2; i++) {
    if (g_signal_pipe[i] >= 0) {
      close(g_signal_pipe[i]);
      g_signal_pipe[i] = -1;
    }
  }
  
  if (g_logger) {
    LOG_INFO("Server shutdown complete.");
  } else {
    printf("Server shutdown complete\n");
  }
  
  return SERVER_OK;
}

/**
 * REMOVED: This socket initialization function has been replaced with init_socket() from initialize/socket.c
 * 
 * Use init_socket() instead of this function for socket initialization.
 */

/**
 * Initialize thread pool
 */
static int initialize_thread_pool(server_config_t* config) {
  if (!config) {
    fprintf(stderr, "Error: NULL server configuration\n");
    return -1;
  }
  
  /* Calculate thread pool size based on configuration */
  int min_threads = 4; /* Default minimum */
  int max_threads = config->max_connections > 0 ? config->max_connections : 16;
  
  thread_pool_config_t pool_config = {
    .min_threads = min_threads,
    .max_threads = max_threads,
    .queue_size = max_threads * 4, /* Queue size proportional to max threads */
    .idle_timeout = 60 /* 1 minute idle timeout */
  };
  
  /* Create thread pool */
  config->thread_pool = thread_pool_create_config(&pool_config);
  if (!config->thread_pool) {
    fprintf(stderr, "Error: Failed to create thread pool\n");
    return -1;
  }
  
  printf("Thread pool created with %d-%d threads\n", 
      pool_config.min_threads, pool_config.max_threads);
  
  return 0;
}

/**
 * Accept thread function
 */
static void* accept_thread_func(void* arg) {
  server_config_t* config = (server_config_t*)arg;
  
  if (!config) {
    if (g_logger) {
      LOG_ERROR("NULL server configuration in accept thread.");
    } else {
      fprintf(stderr, "Error: NULL server configuration in accept thread\n");
    }
    return NULL;
  }
  
  /* Get thread ID for logging */
  pthread_t tid = pthread_self();
  pid_t system_tid = (pid_t)syscall(SYS_gettid);
  
  if (g_logger) {
    LOG_INFO("Accept thread started (pthread_id=%lu, system_tid=%d)", 
        (unsigned long)tid, system_tid);
  } else {
    printf("Accept thread started (pthread_id=%lu, system_tid=%d)\n", 
        (unsigned long)tid, system_tid);
  }
  
  /* Verify socket is valid */
  if (config->socket_fd <= 0) {
    if (g_logger) {
      LOG_ERROR("Invalid socket descriptor (%d) in accept thread", config->socket_fd);
    } else {
      fprintf(stderr, "Error: Invalid socket descriptor (%d) in accept thread\n", config->socket_fd);
    }
    return NULL;
  }
  
  /* Verify API context is available */
  if (!config->api_ctx) {
    if (g_logger) {
      LOG_ERROR("NULL API context in accept thread.");
    } else {
      fprintf(stderr, "Error: NULL API context in accept thread\n");
    }
    return NULL;
  }
  
  /* Verify thread pool is available */
  if (!config->thread_pool) {
    if (g_logger) {
      LOG_ERROR("NULL thread pool in accept thread.");
    } else {
      fprintf(stderr, "Error: NULL thread pool in accept thread\n");
    }
    return NULL;
  }
  
  /* Accept loop */
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
  int client_fd;
  int connection_count = 0;
  
  /* Debug: Print socket details */
  TRACE_NET("Starting accept thread with socket_fd=%d, PID=%d", config->socket_fd, getpid());
  
  /* Verify socket is still in listen state */
  int acceptconn = 0;
  socklen_t acceptconn_len = sizeof(acceptconn);
  if (getsockopt(config->socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &acceptconn, &acceptconn_len) < 0) {
    LOG_WARNING("Failed to check SO_ACCEPTCONN: %s", strerror(errno));
  } else {
    TRACE_NET("Socket listening state: %s", 
       acceptconn ? "LISTENING" : "NOT LISTENING");
  }
  
  /* Check actual bound address */
  struct sockaddr_in actual_addr;
  socklen_t actual_len = sizeof(actual_addr);
  if (getsockname(config->socket_fd, (struct sockaddr*)&actual_addr, &actual_len) < 0) {
    TRACE_NET("Failed to get socket name: %s", strerror(errno));
  } else {
    TRACE_NET("Socket bound to %s:%d", 
        inet_ntoa(actual_addr.sin_addr), ntohs(actual_addr.sin_port));
  }
  
  /* Set up file descriptor set for select */
  fd_set read_fds;
  int max_fd = config->socket_fd;
  
  /* If signal pipe is active, include it in select */
  if (g_signal_pipe[0] >= 0 && g_signal_pipe[0] > max_fd) {
    max_fd = g_signal_pipe[0];
  }
  
  /* Continue until shutdown is requested */
  while (!g_shutdown_requested) {
    /* Clear and set file descriptor set */
    FD_ZERO(&read_fds);
    FD_SET(config->socket_fd, &read_fds);
    
    /* Add signal pipe to set */
    if (g_signal_pipe[0] >= 0) {
      FD_SET(g_signal_pipe[0], &read_fds);
    }
    
    /* Wait for activity with timeout */
    struct timeval tv;
    tv.tv_sec = 1; /* 1 second timeout */
    tv.tv_usec = 0;
    
    int activity = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
    
    /* Check for errors */
    if (activity < 0) {
      if (errno == EINTR) {
        /* Interrupted by signal, continue */
        continue;
      }
      
      fprintf(stderr, "Error: select() failed: %s\n", strerror(errno));
      break;
    }
    
    /* Check for timeout */
    if (activity == 0) {
      /* No activity, continue */
      continue;
    }
    
    /* Check for signal pipe activity */
    if (g_signal_pipe[0] >= 0 && FD_ISSET(g_signal_pipe[0], &read_fds)) {
      /* Received shutdown signal */
      char buffer[10];
      read(g_signal_pipe[0], buffer, sizeof(buffer));
      printf("Received shutdown signal in accept thread\n");
      break;
    }
    
    /* Check for socket activity */
    if (FD_ISSET(config->socket_fd, &read_fds)) {
      /* Debug: print that we're about to accept a connection */
      TRACE_NET("Detected activity on socket %d, calling accept()...", config->socket_fd);
      
      /* Accept connection */
      client_fd = accept(config->socket_fd, (struct sockaddr*)&client_addr, &client_len);
      
      if (client_fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          /* Non-blocking socket with no connections ready */
          TRACE_NET("accept() returned EAGAIN/EWOULDBLOCK, no connection ready");
          continue;
        }
        
        /* Log more detailed error information */
        if (errno == EMFILE) {
          fprintf(stderr, "Error: accept() failed - too many open files in process\n");
        } else if (errno == ENFILE) {
          fprintf(stderr, "Error: accept() failed - too many open files in system\n");
        } else {
          fprintf(stderr, "Error: accept() failed: %s (errno=%d)\n", strerror(errno), errno);
        }
        
        /* Check if the server socket is still valid */
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(config->socket_fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
          TRACE_NET("Socket error check failed: %s", strerror(errno));
        } else if (error != 0) {
          TRACE_NET("Socket has error condition: %s", strerror(error));
        }
        
        continue;
      }
      
      /* Connection accepted */
      connection_count++;
      printf("Accepted connection #%d from %s:%d\n", 
          connection_count, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
      
      /* Create client connection structure with enhanced safety */
      client_conn_t* client = (client_conn_t*)BUFFER_ALLOC(sizeof(client_conn_t));
      if (!client) {
        fprintf(stderr, "Error: Failed to allocate memory for client connection\n");
        close(client_fd);
        continue;
      }
      
      /* BAR RAISING: Client connections must be promoted when using SSL
       * SSL holds references to client structure that survive checkpoint operations
       * Without promotion, SSL_free() crashes accessing freed client memory
       * See CLAUDE.md v6.3.3 - SSL MEMORY PROMOTION FOR UI STABILITY */
      if (config->use_ssl) {
        memory_promote(client);
      }
      
      /* Initialize client connection with memory safety */
      memset(client, 0, sizeof(client_conn_t)); /* Zero entire structure */
      client->client_fd = client_fd;
      client->address = client_addr;
      client->api_ctx = config->api_ctx;
      
      /* Initialize SSL fields */
      client->use_ssl = config->use_ssl;
      client->ssl_conn = NULL;
      
      /* Log client allocation for debugging */
      if (g_logger) {
        TRACE_NET("CLIENT_ALLOC: allocated client=%p, fd=%d, api_ctx=%p", 
             (void*)client, client_fd, (void*)config->api_ctx);
      }
      
      /* Add client to thread pool using the unified handle_client function */
      if (thread_pool_add_work(config->thread_pool, handle_client, client) != 0) {
        fprintf(stderr, "Error: Failed to add client to thread pool\n");
        BUFFER_FREE(client);
        close(client_fd);
        continue;
      }
      
      /* Log thread pool statistics periodically */
      if (connection_count % 10 == 0) {
        int active_threads = 0;
        int queue_size = 0;
        uint64_t tasks_processed = 0;
        
        thread_pool_stats(config->thread_pool, &active_threads, &queue_size, &tasks_processed);
        
        printf("Thread pool stats: active=%d, queue=%d, processed=%llu\n", 
            active_threads, queue_size, (unsigned long long)tasks_processed);
      }
    }
  }
  
  printf("Accept loop terminated after handling %d connections\n", connection_count);
  
  return NULL;
}

/**
 * Set up signal handlers
 */
static void handle_signals(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = signal_handler;
  
  /* Register signal handlers */
  if (sigaction(SIGINT, &sa, NULL) < 0) {
    fprintf(stderr, "Warning: Failed to register SIGINT handler\n");
  }
  
  if (sigaction(SIGTERM, &sa, NULL) < 0) {
    fprintf(stderr, "Warning: Failed to register SIGTERM handler\n");
  }
  
  if (sigaction(SIGPIPE, &sa, NULL) < 0) {
    fprintf(stderr, "Warning: Failed to register SIGPIPE handler\n");
  }
  
  printf("Signal handlers registered\n");
}

/**
 * Signal handler function
 */
static void signal_handler(int sig) {
  /* Set shutdown flag */
  g_shutdown_requested = 1;
  
  /* Only certain operations are safe in signal handlers */
  const char* signal_name = sig == SIGINT ? "SIGINT" :
               sig == SIGTERM ? "SIGTERM" :
               sig == SIGPIPE ? "SIGPIPE" : "Unknown";
  
  /* Write to stderr (should be safe) */
  char buffer[100];
  snprintf(buffer, sizeof(buffer), "Received signal %s (%d), initiating shutdown\n", signal_name, sig);
  write(STDERR_FILENO, buffer, strlen(buffer));
  
  /* Write to signal pipe (safe) */
  if (g_signal_pipe[1] >= 0) {
    char byte = 1;
    write(g_signal_pipe[1], &byte, 1);
  }
}

/**
 * Request server shutdown from another function
 */
void server_request_shutdown(void) {
  /* Set shutdown flag */
  g_shutdown_requested = 1;
  
  /* Signal through pipe */
  if (g_signal_pipe[1] >= 0) {
    char byte = 1;
    write(g_signal_pipe[1], &byte, 1);
  }
  
  printf("Server shutdown requested\n");
}

/* Removed unused function set_socket_non_blocking */

/* SSL initialization function removed - handled by socket initialization */

/* SSL cleanup function removed - handled inline in server shutdown */

/**
 * Get the SSL context from server configuration for use by client handlers
 * @return SSL context or NULL if SSL is disabled
 */
ssl_context_t* server_get_ssl_context(void) {
  /* Access the global server configuration */
  extern server_config_t* g_server_config;
  
  if (!g_server_config) {
    return NULL;
  }
  
  /* Return the SSL context from the server configuration */
  return g_server_config->ssl_context;
}

/*
 * Legacy compatibility functions (deprecated)
 * These stubs maintain backward compatibility but are not used
 * in the new implementation. All new code should use server_initialize_and_run.
 */

server_status_t server_init(server_config_t* config, struct api_context* api_ctx) {
  fprintf(stderr, "Warning: server_init() is deprecated, use server_initialize_and_run() instead.\n");
  
  /* Store the API context in the server config for sharing with client threads */
  if (config && api_ctx) {
    config->api_ctx = api_ctx;
  }
  
  return SERVER_OK;
}

server_status_t server_start(server_config_t* config) {
  (void)config; /* Mark parameter as unused */
  fprintf(stderr, "Warning: server_start() is deprecated, use server_initialize_and_run() instead.\n");
  return SERVER_OK;
}

void* server_accept_loop(void* config_ptr) {
  (void)config_ptr; /* Mark parameter as unused */
  fprintf(stderr, "Warning: server_accept_loop() is deprecated.\n");
  return NULL;
}

void server_stop(server_config_t* config) {
  (void)config; /* Mark parameter as unused */
  fprintf(stderr, "Warning: server_stop() is deprecated, use server_request_shutdown() instead.\n");
  server_request_shutdown();
}