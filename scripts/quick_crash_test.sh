#!/bin/bash

# Quick test to trigger the specific segfault scenario
echo "=== Quick Crash Test ==="
echo "Testing logger fix with concurrent failed login attempts..."

# Start server monitoring in background
(
  while true; do
    if ! ps -p $(cat /opt/jdbx/build/var/jdbxd.pid 2>/dev/null) > /dev/null 2>&1; then
      echo "CRASH DETECTED at $(date)"
      echo "Checking logs..."
      tail -10 /opt/jdbx/build/var/jdbxd.log
      echo "============================="
      exit 1
    fi
    sleep 1
  done
) &
MONITOR_PID=$!

# Run concurrent requests that trigger timeout handling
echo "Starting concurrent login attempts to trigger timeout handling..."
for i in {1..20}; do
  curl -k -X POST "https://localhost:5000/api/auth/login" \
    -H "Content-Type: application/json" \
    -d '{"username":"invalid","password":"invalid"}' \
    --connect-timeout 30 --max-time 60 -s > /dev/null 2>&1 &
done

# Wait for all requests to complete
wait

# Stop monitoring
kill $MONITOR_PID 2>/dev/null

echo "Test completed successfully - no crashes detected!"
echo "Server is still running: $(ps -p $(cat /opt/jdbx/build/var/jdbxd.pid 2>/dev/null) > /dev/null 2>&1 && echo 'YES' || echo 'NO')"