# JSONdb Socket Binding Debug Tools

This directory contains tools and utilities for diagnosing and debugging socket binding issues in the JSONdb server.

## Background

The JSONdb server has experienced issues with socket binding, where the server process starts but fails to bind to the expected port. These tools help to isolate and diagnose the root cause of these issues.

## Available Tools

### 1. Diagnostic Scripts

- **diagnose_server_socket.sh**: Comprehensive diagnostic script that tests multiple aspects of socket binding and produces a detailed report.
- **build_debug_tools.sh**: Script to compile and prepare all debugging tools.

### 2. Socket Testing Utilities

- **test_port_binding.c**: Standalone utility to test basic TCP socket binding.
- **server_thread_debug.c**: Thread monitoring utility for debugging server thread issues.
- **process_monitor.c**: Process monitoring utility for tracking server process state.

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

## Logging

All diagnostic tools produce detailed logs:

- Main diagnostic logs: `/opt/jsondb/build/debug/logs/`
- Individual tool logs: `/tmp/` directory with appropriate prefixes

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

## Additional Resources

- [SOCKET_BINDING_STATUS.md](/opt/jsondb/docs/socket-binding/SOCKET_BINDING_STATUS.md): Current status of socket binding fixes
- [socket_binding_summary.md](/opt/jsondb/docs/socket-binding/socket_binding_summary.md): Summary of socket binding issues and fixes