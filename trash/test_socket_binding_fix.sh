#!/bin/bash
#
# Socket Binding Test Script for JSONDB
# This script tests the server's ability to bind to ports and accept connections
# with different modes and configurations.
#

# Set colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Utility functions
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check if a port is in use
check_port() {
    local port=$1
    netstat -tuln | grep -q ":$port " || ss -tuln | grep -q ":$port "
    return $?
}

# Function to kill server
kill_server() {
    log_info "Terminating any running server instances..."
    pkill -f jsondb_server || true
    sleep 1
}

# Function to build server
build_server() {
    log_info "Building server..."
    cd /opt/jsondb/src
    make clean && make
    if [ $? -ne 0 ]; then
        log_error "Failed to build server"
        exit 1
    fi
    log_info "Server built successfully"
}

# Function to verify socket binding
verify_socket_binding() {
    local port=$1
    local wait_time=${2:-3}
    
    log_info "Waiting for socket binding on port $port..."
    sleep $wait_time
    
    if check_port $port; then
        log_info "Socket bound successfully to port $port"
        return 0
    else
        log_error "Socket binding failed on port $port"
        return 1
    fi
}

# Function to test connection
test_connection() {
    local port=$1
    local attempts=${2:-3}
    local wait_time=${3:-1}
    
    log_info "Testing connection to localhost:$port (attempts: $attempts)..."
    
    for i in $(seq 1 $attempts); do
        sleep $wait_time
        curl -s -m 2 -o /dev/null -w "%{http_code}" "http://localhost:$port" > /dev/null 2>&1
        if [ $? -eq 0 ]; then
            log_info "Connection successful to port $port on attempt $i"
            return 0
        else
            if [ $i -eq $attempts ]; then
                log_error "All $attempts connection attempts failed"
            else
                log_warning "Connection attempt $i failed, retrying..."
            fi
        fi
    done
    
    return 1
}

# Function to test socket binding with simplified program
test_socket_binding() {
    local port=$1
    
    log_info "Testing socket binding with test program on port $port..."
    cd /opt/jsondb/src
    
    # Kill any existing process
    pkill -f socket_test || true
    
    # Compile and run the test program
    gcc socket_test.c -o socket_test
    
    # Run in background and save PID
    ./socket_test $port &
    local pid=$!
    
    # Wait for socket binding
    sleep 2
    
    # Check if port is bound
    if check_port $port; then
        log_info "Test program successfully bound to port $port (PID: $pid)"
        kill $pid
        return 0
    else
        log_error "Test program failed to bind to port $port"
        kill $pid
        return 1
    fi
}

# Function to test foreground mode
test_foreground_mode() {
    local port=$1
    
    log_info "Testing server in foreground mode on port $port..."
    cd /opt/jsondb/build
    
    # Start server in foreground mode with output redirected
    ./bin/jsondb_server -p $port --foreground > server_fg.log 2>&1 &
    local pid=$!
    
    # Verify socket binding
    verify_socket_binding $port 3
    local bind_status=$?
    
    # Test connection
    if [ $bind_status -eq 0 ]; then
        test_connection $port 3 1
        local conn_status=$?
    else
        local conn_status=1
    fi
    
    # Kill the server
    kill $pid
    
    # Return combined status
    if [ $bind_status -eq 0 ] && [ $conn_status -eq 0 ]; then
        log_info "Foreground mode test succeeded"
        return 0
    else
        log_error "Foreground mode test failed"
        return 1
    fi
}

# Function to test daemon mode
test_daemon_mode() {
    local port=$1
    
    log_info "Testing server in daemon mode on port $port..."
    cd /opt/jsondb/build
    
    # Start server in daemon mode
    ./bin/jsondb_server -p $port
    
    # Verify socket binding
    verify_socket_binding $port 3
    local bind_status=$?
    
    # Test connection
    if [ $bind_status -eq 0 ]; then
        test_connection $port 3 1
        local conn_status=$?
    else
        local conn_status=1
    fi
    
    # Kill the server
    kill_server
    
    # Return combined status
    if [ $bind_status -eq 0 ] && [ $conn_status -eq 0 ]; then
        log_info "Daemon mode test succeeded"
        return 0
    else
        log_error "Daemon mode test failed"
        return 1
    fi
}

# Function to test port fallback
test_port_fallback() {
    local primary_port=$1
    local fallback_port=$(($primary_port + 1))
    
    log_info "Testing port fallback from $primary_port to $fallback_port..."
    
    # Start a dummy server on the primary port
    nc -l $primary_port > /dev/null 2>&1 &
    local dummy_pid=$!
    
    # Wait for dummy server to start
    sleep 1
    
    # Start the actual server, which should fall back to fallback_port
    cd /opt/jsondb/build
    ./bin/jsondb_server -p $primary_port > fallback.log 2>&1 &
    local server_pid=$!
    
    # Verify binding on fallback port
    verify_socket_binding $fallback_port 3
    local bind_status=$?
    
    # Test connection
    if [ $bind_status -eq 0 ]; then
        test_connection $fallback_port 3 1
        local conn_status=$?
    else
        local conn_status=1
    fi
    
    # Kill both servers
    kill $dummy_pid $server_pid
    
    # Return combined status
    if [ $bind_status -eq 0 ] && [ $conn_status -eq 0 ]; then
        log_info "Port fallback test succeeded"
        return 0
    else
        log_error "Port fallback test failed"
        return 1
    fi
}

# Main test sequence
main() {
    log_info "=== JSONDB Socket Binding Test ==="
    
    # Clean up any existing server instances
    kill_server
    
    # Build server
    build_server
    
    # Test socket binding with test program
    test_socket_binding 5000
    test_socket_binding 8080
    
    # Test server in foreground mode
    test_foreground_mode 5000
    
    # Test server in daemon mode
    test_daemon_mode 5001
    
    # Test port fallback
    test_port_fallback 5002
    
    log_info "=== Socket Binding Tests Completed ==="
}

# Run main function
main