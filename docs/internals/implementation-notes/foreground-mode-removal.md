# Foreground Mode Removal Plan

## Overview

This document outlines the complete removal of foreground mode from the JSONdb server. The server will now exclusively operate in daemon mode, with all messages going through the logging framework.

## Rationale

1. **Consistency**: Having a single operational mode ensures consistent behavior across all deployments.
2. **Reliability**: Early daemon initialization prevents TTY-related issues.
3. **Logging**: All messages are properly captured in log files, making debugging and monitoring easier.
4. **Code simplification**: Removing conditional paths based on foreground vs daemon mode simplifies the codebase.

## Implementation Steps

1. **Remove command-line options**:
   - Remove `-f/--foreground` flag
   - Replace with `-v/--verbose` to control log verbosity

2. **Consolidate daemon initialization**:
   - Move daemon initialization early in the startup sequence
   - Ensure all file descriptors are properly handled

3. **Standardize logging**:
   - Initialize logging system immediately after daemonization
   - Direct all output through the logging framework
   - Add log level configuration options

4. **Remove conditional paths**:
   - Eliminate all `if (foreground_mode)` conditionals
   - Standardize server initialization sequence

## Technical Details

### 1. Daemon Initialization

The daemonization process will be moved to happen early in the program, right after basic command-line argument processing:

1. Parse command-line arguments
2. Handle help, version, and termination requests
3. Daemonize the process
4. Initialize logging
5. Continue with server initialization

### 2. Socket Handling

Socket creation, binding, and listening will use a single, consistent path:

1. Create socket
2. Set socket options
3. Bind to address/port
4. Listen for connections

### 3. Signal Handling

Signal handling will be standardized to use a long-running loop in daemon mode.

### 4. Console Output Removal

All `printf` and `fprintf` calls displaying server status will be replaced with logging calls.

## Benefits

1. Simplified code with fewer conditionals
2. All server output properly captured in logs
3. Improved reliability due to proper TTY detachment
4. Consistent behavior across deployments
5. Easier debugging through comprehensive logs

## Migration Guide for Users

Users previously using foreground mode should:

1. Use `-v/--verbose` for detailed logging output
2. Monitor log files instead of console output
3. Use `tail -f <logfile>` for real-time log viewing

## Implementation Timeline

This change will be implemented immediately and included in the next release.