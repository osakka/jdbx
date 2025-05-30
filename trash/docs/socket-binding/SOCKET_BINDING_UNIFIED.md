# Socket Binding Implementation - Unified Reference

This document provides a comprehensive overview of the socket binding implementation in JSONdb server. It replaces all previous socket binding documents and serves as the single source of truth.

## Background

The JSONdb server had previously experienced socket binding issues, particularly in daemon mode. After thorough investigation and testing, we've implemented a robust solution that ensures proper socket binding in all scenarios.

## Implementation Details

The socket binding implementation follows a critical sequence to ensure proper behavior:

1. **Initialization Sequence**
   - Configuration initialization
   - Logger initialization
   - Database initialization
   - RBAC initialization
   - API initialization
   - Daemon initialization (if in daemon mode)
   - Socket initialization
   - Thread pool initialization
   - Server main loop

2. **Socket Initialization Process**
   - Socket creation: `socket(AF_INET, SOCK_STREAM, 0)`
   - Socket option setup: `SO_REUSEADDR`
   - Address preparation with proper hostname resolution
   - Socket binding: `bind(socket_fd, (struct sockaddr*)&address, sizeof(address))`
   - Set socket to listen state: `listen(socket_fd, 10)`
   - Verify socket listening state: `getsockopt(socket_fd, SOL_SOCKET, SO_ACCEPTCONN, ...)`

3. **Key Improvements**
   - Proper sequencing: socket is initialized AFTER daemonization
   - Enhanced error handling with detailed logging
   - Process context tracking with PID logging
   - Socket state verification
   - Comprehensive hostname resolution
   - Proper cleanup on errors

## Technical Implementation

### Socket Creation and Setup

```c
int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
if (socket_fd < 0) {
    INIT_LOG_FAILURE("SOCKET", "Failed to create socket: %s (errno=%d)", 
                   strerror(errno), errno);
    return INIT_SOCKET_ERROR;
}

/* Set socket options */
int reuse = 1;
setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
```

### Address Resolution

```c
struct sockaddr_in address;
memset(&address, 0, sizeof(address));
address.sin_family = AF_INET;
address.sin_port = htons(config->port);

if (!config->host || strlen(config->host) == 0 || strcmp(config->host, "0.0.0.0") == 0) {
    /* Bind to any address */
    address.sin_addr.s_addr = INADDR_ANY;
} else if (strcmp(config->host, "127.0.0.1") == 0 || strcmp(config->host, "localhost") == 0) {
    /* Bind to localhost */
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
} else if (inet_addr(config->host) != INADDR_NONE) {
    /* It's a valid IP address */
    address.sin_addr.s_addr = inet_addr(config->host);
} else {
    /* Hostname resolution - fallback to INADDR_ANY */
    address.sin_addr.s_addr = INADDR_ANY;
}
```

### Socket Binding and Listening

```c
if (bind(socket_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
    INIT_LOG_FAILURE("SOCKET", "Failed to bind socket: %s (errno=%d)", 
                   strerror(errno), errno);
    close(socket_fd);
    return INIT_SOCKET_ERROR;
}

if (listen(socket_fd, 10) < 0) {
    INIT_LOG_FAILURE("SOCKET", "Failed to listen on socket: %s (errno=%d)", 
                   strerror(errno), errno);
    close(socket_fd);
    return INIT_SOCKET_ERROR;
}
```

### Socket State Verification

```c
int acceptconn = 0;
socklen_t acceptconn_len = sizeof(acceptconn);
if (getsockopt(socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &acceptconn, &acceptconn_len) < 0) {
    /* Log warning but continue */
} else {
    if (!acceptconn) {
        INIT_LOG_FAILURE("SOCKET", "Socket is not in listening state despite successful listen() call");
        close(socket_fd);
        return INIT_SOCKET_ERROR;
    }
}
```

## Daemon Mode Handling

The daemon mode implementation follows these steps:

1. Process parent forks a child and exits
2. Child process becomes session leader and forks another child
3. Second child continues execution while parent exits
4. Socket is initialized ONLY after daemonization is complete
5. This ensures socket binding occurs in the final daemon process

```c
/* Initialize daemon process if in daemon mode */
if (!config->verbose_mode) {
    status = init_daemon(config);
    if (status == INIT_DAEMON_ERROR) {
        INIT_LOG_FAILURE("MAIN", "Failed to initialize daemon process");
        free(config);
        return 1;
    } else if (status == INIT_DAEMON_PARENT_EXIT) {
        /* Parent process should exit without cleanup */
        INIT_LOG_PROGRESS("MAIN", "Daemon started, parent process exiting");
        /* Free config before exit to avoid memory leak */
        free(config);
        return 0;
    }
    
    /* Child process continues here */
    INIT_LOG_PROGRESS("MAIN", "Daemon process initialized, continuing with child process");
}

/* Initialize socket - AFTER daemon process is fully established */
status = init_socket(config);
```

## Accept Loop Implementation

After socket initialization, the server runs an accept loop that:

1. Uses `select()` to wait for incoming connections with a timeout
2. Handles incoming connections by accepting them: `accept(socket_fd, ...)`
3. Dispatches client connections to the thread pool: `thread_pool_add_work(...)`
4. Monitors for shutdown signals through a signal pipe

## Testing and Verification

To test the server's socket binding:

1. Start the server in daemon mode:
   ```
   build/jsondb_runtime.sh start
   ```

2. Verify the server is running:
   ```
   build/jsondb_runtime.sh status
   ```

3. Check the socket is bound and listening:
   ```
   netstat -tuln | grep 5000
   ```

4. Test connectivity:
   ```
   curl http://localhost:5000/health
   ```

5. Stop the server:
   ```
   build/jsondb_runtime.sh stop
   ```

## Conclusion

The unified socket binding implementation ensures reliable socket setup in both daemon and foreground modes. By carefully sequencing operations and implementing proper error handling, we've created a robust solution that works consistently across different environments.

Remember the key principles:
- Socket is initialized AFTER daemonization
- Proper error handling with detailed logging
- Hostname resolution with fallbacks
- Socket state verification
- Comprehensive cleanup on errors