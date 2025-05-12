#!/bin/bash

# Enable debug output
set -x

# Set ASAN options for better debugging
export ASAN_OPTIONS=halt_on_error=1:detect_leaks=0:abort_on_error=1:print_stacktrace=1

# Start the server with more debugging
../../bin/jsondb_server > server_test.log 2>&1 &
SERVER_PID=$!

# Wait for server to start
sleep 5

# Check if server is still running
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "Server failed to start or crashed. Check server_test.log"
    cat server_test.log
fi

# Test admin interface
echo "Testing admin interface..."
curl -s -o /dev/null -w "%{http_code}\n" http://localhost:5000/

# Test API endpoints
echo "Testing API endpoints..."
curl -s -X POST -H "Content-Type: application/json" -d '{"username":"admin","password":"admin"}' http://localhost:5000/api/auth/login

# Test metrics endpoint
echo "Testing metrics endpoint..."
curl -s http://localhost:5000/api/metrics

# Stop the server
kill $SERVER_PID