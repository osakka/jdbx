# Socket Binding Fix Documentation

## Overview

This document explains the socket binding fix implemented to solve API request issues in the JSONdb server. The primary issue was that API requests were failing with 500 Internal Server Error responses because the API context wasn't properly shared between the main process and client handler threads.

## Root Cause

The root cause of the issue was that we were forking the daemon process before fully initializing the socket and API context. This led to problems with the socket binding and API context sharing.

## Solution

The solution involves three key changes:

1. Update the server initialization sequence to ensure the API context is created and properly shared
2. Ensure the socket is created, bound, and set to listening state BEFORE forking the daemon process
3. Verify and preserve the socket file descriptor across the fork operation

## Implementation Details

### Server Initialization

The `server_init` function in `server.c` was updated to accept an API context parameter and store it in the server configuration:

```c
server_status_t server_init(server_config_t* config, api_context_t* api_ctx) {
    // ...
    /* Store the API context in the server config for sharing with client threads */
    config->api_ctx = api_ctx;
    // ...
}
```

### Server Start

The `server_start` function was enhanced to verify the socket file descriptor and recreate it if necessary:

```c
server_status_t server_start(server_config_t* config) {
    // ...
    /* Create a new socket if the existing one is invalid */
    if (config->socket_fd <= 0) {
        // Socket recreation code
    }
    // ...
}
```

### Client Connection Handling

The client connection structure (`client_conn_t`) was updated to include a reference to the API context:

```c
typedef struct {
    int client_fd;
    struct sockaddr_in address;
    pthread_t thread;
    struct api_context *api_ctx;  /* Reference to the API context */
} client_conn_t;
```

### Accept Thread

The `server_accept_loop` function was updated to verify that the API context is available:

```c
void* server_accept_loop(void* config_ptr) {
    // ...
    /* Verify API context is available */
    if (!config->api_ctx) {
        fprintf(stderr, "Error: NULL API context in accept loop\n");
        // Signal initialization failure
        // ...
        return NULL;
    }
    // ...
}
```

### Initialization Sequence

The initialization sequence in `main.c` ensures the proper order of operations:

1. Initialize the database and RBAC system
2. Create the API context
3. Register API routes
4. Initialize the server with the API context
5. Start the server (bind and listen)
6. Only then fork the daemon process (if in daemon mode)

## Benefits

This fix ensures that:

1. The API context is properly shared between the main process and client handler threads
2. The socket is correctly bound and in listening state before forking
3. Socket file descriptors are properly preserved across the fork operation

## Testing

To test the fix, restart the server and use curl to test the API endpoints:

```bash
cd /opt/jsondb && make -C src
./build/jsondb_runtime.sh restart
curl -i http://localhost:5000/api/health
```

A successful response from the health endpoint indicates that the API context is now properly shared with client handler threads.