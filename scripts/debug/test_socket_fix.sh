#!/bin/bash

echo "Testing JSONdb socket binding..."

# Clean up any existing processes
pkill -f jsondb_server || true
sleep 1

# Start the server in the background with debug flag
cd /opt/jsondb
./build/jsondb_runtime.sh stop 2>/dev/null
sleep 1
./build/jsondb_runtime.sh start --debug > server_log.txt 2>&1 &
SCRIPT_PID=$!

# Wait for the server to start up
echo "Waiting for server startup (up to 10 seconds)..."
for i in {1..10}; do
  echo "Checking server status (attempt $i)..."
  sleep 1
  
  # Find the actual server PID
  SERVER_PID=$(ps aux | grep jsondb_server | grep -v grep | awk '{print $2}')
  
  if [ -n "$SERVER_PID" ]; then
    echo "Server process found with PID: $SERVER_PID"
    break
  fi
done

# Check if the server is running
if [ -z "$SERVER_PID" ]; then
  echo "ERROR: Server failed to start! Check server_log.txt for details."
  cat server_log.txt
  exit 1
fi

# Test if the port is in use (wait up to 5 seconds)
echo "Checking if port 5000 is in use..."
for i in {1..5}; do
  echo "Port check attempt $i..."
  PORT_USED=$(netstat -tuln < /dev/null | grep ":5000 " | wc -l)
  if [ $PORT_USED -gt 0 ]; then
    echo "SUCCESS: Port 5000 is in use!"
    break
  fi
  sleep 1
done

if [ $PORT_USED -eq 0 ]; then
  echo "ERROR: Port 5000 is not in use after 5 seconds! Server may not be binding correctly."
  cat server_log.txt
  kill -9 $SERVER_PID 2>/dev/null || true
  exit 1
fi

# Test basic HTTP connectivity
echo "Testing HTTP connectivity..."
HTTP_RESPONSE=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:5000/health || echo "FAILED")

if [ "$HTTP_RESPONSE" = "FAILED" ] || [ "$HTTP_RESPONSE" != "200" ]; then
  echo "ERROR: HTTP connectivity test failed with response: $HTTP_RESPONSE"
  cat server_log.txt
  kill -9 $SERVER_PID 2>/dev/null || true
  exit 1
fi

# If we got here, all tests passed
echo "SUCCESS: Server started and is accepting connections!"
echo "Server log content:"
echo "===================="
cat server_log.txt
echo "===================="

# Clean up
kill -9 $SERVER_PID 2>/dev/null || true
echo "Test completed successfully."
