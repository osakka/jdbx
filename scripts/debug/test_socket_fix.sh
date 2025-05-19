#\!/bin/bash

echo "Testing JSONdb socket binding with shorter timeout..."

# Clean up any existing processes
pkill -f jsondb_server || true
sleep 1

# Start the server in the background
cd /opt/jsondb && ./build/jsondb_runtime.sh --foreground > server_log.txt 2>&1 &
SERVER_PID=$\!

# Wait a short time for the server to start up (shorter timeout)
echo "Waiting 1 second for server startup..."
sleep 1

# Test if the server is running
if \! ps -p $SERVER_PID > /dev/null; then
  echo "ERROR: Server failed to start\! Check server_log.txt for details."
  cat server_log.txt
  exit 1
fi

# Test if the port is in use
PORT_USED=$(netstat -tuln  < /dev/null |  grep ":5000 " | wc -l)
if [ $PORT_USED -eq 0 ]; then
  echo "ERROR: Port 5000 is not in use! Server may not be binding correctly."
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
