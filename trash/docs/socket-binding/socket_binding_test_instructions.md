# Socket Binding Test Instructions

This document provides instructions for testing the socket binding implementation in JSONdb server to verify that the fixes have been successfully applied.

## Testing Environment

Before testing, ensure you have the following:

1. A clean build of the JSONdb server with the latest socket binding fixes
2. Terminal access to run commands
3. Basic networking tools (netstat, curl) installed

## Building the Server

First, ensure you have a clean build with the latest changes:

```bash
cd /opt/jsondb/src
make clean
make
```

## Testing Steps

### 1. Daemon Mode Testing

#### Start the server in daemon mode:

```bash
cd /opt/jsondb
build/jsondb_runtime.sh start
```

You should see output indicating that the server has started successfully.

#### Check server process:

```bash
ps aux | grep jsondb
```

You should see at least one jsondb_server process running.

#### Verify socket binding:

```bash
netstat -tuln | grep 5000
```

You should see output similar to:
```
tcp        0      0 0.0.0.0:5000            0.0.0.0:*               LISTEN
```

This indicates the server is listening on port 5000 on all interfaces.

#### Test API connectivity:

```bash
curl http://localhost:5000/health
```

You should receive a JSON response indicating the server is healthy.

#### Stop the server:

```bash
build/jsondb_runtime.sh stop
```

### 2. Foreground Mode Testing

#### Start the server in foreground mode:

```bash
cd /opt/jsondb
build/jsondb_runtime.sh run
```

The server should start in foreground mode with verbose logging.

#### In a separate terminal, test API connectivity:

```bash
curl http://localhost:5000/health
```

You should receive a JSON response indicating the server is healthy.

#### Stop the server:

Press Ctrl+C in the terminal where the server is running.

### 3. Custom Host/Port Testing

#### Edit the configuration file:

```bash
cd /opt/jsondb
vi etc/jsondb/jsondb.conf
```

Change the host and port settings as desired.

#### Start the server:

```bash
build/jsondb_runtime.sh start
```

#### Verify socket binding with custom settings:

```bash
netstat -tuln | grep <custom-port>
```

#### Test API connectivity:

```bash
curl http://<custom-host>:<custom-port>/health
```

#### Stop the server:

```bash
build/jsondb_runtime.sh stop
```

## Verification Criteria

The socket binding fix is considered successful if:

1. The server starts successfully in both daemon and foreground modes
2. The server socket is properly bound and listening on the configured port
3. The server accepts and responds to API requests
4. No socket binding errors are reported in the logs

## Troubleshooting

If you encounter issues during testing:

1. Check server logs at `/opt/jsondb/var/log/jsondb.log`
2. Ensure no other process is using port 5000 (or your custom port)
3. Verify that the server has proper permissions to bind to ports

## Advanced Testing

For more advanced testing, you can use the minimal socket binding test program included in the source code:

```bash
cd /opt/jsondb/src
gcc -Wall -Wextra socket_bind_test.c -o socket_bind_test
./socket_bind_test 5000  # Test with port 5000
```

This program tests the essential socket binding functionality:
1. Creating a socket
2. Setting socket options (SO_REUSEADDR)
3. Binding to the specified port
4. Setting the socket to listen state
5. Verifying the socket is in listening state

You can also use the debug scripts in the `scripts/debug` directory:

```bash
cd /opt/jsondb
scripts/debug/test_socket_binding.sh
```

These scripts perform a series of comprehensive tests to verify the socket binding implementation.

## Reporting Issues

If you encounter persistent issues with socket binding, document the following:

1. Exact command used to start the server
2. Server configuration settings
3. Error messages from server logs
4. Output of `netstat -tuln | grep <port>`
5. Operating system and environment details