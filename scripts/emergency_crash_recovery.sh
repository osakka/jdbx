#!/bin/bash

# Emergency crash recovery script for JDBX server
# This script monitors the server and automatically restarts on crashes

JDBX_PID_FILE="/opt/jdbx/build/var/jdbxd.pid"
CRASH_LOG="/opt/jdbx/build/var/crash_recovery.log"
MAX_CRASHES=5
CRASH_WINDOW=300  # 5 minutes

echo "Starting JDBX crash recovery monitor..."
echo "$(date): Crash recovery started" > "$CRASH_LOG"

# Function to check if server is running
is_server_running() {
    if [ -f "$JDBX_PID_FILE" ]; then
        local pid=$(cat "$JDBX_PID_FILE")
        if kill -0 "$pid" 2>/dev/null; then
            return 0
        fi
    fi
    return 1
}

# Function to restart server
restart_server() {
    local crash_count=$1
    echo "$(date): SERVER CRASH #$crash_count DETECTED - Restarting..." | tee -a "$CRASH_LOG"
    
    # Stop any remaining processes
    cd /opt/jdbx
    ./build/jdbx_runtime.sh stop
    sleep 3
    
    # Clean up any stale files
    rm -f "$JDBX_PID_FILE"
    
    # Check for core dumps
    if ls core.* 2>/dev/null; then
        echo "$(date): Core dump found - moving to crash_logs/" | tee -a "$CRASH_LOG"
        mkdir -p /opt/jdbx/crash_logs
        mv core.* /opt/jdbx/crash_logs/
    fi
    
    # Set memory limits
    ulimit -v 1048576  # 1GB virtual memory limit
    ulimit -m 524288   # 512MB physical memory limit
    ulimit -c unlimited # Enable core dumps
    
    # Start server with reduced configuration
    echo "$(date): Starting server with emergency configuration..." | tee -a "$CRASH_LOG"
    
    # Temporarily reduce thread pool size
    export JDBX_THREAD_POOL_MAX=4
    export JDBX_MAX_CONNECTIONS=50
    
    ./build/jdbx_runtime.sh start
    
    # Wait for startup
    sleep 10
    
    if is_server_running; then
        echo "$(date): Server restarted successfully" | tee -a "$CRASH_LOG"
        return 0
    else
        echo "$(date): Server restart failed" | tee -a "$CRASH_LOG"
        return 1
    fi
}

# Main monitoring loop
crash_count=0
last_crash_time=0

while true; do
    if is_server_running; then
        echo "$(date): Server is running (PID: $(cat $JDBX_PID_FILE))"
        sleep 30
        continue
    fi
    
    # Server is not running - check if it's a crash
    current_time=$(date +%s)
    
    # Reset crash count if enough time has passed
    if [ $((current_time - last_crash_time)) -gt $CRASH_WINDOW ]; then
        crash_count=0
    fi
    
    crash_count=$((crash_count + 1))
    last_crash_time=$current_time
    
    if [ $crash_count -gt $MAX_CRASHES ]; then
        echo "$(date): CRITICAL: Too many crashes ($crash_count) in $CRASH_WINDOW seconds" | tee -a "$CRASH_LOG"
        echo "$(date): Stopping crash recovery to prevent infinite loop" | tee -a "$CRASH_LOG"
        break
    fi
    
    # Attempt restart
    if restart_server $crash_count; then
        echo "$(date): Recovery successful" | tee -a "$CRASH_LOG"
    else
        echo "$(date): Recovery failed - waiting before retry" | tee -a "$CRASH_LOG"
        sleep 60
    fi
done

echo "$(date): Crash recovery monitor stopped" | tee -a "$CRASH_LOG"