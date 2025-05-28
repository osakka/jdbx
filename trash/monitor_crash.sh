#!/bin/bash
# Monitor JSONdb for crashes and capture debug info

echo "=== JSONdb Crash Monitor ==="
echo "Monitoring server (PID: $(cat /opt/jsondb/build/var/jsondb.pid 2>/dev/null || echo 'unknown'))"
echo ""
echo "Please reproduce the crash by:"
echo "1. Opening the web UI at http://localhost:5000"
echo "2. Logging in with admin credentials"
echo "3. Browsing different sections (Dashboard, Browser, Metrics, RBAC, etc.)"
echo "4. Clicking on collections and documents"
echo ""
echo "Monitoring logs for errors..."
echo "Press Ctrl+C to stop monitoring"
echo ""

# Function to check if server is still running
check_server() {
    if [ -f /opt/jsondb/build/var/jsondb.pid ]; then
        PID=$(cat /opt/jsondb/build/var/jsondb.pid)
        if ! kill -0 $PID 2>/dev/null; then
            return 1
        fi
    else
        return 1
    fi
    return 0
}

# Tail the log and watch for issues
tail -f /opt/jsondb/build/var/jsondb.log | while read line; do
    echo "$line"
    
    # Check for error patterns
    if echo "$line" | grep -E "(ERROR|FATAL|SEGV|signal|abort|crash|died|terminated)" > /dev/null; then
        echo ""
        echo "!!! POTENTIAL ISSUE DETECTED !!!"
        echo "$line"
        echo ""
    fi
    
    # Check if server is still running
    if ! check_server; then
        echo ""
        echo "=== SERVER HAS CRASHED ==="
        echo "Last log entries:"
        tail -20 /opt/jsondb/build/var/jsondb.log
        echo ""
        echo "Checking for core dump..."
        ls -la /tmp/core* 2>/dev/null || echo "No core dumps found"
        echo ""
        echo "Checking system logs..."
        dmesg | tail -10 | grep -i jsondb || echo "No kernel messages"
        break
    fi
done