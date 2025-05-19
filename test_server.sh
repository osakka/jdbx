#!/bin/bash
# Test script for JSONdb server startup and TTY handling

# Ensure we're in the JSONdb directory
cd /opt/jsondb || exit 1

echo "Building JSONdb server..."
(cd src && make clean && make)

echo "Setting up environment..."
mkdir -p /tmp/jsondb_test/var/log
mkdir -p /tmp/jsondb_test/var/db
mkdir -p /tmp/jsondb_test/var/run

echo "Testing server in verbose mode (foreground)..."
echo "----------------------------------------------"
build/bin/jsondb_server -V -p 18080 \
  -o /tmp/jsondb_test/var/log/jsondb.log \
  -b /tmp/jsondb_test/var/db/db.json \
  -i /tmp/jsondb_test/var/run/jsondb.pid &

SERVER_PID=$!
echo "Server started with PID: $SERVER_PID"

# Wait for server to initialize
sleep 5

# Check if the server is responding
echo "Testing server connectivity..."
if curl -s http://localhost:18080/api/health 2>/dev/null | grep -q "status"; then
  echo "Server is responding to requests!"
else
  echo "Server is not responding to requests on port 18080"
fi

# Check server process
if ps -p $SERVER_PID >/dev/null; then
  echo "Server process is still running"
else
  echo "Server process has exited unexpectedly"
fi

# Terminate the server
echo "Terminating server process..."
kill $SERVER_PID
sleep 2

echo ""
echo "Testing server in daemon mode..."
echo "-------------------------------"
build/bin/jsondb_server -p 18081 \
  -o /tmp/jsondb_test/var/log/jsondb_daemon.log \
  -b /tmp/jsondb_test/var/db/db_daemon.json \
  -i /tmp/jsondb_test/var/run/jsondb_daemon.pid

# Wait for server to initialize
sleep 5

# Read PID file
DAEMON_PID=$(cat /tmp/jsondb_test/var/run/jsondb_daemon.pid 2>/dev/null)
echo "Daemon PID from file: $DAEMON_PID"

# Check if the daemon is running
if [ -n "$DAEMON_PID" ] && ps -p $DAEMON_PID >/dev/null; then
  echo "Daemon process is running"
else
  echo "Daemon process is not running or PID file not found"
fi

# Check if the daemon is responding
echo "Testing daemon connectivity..."
if curl -s http://localhost:18081/api/health 2>/dev/null | grep -q "status"; then
  echo "Daemon is responding to requests!"
else
  echo "Daemon is not responding to requests on port 18081"
fi

# Check the daemon log
echo "Daemon log excerpts:"
if [ -f /tmp/jsondb_test/var/log/jsondb_daemon.log ]; then
  tail -n 20 /tmp/jsondb_test/var/log/jsondb_daemon.log
else
  echo "Daemon log file not found"
fi

# Terminate the daemon if running
if [ -n "$DAEMON_PID" ] && ps -p $DAEMON_PID >/dev/null; then
  echo "Terminating daemon process..."
  kill $DAEMON_PID
fi

echo "Test complete"