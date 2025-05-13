#!/bin/bash
# Script to test server control functionality

set -e

echo "=== JSON Database Server Control Test ==="
echo "This script will test starting, checking status, and stopping the server"

# Make sure all server processes are stopped first
echo "Ensuring no server processes are running..."
killall -9 jsondb_server 2>/dev/null || echo "No jsondb_server processes running"
sleep 1

# Start the server
echo -e "\n1. Starting server in daemon mode..."
./bin/jsondb_server -daemon

# Check status
echo -e "\n2. Checking server status..."
./bin/jsondb_server -status

# Verify PID file exists
echo -e "\n3. Verifying PID file was created..."
if [ -f "./var/run/jsondb_server.pid" ]; then
    PID=$(cat ./var/run/jsondb_server.pid)
    echo "PID file exists with value: $PID"
    
    # Check if process with this PID exists
    if ps -p $PID > /dev/null; then
        echo "Process with PID $PID is running"
    else
        echo "ERROR: Process with PID $PID is NOT running!"
    fi
else
    echo "ERROR: PID file was not created!"
fi

# Check log file
echo -e "\n4. Checking log file..."
if [ -f "./var/log/jsondb/server.log" ]; then
    echo "Log file exists. Last 5 lines:"
    tail -n 5 ./var/log/jsondb/server.log
else
    echo "ERROR: Log file was not created!"
fi

# Verify database file
echo -e "\n5. Verifying database file..."
if [ -f "./var/data/jsondb/db.json" ]; then
    echo "Database file exists"
else
    echo "ERROR: Database file was not created!"
fi

# Stop the server
echo -e "\n6. Stopping server..."
./bin/jsondb_server -stop

# Verify server stopped
echo -e "\n7. Verifying server stopped..."
if pgrep -f jsondb_server > /dev/null; then
    echo "ERROR: Server process is still running!"
else
    echo "Server process is no longer running"
fi

# Verify PID file removed
echo -e "\n8. Verifying PID file was removed..."
if [ -f "./var/run/jsondb_server.pid" ]; then
    echo "ERROR: PID file still exists!"
else
    echo "PID file was properly removed"
fi

echo -e "\n=== Test completed ==="