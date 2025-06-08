#!/bin/bash

echo "Testing single keep-alive connection..."

# Check initial state
initial=$(curl -s http://localhost:5000/api/health | jq -r '.metrics.performance.active_connections')
echo "Initial active connections: $initial"

# Make 5 requests on a single keep-alive connection
echo -e "\nMaking 5 requests on single keep-alive connection..."
(
    exec 3<>/dev/tcp/localhost/5000
    
    # Request 1
    echo -e "GET /api/health HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n" >&3
    timeout 1 cat <&3 >/dev/null
    
    # Request 2
    echo -e "GET /api/health HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n" >&3
    timeout 1 cat <&3 >/dev/null
    
    # Request 3
    echo -e "GET /api/health HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n" >&3
    timeout 1 cat <&3 >/dev/null
    
    # Request 4
    echo -e "GET /api/health HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n" >&3
    timeout 1 cat <&3 >/dev/null
    
    # Request 5 - close connection
    echo -e "GET /api/health HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n" >&3
    timeout 1 cat <&3 >/dev/null
    
    exec 3>&-
)

echo -e "\nMonitoring after requests..."
for i in {1..10}; do
    active=$(curl -s http://localhost:5000/api/health | jq -r '.metrics.performance.active_connections')
    echo "[$i] Active connections: $active"
    sleep 0.5
done

echo -e "\nTest complete."