# Socket Initialization Patch for JSONdb Server

## Problem Identified

The server is failing at socket initialization with the following error:

```
[ERROR] [server.c:121:server_initialize_and_run] Failed to initialize server socket
```

Upon investigating the socket initialization logic and execution flow, I've identified that the server tries to initialize a socket in two different contexts:

1. First in `init_socket()` in `/opt/jsondb/src/initialize/socket.c` - called from main.c
2. Then again in `server_initialize_and_run()` in `/opt/jsondb/src/components/core/server.c` - called during `run_server()`

This double socket initialization is causing a race condition or conflict, as the server is trying to create, bind, and listen on the same port twice.

## Solution

We need to ensure that the socket initialization only happens once, and we consistently use the same socket descriptor throughout the server lifecycle.

### Changes Required:

1. Modify `server_initialize_and_run()` to skip the internal socket initialization call if the socket is already initialized.

2. Ensure socket descriptor is properly passed between components.

3. Add additional validation to detect if a socket is already bound to the port.

## Implementation

Here's the patch for `/opt/jsondb/src/components/core/server.c`:

```c
/* Initialize socket ONLY if not already initialized */
if (config->socket_fd <= 0) {
    if (g_logger) {
        LOG_INFO("Initializing socket on %s:%d", 
                config->host ? config->host : "0.0.0.0", config->port);
    } else {
        printf("Starting JSONdb server on port %d...\n", config->port);
    }
    
    if (init_socket(config) != INIT_OK) {
        if (g_logger) {
            LOG_ERROR("Failed to initialize server socket");
        } else {
            fprintf(stderr, "Error: Failed to initialize server socket\n");
        }
        return SERVER_SOCKET_ERROR;
    }
} else {
    /* Socket already initialized */
    if (g_logger) {
        LOG_INFO("Using pre-initialized socket (fd=%d)", config->socket_fd);
    } else {
        printf("Using pre-initialized socket (fd=%d)\n", config->socket_fd);
    }
}
```

Replace the current code around line 111-126 in `server_initialize_and_run()` with this patched code.

## Initialization Sequence Recommendations

To maintain a consistent, error-free initialization sequence, I recommend the following best practices:

1. **Single Point of Initialization**: Each system component should be initialized exactly once and in a well-defined sequence.

2. **Proper Dependency Management**: Initialize components in proper order of dependencies. For example, socket initialization should happen after daemonization but before thread pool creation.

3. **Clean Resource Cleanup**: All components that allocate resources should have corresponding cleanup functions registered with atexit() or handled in a central cleanup function.

4. **Validation Steps**: Add validation between initialization steps to catch issues early.

5. **Context-Aware Configuration**: Be mindful of what process context each initialization step runs in, especially when using fork().

## Recommended Initialization Sequence

Based on analysis of the JSONdb codebase, I recommend the following strict initialization sequence:

1. Config initialization
2. Logger initialization 
3. Daemon process initialization (if in daemon mode)
4. Socket initialization (ONCE, in the final daemon child process)
5. Database initialization
6. Thread pool initialization
7. RBAC initialization
8. API initialization
9. Server main loop

This sequence ensures that each component is initialized in the proper context and with all necessary dependencies available.

## Additional Safeguards

Add the following safeguards to prevent initialization issues:

1. Add status flags to track initialization state of each component.
2. Add validation checks before accessing each component.
3. Implement proper error handling and cleanup for each initialization step.
4. Add timeouts for operations that may hang or block indefinitely.
5. Log detailed debugging information during each initialization step.

## Testing the Fix

To test the fix, apply the patch and:

1. Rebuild the server:
   ```
   cd /opt/jsondb/src && make
   ```

2. Start the server in daemon mode:
   ```
   cd /opt/jsondb && build/jsondb_runtime.sh start
   ```

3. Check server status:
   ```
   cd /opt/jsondb && build/jsondb_runtime.sh status
   ```

4. Verify logs for proper socket initialization:
   ```
   cat /opt/jsondb/build/var/jsondb_server.log | grep -A 10 "Using pre-initialized socket"
   ```

The server should now successfully start and bind to the configured port without errors.