# JSONdb Socket Binding Debug Tools

This directory contains tools and utilities for diagnosing and debugging socket binding issues in the JSONdb server.

## Background

The JSONdb server has experienced issues with socket binding, where the server process starts but fails to bind to the expected port. These tools help to isolate and diagnose the root cause of these issues.

## Available Tools

### 1. Diagnostic Scripts

- **diagnose_server_socket.sh**: Comprehensive diagnostic script that tests multiple aspects of socket binding and produces a detailed report.
- **build_debug_tools.sh**: Script to compile and prepare all debugging tools.
- **build_socket_diagnostics.sh**: Builds the new socket diagnostics tool that provides detailed information about socket binding capabilities.

### 2. Socket Testing Utilities

- **test_port_binding.c**: Standalone utility to test basic TCP socket binding.
- **server_thread_debug.c**: Thread monitoring utility for debugging server thread issues.
- **process_monitor.c**: Process monitoring utility for tracking server process state.
- **socket_diagnostics.c**: Comprehensive socket diagnostic tool that tests interfaces, port availability, and hostname resolution.

### 3. Server Code Enhancements

- **server_thread_debug.patch**: Patch for adding extensive thread state tracking to the server code.

## Usage

### Running the Comprehensive Diagnostic

```bash
cd /opt/jsondb/scripts/debug
./diagnose_server_socket.sh
```

This will run a series of tests and produce a detailed diagnostic report in `/opt/jsondb/build/debug/logs/`.

### Building the Debug Tools

```bash
cd /opt/jsondb/scripts/debug
./build_debug_tools.sh
```

This compiles all the debug utilities and places them in `/opt/jsondb/build/debug/tools/`.

### Using the New Socket Diagnostics Tool

```bash
# Build the socket diagnostics tool
cd /opt/jsondb/scripts/debug
./build_socket_diagnostics.sh

# Run with default settings (port 5000, host localhost)
/opt/jsondb/build/debug/socket_diagnostics

# Specify custom port
/opt/jsondb/build/debug/socket_diagnostics --port=5001

# Specify custom host
/opt/jsondb/build/debug/socket_diagnostics --host=127.0.0.1
```

The socket diagnostics tool provides detailed information about:

1. **Network Interfaces** - Lists all available interfaces with their IP addresses and flags
2. **Socket Creation Test** - Tests basic socket creation and option setting
3. **Port Usage Check** - Verifies if the specified port is available for binding
4. **Hostname Resolution** - Tests whether the specified hostname can be resolved to IP addresses

### Using Individual Tools

After building the debug tools:

```bash
# Test basic port binding
/opt/jsondb/build/debug/tools/test_port_binding 5000

# Run thread monitor test
/opt/jsondb/build/debug/tools/thread_monitor

# Run process monitor test
/opt/jsondb/build/debug/tools/process_monitor
```

## Understanding Socket Issues

The main sources of socket binding issues are:

1. **Race Conditions**: Socket creation, binding, and thread creation happening in an incorrect order.
2. **File Descriptor Handling**: Socket descriptors not being properly preserved during process forking in daemon mode.
3. **Thread Management**: Issues with thread creation and detachment affecting the accept loop.
4. **Error Visibility**: Errors occurring after standard output/error are closed in daemon mode.
5. **Port Conflicts**: Other services using the same port the server is trying to bind to.
6. **Hostname Resolution**: Issues resolving the hostname specified for binding.

## Logging

All diagnostic tools produce detailed logs:

- Main diagnostic logs: `/opt/jsondb/build/debug/logs/`
- Individual tool logs: `/tmp/` directory with appropriate prefixes
- Server socket logs: Look for `[INIT:SOCKET]` entries in the server log file

## Applying Server Enhancements

To apply the server thread debug patch:

```bash
cd /opt/jsondb/src/components/core
patch -p0 < /opt/jsondb/scripts/debug/server_thread_debug.patch
```

## Common Issues and Solutions

1. **Binding Fails in Daemon Mode**: 
   - Ensure socket binding happens before closing standard file descriptors
   - Preserve the socket descriptor during the fork operation

2. **Thread Creation Issues**:
   - Verify thread attributes initialization
   - Check for proper error handling in thread creation
   - Ensure thread gets the correct socket descriptor

3. **Connection Acceptance Issues**:
   - Verify the socket is in LISTENING state
   - Check that the accept thread is actually running
   - Ensure the correct address/port is being used

4. **Port Already in Use**:
   - Check if another process is using the port with `netstat -tulpn | grep PORT`
   - Change the port in the configuration
   - Ensure the server properly releases the port when stopping

5. **Hostname Resolution Issues**:
   - Use IP addresses instead of hostnames if resolution is unreliable
   - Verify the hostname can be resolved properly
   - Use "0.0.0.0" to bind to all interfaces if needed

## Additional Resources

- [SOCKET_BINDING_STATUS.md](/opt/jsondb/docs/socket-binding/SOCKET_BINDING_STATUS.md): Current status of socket binding fixes
- [socket_binding_summary.md](/opt/jsondb/docs/socket-binding/socket_binding_summary.md): Summary of socket binding issues and fixes
- [SOCKET_DIAGNOSTICS.md](/opt/jsondb/docs/socket-binding/SOCKET_DIAGNOSTICS.md): Comprehensive guide to socket diagnostic procedures