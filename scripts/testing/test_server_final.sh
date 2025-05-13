#!/bin/bash
# Final server test script with race condition handling

set -e

echo "=== JSON Database Server Final Test ==="
echo "This script will test server startup with JS engine handling"

# First ensure no server processes are running
echo "Ensuring no server processes are running..."
pkill -9 -f jsondb_server 2>/dev/null || echo "No server processes to kill"
sleep 2

# Clean up any existing PID files
echo "Removing any stale PID files..."
rm -f /home/claude-3/project/var/run/jsondb_server.pid
sleep 1

# Run the server with status check first
echo -e "\n1. Checking server status before starting..."
./bin/jsondb_server -status || echo "No server running (expected)"

# Now start the server in daemon mode (with debug output)
echo -e "\n2. Starting server in daemon mode..."
./bin/jsondb_server -daemon -port 6000

# Give it a moment to start up
sleep 3

# Check status
echo -e "\n3. Checking server status after startup..."
./bin/jsondb_server -status

# Verify PID file exists
echo -e "\n4. Verifying PID file was created..."
PID_FILE="/home/claude-3/project/var/run/jsondb_server.pid"
if [ -f "$PID_FILE" ]; then
    PID=$(cat "$PID_FILE")
    echo "PID file exists with PID: $PID"

    # Validate process is running
    if ps -p $PID > /dev/null; then
        echo "✅ Success: Process with PID $PID is running"
    else
        echo "❌ Error: Process with PID $PID is not running"
    fi
else
    echo "❌ Error: PID file was not created"
fi

# Check log file
echo -e "\n5. Checking log file for successful initialization..."
LOG_FILE="/home/claude-3/project/var/log/jsondb/server.log"
if [ -f "$LOG_FILE" ]; then
    echo "Log file exists. Recent entries:"
    tail -n 10 "$LOG_FILE"
    
    # Check for JavaScript initialization
    if grep -q "JavaScript engine initialized" "$LOG_FILE"; then
        echo "✅ Success: JavaScript engine was initialized properly"
    else
        echo "❌ Warning: Could not find JavaScript engine initialization in log"
    fi
else
    echo "❌ Error: Log file not found!"
fi

# Stop the server
echo -e "\n6. Stopping server..."
./bin/jsondb_server -stop

# Verify server stopped
echo -e "\n7. Verifying server was stopped..."
if ./bin/jsondb_server -status | grep -q "No running server"; then
    echo "✅ Success: Server was stopped correctly"
else
    echo "❌ Error: Server is still running"
    ./bin/jsondb_server -status
fi

echo -e "\n=== Test completed ==="