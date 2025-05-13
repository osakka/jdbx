#!/bin/bash
# Direct server control test with absolute paths

set -e

# Store absolute paths
PROJECT_DIR="/home/claude-3/project"
BINARY="$PROJECT_DIR/bin/jsondb_server"
PID_FILE="$PROJECT_DIR/var/run/jsondb_server.pid"
LOG_FILE="$PROJECT_DIR/var/log/jsondb/server.log"
DB_FILE="$PROJECT_DIR/var/data/jsondb/db.json"

echo "=== JSON Database Server Direct Control Test ==="
echo "Using absolute paths for all operations"

# Kill any lingering processes
echo "Ensuring no server processes are running..."
ps aux | grep jsondb_server | grep -v grep | awk '{print $2}' | xargs kill -9 2>/dev/null || true
sleep 2

# Remove existing PID file if it exists
if [ -f "$PID_FILE" ]; then
    echo "Removing existing PID file..."
    rm -f "$PID_FILE"
fi

# Start the server with absolute paths
echo -e "\n1. Starting server in foreground mode (non-daemon)..."
$BINARY -pid "$PID_FILE" -log "$LOG_FILE" -port 5050 &
SERVER_PID=$!
echo "Started server with PID: $SERVER_PID"
sleep 3

# Check if the process is running
echo -e "\n2. Checking if process is running..."
if ps -p $SERVER_PID > /dev/null; then
    echo "Server process is running with PID $SERVER_PID"
else
    echo "ERROR: Server process is not running!"
    exit 1
fi

# Check if PID file was created
echo -e "\n3. Checking PID file..."
if [ -f "$PID_FILE" ]; then
    SAVED_PID=$(cat "$PID_FILE")
    echo "PID file contains: $SAVED_PID (should match $SERVER_PID)"
else
    echo "ERROR: PID file was not created!"
fi

# Stop the server directly
echo -e "\n4. Stopping server directly with PID..."
kill $SERVER_PID
sleep 2

# Check if server stopped
echo -e "\n5. Verifying server stopped..."
if ps -p $SERVER_PID > /dev/null; then
    echo "ERROR: Server process is still running!"
    kill -9 $SERVER_PID
else
    echo "Server process successfully stopped"
fi

echo -e "\n=== Test completed ==="