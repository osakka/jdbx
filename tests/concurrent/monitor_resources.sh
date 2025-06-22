#!/bin/bash

# Monitor server resources while running operations

PID=$(cat /opt/jdbx/build/var/jdbxd.pid 2>/dev/null)
if [ -z "$PID" ]; then
    echo "Server not running"
    exit 1
fi

echo "Monitoring server PID: $PID"
echo "Initial state:"
echo "Memory: $(ps -o vsz,rss -p $PID --no-headers)"
echo "Open files: $(ls /proc/$PID/fd 2>/dev/null | wc -l)"
echo "Threads: $(ls /proc/$PID/task 2>/dev/null | wc -l)"
echo ""

# Run 10 sequential operations one at a time
for i in {1..10}; do
    echo "Operation $i:"
    
    # Make a request
    curl -k -s -X POST https://localhost:5000/api/auth/login \
        -H "Content-Type: application/json" \
        -d '{"username":"admin","password":"admin"}' > /dev/null 2>&1
    
    # Check if server is still alive
    if ! kill -0 $PID 2>/dev/null; then
        echo "SERVER CRASHED after operation $i!"
        break
    fi
    
    # Show resources
    echo "  Memory: $(ps -o vsz,rss -p $PID --no-headers 2>/dev/null || echo 'N/A')"
    echo "  Open files: $(ls /proc/$PID/fd 2>/dev/null | wc -l || echo 'N/A')"
    echo "  Threads: $(ls /proc/$PID/task 2>/dev/null | wc -l || echo 'N/A')"
    
    sleep 0.5
done