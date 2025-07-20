#!/bin/bash

# Server monitoring script to detect crashes immediately

PIDFILE="/opt/jdbx/build/var/jdbxd.pid"
LOGFILE="/opt/jdbx/build/var/jdbxd.log"

echo "Starting server monitoring..."

# Function to check if server is running
check_server() {
    if [ -f "$PIDFILE" ]; then
        PID=$(cat "$PIDFILE")
        if ps -p "$PID" > /dev/null 2>&1; then
            echo "Server running: PID $PID"
            return 0
        else
            echo "CRASH DETECTED: Server not running but PID file exists"
            return 1
        fi
    else
        echo "No PID file found"
        return 1
    fi
}

# Function to check for segfaults in dmesg
check_segfaults() {
    # Check for recent segfaults (last 100 lines)
    SEGFAULTS=$(dmesg 2>/dev/null | tail -100 | grep -i "segfault.*jdbxd" | tail -5)
    if [ -n "$SEGFAULTS" ]; then
        echo "SEGFAULT DETECTED:"
        echo "$SEGFAULTS"
        return 1
    fi
    return 0
}

# Function to check for crashes in logs
check_log_errors() {
    if [ -f "$LOGFILE" ]; then
        # Check last 20 lines for critical errors
        ERRORS=$(tail -20 "$LOGFILE" | grep -E "(CRITICAL|FATAL|segfault|crash|abort)")
        if [ -n "$ERRORS" ]; then
            echo "LOG ERRORS DETECTED:"
            echo "$ERRORS"
            return 1
        fi
    fi
    return 0
}

# Monitor continuously
while true; do
    echo "=== Server Status Check $(date) ==="
    
    SERVER_OK=true
    
    if ! check_server; then
        SERVER_OK=false
    fi
    
    if ! check_segfaults; then
        SERVER_OK=false
    fi
    
    if ! check_log_errors; then
        SERVER_OK=false
    fi
    
    if [ "$SERVER_OK" = false ]; then
        echo "CRITICAL: Server issues detected!"
        echo "Checking memory usage..."
        ps aux | grep jdbxd | grep -v grep || echo "No server process found"
        
        echo "Recent log entries:"
        if [ -f "$LOGFILE" ]; then
            tail -10 "$LOGFILE"
        fi
        
        echo "=========================================="
        break
    else
        echo "Server status: OK"
    fi
    
    sleep 5
done