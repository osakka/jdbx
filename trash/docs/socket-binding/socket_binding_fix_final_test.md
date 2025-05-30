# Socket Binding Fix: Final Testing Instructions

This document provides instructions for testing the socket binding implementation in JSONdb server to verify that all fixes have been successfully applied.

## Prerequisites

1. A clean build of the JSONdb server with the latest socket binding fixes
2. Terminal access with superuser privileges
3. Basic networking tools (netstat, lsof, curl) 

## Basic Testing Flow

The testing flow follows these steps:

1. Build the server
2. Test daemon mode operation
3. Test foreground mode operation
4. Test custom host/port configurations
5. Verify error handling behavior

## Building The Server

```bash
cd /opt/jsondb/src
make clean
make
```

## Test 1: Daemon Mode

### Start server in daemon mode:

```bash
cd /opt/jsondb
build/jsondb_runtime.sh start
```

### Verify server process:

```bash
ps aux | grep jsondb_server
```

Expected result: Should show at least one jsondb_server process running.

### Verify socket binding:

```bash
netstat -tuln | grep 5000
```

Expected result:
```
tcp        0      0 0.0.0.0:5000            0.0.0.0:*               LISTEN
```

### Test API connectivity:

```bash
curl http://localhost:5000/health
```

Expected result: A JSON response indicating the server is healthy.

### Stop the server:

```bash
build/jsondb_runtime.sh stop
```

## Test 2: Foreground Mode

### Start server in foreground mode:

```bash
cd /opt/jsondb
build/jsondb_runtime.sh start --debug
```

The server should start in the foreground with logging output.

### In another terminal, verify socket binding:

```bash
netstat -tuln | grep 5000
```

### Test API connectivity:

```bash
curl http://localhost:5000/health
```

### Stop the server:

Press Ctrl+C in the terminal where the server is running.

## Test 3: Custom Host and Port

### Edit configuration:

Edit the configuration settings either via command line parameters or in the configuration file:

```bash
# Example using command-line parameters
build/jsondb_runtime.sh start --port=5001 --host=127.0.0.1
```

### Verify custom binding:

```bash
netstat -tuln | grep 5001
```

Expected result:
```
tcp        0      0 127.0.0.1:5001          0.0.0.0:*               LISTEN
```

### Test connectivity to custom host/port:

```bash
curl http://127.0.0.1:5001/health
```

### Stop the server:

```bash
build/jsondb_runtime.sh stop
```

## Test 4: Error Handling

### Test port conflict:

1. Start another service on port 5000, for example:
   ```bash
   python3 -m http.server 5000
   ```

2. Attempt to start JSONdb:
   ```bash
   build/jsondb_runtime.sh start
   ```

Expected result: JSONdb should detect the port conflict and exit with an error message.

3. Clean up:
   ```bash
   # Stop the Python web server
   kill $(lsof -t -i:5000)
   ```

## Test 5: Automated Testing

Use the provided debug script to automate basic tests:

```bash
cd /opt/jsondb
scripts/debug/test_socket_fix.sh
```

This script should:
1. Start the server
2. Verify the process is running
3. Check if the port is in use
4. Test basic HTTP connectivity
5. Report success or detailed errors

## Success Criteria

The socket binding fix is considered successful if:

1. The server starts and binds correctly in both daemon and foreground modes
2. Socket binding works with different host/port configurations
3. Port conflicts are detected and reported correctly
4. The server responds to API requests over the bound socket
5. The automated test script passes

## Troubleshooting

If issues occur during testing:

1. Check server logs:
   ```bash
   tail -f /opt/jsondb/var/jsondb_server.log
   ```

2. Examine socket states:
   ```bash
   ss -tlnp | grep jsondb
   ```

3. Check for port conflicts:
   ```bash
   lsof -i :5000
   ```
   
4. Verify process status:
   ```bash
   ps aux | grep jsondb
   ```

5. Review PID file:
   ```bash
   cat /opt/jsondb/var/jsondb_server.pid
   ```

## Complete Testing Sequence

For a thorough test, run the following commands in sequence:

```bash
# Build the server
cd /opt/jsondb/src
make clean
make

# Test daemon mode
cd /opt/jsondb
build/jsondb_runtime.sh start
netstat -tuln | grep 5000
curl http://localhost:5000/health
build/jsondb_runtime.sh stop

# Test foreground mode
build/jsondb_runtime.sh start --debug
# In another terminal:
curl http://localhost:5000/health
# Then press Ctrl+C to stop

# Test custom configuration
build/jsondb_runtime.sh start --port=5001 --host=127.0.0.1
netstat -tuln | grep 5001
curl http://127.0.0.1:5001/health
build/jsondb_runtime.sh stop

# Run automated test
scripts/debug/test_socket_fix.sh
```

If all these tests pass, the socket binding fix is considered fully successful.