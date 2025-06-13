# JSON Database Server Usage Guide

This document explains how to properly use the JSON Database Server with its binary-relative path feature and daemon mode.

## Recommended Method: Using the Runtime Script

The easiest way to manage the JDBX server is using the `jdbx_runtime.sh` script:

```bash
# Start the server
./build/jdbx_runtime.sh start

# Check server status
./build/jdbx_runtime.sh status

# Stop the server
./build/jdbx_runtime.sh stop

# Restart the server
./build/jdbx_runtime.sh restart
```

The runtime script:
- Sets up all necessary environment variables and paths
- Configures QuickJS library paths if needed
- Handles proper startup and shutdown procedures
- Verifies server status before and after operations
- Provides simple start/stop/status/restart commands

## Path Resolution

The JSON Database Server uses binary-relative paths for all operations. This means:

1. All paths are resolved relative to the location of the `jdbxd` binary.
2. When the binary is in `/path/to/bin/jdbxd`, all relative paths are resolved relative to `/path/to/`.
3. This makes the server more portable as it can be installed anywhere without modifying configuration files.

## Directory Structure

The server expects the following directory structure relative to the binary location:

```
/path/to/bin/jdbxd    # The server binary
/path/to/var/                 # Root for all variable data
  ├── data/jdbx/            # Database files
  │     └── db.json           # Main database file
  │     └── rbac.json         # RBAC configuration
  ├── log/jdbx/             # Log files
  │     └── server.log        # Main server log
  └── run/                    # Runtime files
        └── jdbxd.pid # PID file when running
```

The server will create these directories automatically if they don't exist.

## Direct Command-Line Usage

You can also control the server directly using command-line arguments, though using the runtime script is recommended for most scenarios.

### Basic Operation

```bash
# Start the server in daemon mode (default)
./bin/jdbxd

# Start the server in foreground mode (for debugging only)
./bin/jdbxd --foreground

# Check server status
./bin/jdbxd --status

# Stop a running server
./bin/jdbxd --stop
```

### Customizing Paths

All paths are relative to the binary location by default, but you can override them:

```bash
# Use custom PID file
./bin/jdbxd --pid-file custom/path/to/server.pid

# Use custom log file
./bin/jdbxd --log-file custom/path/to/server.log

# Use absolute paths
./bin/jdbxd --pid-file /absolute/path/to/server.pid --log-file /absolute/path/to/server.log
```

### Other Options

```bash
# Set a custom port
./bin/jdbxd --port 8080

# Set a specific host
./bin/jdbxd --host my.domain.com

# Run JavaScript file and exit
./bin/jdbxd --js script.js

# Set log level
./bin/jdbxd --log-level debug
```

## Server State Management

### Starting the Server

When starting the server, it checks:
1. If there's a PID file and the process with that PID is running
2. If any other `jdbxd` process is running (even without a PID file)
3. In both cases, it will refuse to start if another instance is already running

### Stopping the Server

When stopping the server:
1. It first tries to use the PID from the PID file
2. If no PID file exists, it tries to find the server process by name
3. When successful, it sends SIGTERM to gracefully shut down the server

### Daemon Mode (Default)

The server now runs in daemon mode by default for production use:
1. The parent process forks and exits
2. The child process continues running in the background
3. Standard I/O is redirected to the log file
4. The PID file is created with proper file locking
5. JavaScript engine is initialized after the daemon process is created

### Foreground Mode (For Debugging)

Foreground mode is now only recommended for debugging:
1. The server runs in the current terminal
2. Output is displayed in the terminal
3. You can use Ctrl+C to stop the server
4. No forking is performed, making it easier to debug

## PID File Handling

The server has improved PID file handling for reliability:
1. Stale PID files (from crashed server instances) are detected and cleaned up
2. The server verifies a PID exists before acting on it
3. PID files are properly locked to prevent race conditions
4. Multiple attempts are made to remove PID files if needed

## Common Issues

- **Server already running**: Ensure no other server instance is running with `./build/jdbx_runtime.sh status`
- **Cannot write to PID file**: Ensure the `/path/to/var/run/` directory exists and is writable
- **Cannot write to log file**: Ensure the `/path/to/var/log/jdbx/` directory exists and is writable
- **Stale PID file**: If the server crashed previously, restart it with `./build/jdbx_runtime.sh restart` to clean up stale PID files

## Best Practices

1. Use the `jdbx_runtime.sh` script for managing the server in all environments
2. Always use proper control commands rather than manually killing processes
3. When installing, ensure the server has write permissions to its var/ directory
4. Check the log file when troubleshooting unexpected behavior
5. Use foreground mode only for development and debugging
6. For production deployment, consider using a process monitor like systemd

## Configuration File Support

The server can read settings from a configuration file. Place the `config.json` file in the same directory as the executable or specify the path with `--config <path>`.

## Version Information

Use `--version` to display the version information:

```bash
./bin/jdbxd --version
```