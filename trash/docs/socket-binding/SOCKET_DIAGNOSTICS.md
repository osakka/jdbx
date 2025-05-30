# Socket Binding Diagnostics

This document outlines procedures for diagnosing and resolving socket binding issues in the JSONdb server.

## Common Socket Binding Issues

Socket binding issues typically manifest as:

1. Server failing to start with "Failed to bind socket" errors
2. Server unable to accept connections
3. Intermittent connection failures
4. Port conflicts with other applications

## Diagnostic Tools

### Socket Diagnostics Utility

The project includes a specialized diagnostic tool that can identify common socket issues:

```bash
# Build the tool if needed
/opt/jsondb/scripts/debug/build_socket_diagnostics.sh

# Run with default settings (port 5000)
/opt/jsondb/build/debug/socket_diagnostics

# Run with custom port and host
/opt/jsondb/build/debug/socket_diagnostics --port=5001 --host=127.0.0.1
```

The diagnostics tool provides information about:

- Network interfaces available on the system
- Socket creation capabilities
- Port availability and binding status
- Hostname resolution

### Core System Commands

Beyond the diagnostics tool, these standard commands are useful for troubleshooting socket issues:

**Check port usage:**
```bash
# Using netstat
netstat -tulpn | grep 5000

# Using lsof (List Open Files)
lsof -i :5000

# Using ss (Socket Statistics)
ss -tulpn | grep 5000
```

**Check network interfaces:**
```bash
# Show all interfaces
ifconfig -a

# Show just IP addresses
ip addr show
```

**Test hostname resolution:**
```bash
# Basic hostname lookup
nslookup localhost

# Verbose hostname info
host -v localhost
```

## Common Issues and Solutions

### 1. "Address Already in Use" Errors

**Problem**: The server cannot bind to a port because another process is using it.

**Diagnosis**:
```bash
# Find the process using the port
lsof -i :5000
```

**Solutions**:
- Change the port in the configuration using `JSONDB_PORT` environment variable
- Stop the conflicting service
- Restart the machine if needed

### 2. Permission-Related Issues

**Problem**: The server cannot bind to a port below 1024 due to insufficient privileges.

**Diagnosis**:
```bash
# Check if the port is privileged
if [ $PORT -lt 1024 ]; then echo "Privileged port"; fi
```

**Solutions**:
- Run the server with elevated privileges (not recommended for production)
- Configure the server to use a non-privileged port (>1024)
- Use port forwarding from a privileged port to a non-privileged port

### 3. Hostname Resolution Issues

**Problem**: The server cannot resolve the hostname specified in the configuration.

**Diagnosis**:
```bash
# Test hostname resolution
host $HOSTNAME

# Check host file entries
cat /etc/hosts | grep $HOSTNAME
```

**Solutions**:
- Use IP addresses instead of hostnames
- Add entries to `/etc/hosts` if needed
- Fix DNS configuration
- Use "0.0.0.0" to bind to all interfaces

### 4. IPv4/IPv6 Binding Conflicts

**Problem**: Server attempts to bind to IPv6 when IPv4 is intended or vice versa.

**Diagnosis**:
```bash
# Check if IPv6 is enabled
sysctl -a | grep ipv6

# Check interface IP versions
ip -br addr
```

**Solutions**:
- Specify the exact IP to bind to (e.g., "127.0.0.1" for IPv4 loopback)
- Disable IPv6 if not needed
- Configure dual-stack binding if both are required

## Environment Variable Configuration

The JSONdb server uses environment variables for socket configuration:

```bash
# In .env file or environment:
JSONDB_PORT=5000               # Server port
JSONDB_HOST=0.0.0.0            # Binding address (0.0.0.0 for all interfaces)
```

See the [Configuration README](../reference/CONFIG.md) for more details.

## Logs to Check for Socket Issues

When diagnosing socket issues, check these log messages:

```
[INIT:SOCKET] Creating socket on...
[INIT:SOCKET] Failed to create socket: ...
[INIT:SOCKET] Binding socket to ...
[INIT:SOCKET] Failed to bind socket: ...
[INIT:SOCKET] Socket listening successfully
```

## Advanced Debugging

For difficult socket issues, use these advanced techniques:

1. **Packet Sniffing**:
   ```bash
   # Install tcpdump if needed
   sudo tcpdump -i lo port 5000
   ```

2. **System Call Tracing**:
   ```bash
   # Trace system calls for the server
   strace -e trace=network ./build/jsondb_runtime.sh start
   ```

3. **Socket State Analysis**:
   ```bash
   # List all TCP sockets in various states
   ss -tan state all
   ```

## Recommendations

1. Always use the socket diagnostics tool before reporting socket-related issues
2. Use explicit IP addresses rather than hostnames for binding
3. Check system resource limits (file descriptors) when handling many connections
4. Prefer non-privileged ports (>1024) for security
5. Check for firewall rules that might block connections
6. Always verify the actual bound address and port in logs

When all else fails, contact system administration or create a detailed issue report with:
- Socket diagnostics tool output
- Server logs
- Network configuration
- Environment variable values