#!/bin/bash
# Crash detector for JSONdb

PID=$(cat /opt/jsondb/build/var/jsondb.pid 2>/dev/null)
echo "Monitoring JSONdb server (PID: $PID)"
echo "Checking every 2 seconds..."
echo ""

# Function to get the last N lines before a pattern
get_context_before_crash() {
    local logfile=$1
    local lines=$2
    # Get the byte position of the last line
    local last_pos=$(stat -c %s "$logfile")
    tail -n $lines "$logfile"
}

while true; do
    if [ -z "$PID" ] || ! kill -0 $PID 2>/dev/null; then
        echo ""
        echo "=== SERVER CRASHED at $(date) ==="
        echo ""
        echo "Last 50 log entries:"
        echo "===================="
        get_context_before_crash /opt/jsondb/build/var/jsondb.log 50
        echo ""
        echo "Checking for patterns in the last 200 lines:"
        echo "==========================================="
        tail -200 /opt/jsondb/build/var/jsondb.log | grep -E "(handle_client|Request:|api_dispatch|query_documents|serialize|persistence|save)" | tail -30
        echo ""
        echo "Memory info at crash:"
        echo "===================="
        free -h
        echo ""
        echo "Disk usage:"
        echo "==========="
        df -h /opt/jsondb
        break
    fi
    
    # Show a heartbeat
    printf "."
    sleep 2
done