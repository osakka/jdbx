#!/bin/bash
# Script to isolate the server process detection issue

echo "=== Isolating JSON Database Server Issue ==="

# Kill any server processes definitively
echo "1. Killing any server processes..."
pkill -9 -f jsondb_server 2>/dev/null || true
sleep 2

# Check for any processes
echo "2. Checking for any jsondb_server processes..."
ps aux | grep jsondb_server | grep -v grep

# Clean environment
echo "3. Removing any PID files and log files..."
rm -f /home/claude-3/project/var/run/jsondb_server.pid
rm -f /home/claude-3/project/var/log/jsondb/server.log

# Hard-code the PID path for direct operations
PID_FILE="/home/claude-3/project/var/run/jsondb_server.pid"
LOG_FILE="/home/claude-3/project/var/log/jsondb/server.log"

# Check the code that detects running processes directly
echo "4. Running process detection directly..."
pgrep -f jsondb_server || echo "No jsondb_server processes found (good)"

# Try setting up dummy PID file to see if it's detected properly
echo "5. Setting up dummy PID file..."
mkdir -p $(dirname "$PID_FILE")
echo "12345" > "$PID_FILE"

# See if our process detection picks it up
echo "6. Checking if server status detects our dummy PID..."
./bin/jsondb_server -status

# Clean up
echo "7. Cleaning up..."
rm -f "$PID_FILE"

echo "=== Isolation test completed ==="