#!/bin/bash

# Apply socket binding patch and test the fix

set -e

echo "Applying socket binding patch..."
cd /opt/jsondb
patch -p1 < scripts/debug/socket_binding_fix.patch

echo "Rebuilding server..."
cd src
make clean
make

echo "Testing server with fixed socket binding..."
cd /opt/jsondb

# Kill any existing server processes
pkill -f "jsondb_server" || echo "No server processes running"

# Start server with test port
build/bin/jsondb_server -V -p 9999 -H 0.0.0.0 > server_test_patched.log 2>&1 &
SERVER_PID=$!

echo "Server started with PID $SERVER_PID"
sleep 5

# Check if socket is listening
echo "Checking socket status..."
netstat -tuln | grep 9999 || echo "Port 9999 not found in netstat"
ss -tuln | grep 9999 || echo "Port 9999 not found in ss"

# Test health endpoint
echo "Testing health endpoint..."
curl -s -I http://localhost:9999/health || echo "Health endpoint not accessible"

# Clean up
echo "Shutting down test server..."
kill $SERVER_PID

echo "Test complete"