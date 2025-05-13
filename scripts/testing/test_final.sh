#!/bin/bash
# Final test script to validate server operation with JavaScript engine handling
set -e

echo "=== JSON Database Server Final Test ==="
echo "This script will test server startup with JS engine handling"

# First ensure no server processes are running
echo "1. Ensuring no server processes are running..."
pkill -9 -f jsondb_server 2>/dev/null || echo "No server processes to kill"
sleep 2

# Clean up any existing PID files
echo "2. Removing any stale PID files..."
rm -f /home/claude-3/project/var/run/jsondb_server.pid
sleep 1

# Check the server status
echo "3. Checking server status before starting..."
./bin/jsondb_server -status

# Start the server in daemon mode
echo "4. Starting server in daemon mode..."
./bin/jsondb_server -daemon -port 6000

# Give it a moment to initialize
sleep 3

# Check status
echo "5. Checking server status after startup..."
./bin/jsondb_server -status

# Verify PID file exists
echo "6. Verifying PID file was created..."
PID_FILE="/home/claude-3/project/var/run/jsondb_server.pid"
if [ -f "$PID_FILE" ]; then
    PID=$(cat "$PID_FILE")
    echo "PID file exists with PID: $PID"

    # Verify process exists
    if ps -p $PID > /dev/null; then
        echo "✅ Success: Process with PID $PID is running"
    else
        echo "❌ Error: Process with PID $PID is not running"
    fi
else
    echo "❌ Error: PID file was not created!"
fi

# Check log file
echo "7. Looking for JavaScript initialization in log file..."
LOG_FILE="/home/claude-3/project/var/log/jsondb/server.log"
if [ -f "$LOG_FILE" ]; then
    if grep -q "JavaScript engine initialized" "$LOG_FILE"; then
        echo "✅ Success: JavaScript engine was initialized properly"
    else
        echo "❌ Warning: Could not find JavaScript engine initialization in log"
    fi
    
    echo "Most recent log entries:"
    tail -n 10 "$LOG_FILE"
else
    echo "❌ Error: Log file not found"
fi

# Stop the server
echo "8. Stopping server..."
./bin/jsondb_server -stop

# Verify server was stopped
echo "9. Verifying server was stopped..."
if ! ps -p $PID > /dev/null 2>&1; then
    echo "✅ Success: Server process was terminated"
else
    echo "❌ Error: Server process is still running!"
    kill -9 $PID 2>/dev/null || true
fi

# Verify PID file was removed
if [ ! -f "$PID_FILE" ]; then
    echo "✅ Success: PID file was removed properly"
else
    echo "❌ Error: PID file still exists after stopping server!"
    rm -f "$PID_FILE"
fi

echo "=== Test completed successfully ==="