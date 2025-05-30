# JSONdb Socket Binding Reference

**Version**: 2.0.7  
**Last Updated**: January 2025

This comprehensive reference documents the socket binding implementation in JSONdb server, consolidating all socket binding documentation into a single source of truth.

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Implementation Details](#implementation-details)
4. [Configuration](#configuration)
5. [Troubleshooting](#troubleshooting)
6. [Testing](#testing)
7. [Historical Context](#historical-context)

## Overview

The JSONdb server uses a robust socket binding implementation that ensures reliable network connectivity in both foreground and daemon modes. The implementation has been thoroughly tested and addresses all known edge cases.

### Key Features

- **Reliable Binding**: Guaranteed socket binding with comprehensive error handling
- **Daemon Support**: Proper socket handling across process boundaries
- **State Verification**: Active verification of socket state before accepting connections
- **Detailed Logging**: Comprehensive logging for debugging and monitoring
- **Graceful Recovery**: Automatic recovery from common socket errors

## Architecture

### Initialization Sequence

The socket binding implementation follows a critical initialization sequence:

```
1. Configuration initialization
2. Logger initialization  
3. Database initialization
4. RBAC initialization
5. API initialization
6. Daemon initialization (if in daemon mode)
7. Socket initialization (in the final process)
8. Thread pool initialization
9. Server main loop
```

### Process Flow

```
┌─────────────────┐
│ Server Start    │
└────────┬────────┘
         │
         v
┌─────────────────┐
│ Init Config     │
└────────┬────────┘
         │
         v
┌─────────────────┐
│ Init Logger     │
└────────┬────────┘
         │
         v
┌─────────────────┐
│ Init Database   │
└────────┬────────┘
         │
         v
┌─────────────────┐
│ Init RBAC       │
└────────┬────────┘
         │
         v
┌─────────────────┐
│ Init API        │
└────────┬────────┘
         │
         v
┌─────────────────┐     ┌─────────────────┐
│ Daemon Mode?    │────>│ Fork Process    │
└────────┬────────┘ Yes └────────┬────────┘
         │ No                     │
         v                        v
┌─────────────────────────────────┐
│     Init Socket (Final Process) │
└────────┬────────────────────────┘
         │
         v
┌─────────────────┐
│ Init Thread Pool│
└────────┬────────┘
         │
         v
┌─────────────────┐
│ Server Main Loop│
└─────────────────┘
```

## Implementation Details

### Socket Creation

The socket is created using the standard BSD socket API:

```c
int init_socket(const char* host, int port, InitStatus* status) {
    // Create socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        INIT_LOG_FAILURE("SOCKET", "Failed to create socket: %s (errno=%d)", 
                       strerror(errno), errno);
        return INIT_SOCKET_ERROR;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        INIT_LOG_WARNING("SOCKET", "Failed to set SO_REUSEADDR: %s", strerror(errno));
    }
    
    // Prepare address structure
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    
    // Resolve hostname
    if (!resolve_hostname(host, &address.sin_addr)) {
        INIT_LOG_WARNING("SOCKET", "Failed to resolve hostname '%s', using INADDR_ANY", host);
        address.sin_addr.s_addr = INADDR_ANY;
    }
    
    // Bind socket
    if (bind(socket_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        INIT_LOG_FAILURE("SOCKET", "Failed to bind socket: %s (errno=%d)", 
                       strerror(errno), errno);
        close(socket_fd);
        return INIT_SOCKET_ERROR;
    }
    
    // Set socket to listen
    if (listen(socket_fd, 10) < 0) {
        INIT_LOG_FAILURE("SOCKET", "Failed to listen on socket: %s", strerror(errno));
        close(socket_fd);
        return INIT_SOCKET_ERROR;
    }
    
    // Verify socket state
    int listening = 0;
    socklen_t len = sizeof(listening);
    if (getsockopt(socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &listening, &len) == 0) {
        INIT_LOG_DEBUG("SOCKET", "Socket state: %s", listening ? "LISTENING" : "NOT LISTENING");
    }
    
    INIT_LOG_SUCCESS("SOCKET", "Server listening on %s:%d (PID: %d)", 
                   inet_ntoa(address.sin_addr), port, getpid());
    
    return socket_fd;
}
```

### Hostname Resolution

The implementation includes robust hostname resolution with fallback:

```c
int resolve_hostname(const char* hostname, struct in_addr* addr) {
    // Try direct IP address conversion
    if (inet_aton(hostname, addr)) {
        return 1;
    }
    
    // Try hostname resolution
    struct hostent* he = gethostbyname(hostname);
    if (he && he->h_addr_list[0]) {
        memcpy(addr, he->h_addr_list[0], sizeof(struct in_addr));
        return 1;
    }
    
    // Special cases
    if (strcmp(hostname, "localhost") == 0) {
        addr->s_addr = inet_addr("127.0.0.1");
        return 1;
    }
    
    if (strcmp(hostname, "*") == 0 || strcmp(hostname, "0.0.0.0") == 0) {
        addr->s_addr = INADDR_ANY;
        return 1;
    }
    
    return 0;
}
```

### Socket State Verification

Before entering the accept loop, the socket state is verified:

```c
void verify_socket_state(int socket_fd) {
    int listening = 0;
    socklen_t len = sizeof(listening);
    
    if (getsockopt(socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &listening, &len) == 0) {
        if (!listening) {
            LOG_ERROR("Socket is not in listening state!");
            exit(1);
        }
    }
    
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if (getsockname(socket_fd, (struct sockaddr*)&addr, &addr_len) == 0) {
        LOG_INFO("Socket bound to %s:%d", 
               inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    }
}
```

## Configuration

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `JSONDB_HOST` | Server binding address | `0.0.0.0` |
| `JSONDB_PORT` | Server listening port | `5000` |

### Configuration File

```json
{
  "server": {
    "host": "0.0.0.0",
    "port": 5000
  }
}
```

### Binding Options

- `0.0.0.0` - Bind to all available interfaces (default)
- `127.0.0.1` - Bind to localhost only
- `192.168.1.100` - Bind to specific IP address
- `localhost` - Bind to localhost (resolved to 127.0.0.1)
- `*` - Bind to all interfaces (same as 0.0.0.0)

## Troubleshooting

### Common Issues

#### Port Already in Use

**Symptom**: `bind: Address already in use`

**Solutions**:
1. Check if another process is using the port:
   ```bash
   sudo lsof -i :5000
   ```
2. Stop the conflicting process or change the port
3. Wait for TIME_WAIT sockets to expire (usually 60 seconds)

#### Permission Denied

**Symptom**: `bind: Permission denied`

**Solutions**:
1. Use a port above 1024 (non-privileged)
2. Run with appropriate permissions for privileged ports
3. Check SELinux/AppArmor policies

#### Cannot Assign Requested Address

**Symptom**: `bind: Cannot assign requested address`

**Solutions**:
1. Verify the IP address exists on the system:
   ```bash
   ip addr show
   ```
2. Use `0.0.0.0` to bind to all interfaces
3. Check network configuration

### Diagnostic Commands

```bash
# Check if server is listening
netstat -tlnp | grep 5000
ss -tlnp | grep 5000

# Test connection
nc -zv localhost 5000
telnet localhost 5000

# Check socket state
lsof -i :5000
```

### Debug Logging

Enable debug logging for detailed socket information:

```bash
export JSONDB_LOG_LEVEL=debug
./bin/jsondb_server
```

## Testing

### Manual Testing

1. **Start Server**
   ```bash
   build/jsondb_runtime.sh start
   ```

2. **Verify Socket**
   ```bash
   netstat -tlnp | grep 5000
   ```

3. **Test Connection**
   ```bash
   curl http://localhost:5000/api/health
   ```

### Automated Testing

```bash
# Run socket binding tests
scripts/test_socket_binding.sh

# Test daemon mode specifically
scripts/test_daemon_socket_fix.sh
```

### Test Scenarios

1. **Basic Binding**
   - Start server on default port
   - Verify listening state
   - Test API connectivity

2. **Port Conflict**
   - Start first instance
   - Attempt to start second instance
   - Verify appropriate error handling

3. **Daemon Mode**
   - Start in daemon mode
   - Verify PID file creation
   - Test socket connectivity
   - Verify clean shutdown

4. **Interface Binding**
   - Test binding to localhost
   - Test binding to specific IP
   - Test binding to all interfaces

## Historical Context

### Previous Issues

The socket binding implementation has evolved to address several historical issues:

1. **Daemon Mode Failures** (Fixed in v2.0.3)
   - Sockets were initialized before forking
   - Child process inherited invalid file descriptors
   - Solution: Initialize sockets after daemonization

2. **Hostname Resolution** (Fixed in v2.0.4)
   - Server failed to start with certain hostnames
   - No fallback for resolution failures
   - Solution: Comprehensive hostname resolution with fallbacks

3. **Socket State Verification** (Added in v2.0.5)
   - Server accepted connections before fully initialized
   - Race conditions in thread pool startup
   - Solution: Verify socket state before accepting

4. **Resource Cleanup** (Improved in v2.0.6)
   - Sockets remained in TIME_WAIT after crashes
   - Port remained unavailable for rebinding
   - Solution: SO_REUSEADDR and proper cleanup handlers

### Design Decisions

1. **Initialization Order**: Sockets are initialized after all other components to ensure the server is ready to handle connections immediately.

2. **Error Recovery**: Comprehensive error handling with detailed logging helps diagnose issues quickly.

3. **State Verification**: Active verification of socket state prevents subtle bugs and race conditions.

4. **Process Awareness**: The implementation tracks process context (PID) to handle daemon mode correctly.

## References

- [init_socket() implementation](src/initialize/socket.c)
- [Server main loop](src/components/core/server.c)
- [Configuration defaults](src/include/utils/config_defaults.h)
- [Test scripts](scripts/test_socket_binding.sh)