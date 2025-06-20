#!/bin/bash
#
# Daemon Mode Socket Fix Test for JSONDB
# This script specifically tests the fix for the race condition in daemon mode
# where socket binding happens after logging is initialized.
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

# Function to check logs for specific patterns
check_logs() {
    local log_file=$1
    local pattern=$2
    
    if [ ! -f "$log_file" ]; then
        log_error "Log file $log_file does not exist"
        return 1
    fi
    
    if grep -q "$pattern" "$log_file"; then
        log_info "Found pattern '$pattern' in log file"
        return 0
    else
        log_error "Pattern '$pattern' not found in log file"
        return 1
    fi
}

# Clean up logs and PIDs
cleanup() {
    log_info "Cleaning up logs and PID files..."
    rm -f /tmp/jsondb_daemon_test_*.log /tmp/jsondb_daemon_test_*.pid
}

# Main test function
test_daemon_socket_fix() {
    local port=5050
    local log_file="/tmp/jsondb_daemon_test_$port.log"
    local pid_file="/tmp/jsondb_daemon_test_$port.pid"
    
    log_info "Testing daemon mode socket fix on port $port..."
    
    # Kill any existing instances
    kill_server
    
    # Clean up any existing files
    cleanup
    
    # Start server in daemon mode
    log_info "Starting server in daemon mode on port $port..."
    cd /opt/jsondb/build
    ./bin/jsondb_server -p $port --pid-file $pid_file --log-file $log_file
    
    # Give server time to start
    sleep 3
    
    # Check if process is running
    if [ -f "$pid_file" ]; then
        local pid=$(cat "$pid_file")
        log_info "Server started with PID $pid"
        
        # Check if process exists
        if kill -0 $pid 2>/dev/null; then
            log_info "Process is running"
        else
            log_error "Process is not running despite PID file existing"
            cat "$log_file"
            return 1
        fi
    else
        log_error "PID file was not created, server failed to start"
        cat "$log_file"
        return 1
    fi
    
    # Check socket binding
    if check_port $port; then
        log_info "Socket bound successfully to port $port"
        netstat -tuln | grep ":$port "
    else
        log_error "Socket binding failed on port $port"
        netstat -tuln
        cat "$log_file"
        return 1
    fi
    
    # Check logs for successful starting server message
    check_logs "$log_file" "Starting server on.*$port.*in daemon mode"
    local start_msg_status=$?
    
    # Check logs for successful binding message
    check_logs "$log_file" "Successfully bound to port $port"
    local bind_msg_status=$?
    
    # Check logs for successful running message
    check_logs "$log_file" "Server running in daemon mode"
    local run_msg_status=$?
    
    # Kill the server
    if [ -f "$pid_file" ]; then
        local pid=$(cat "$pid_file")
        log_info "Terminating server with PID $pid..."
        kill $pid
        sleep 1
    fi
    
    # Return combined status
    if [ $start_msg_status -eq 0 ] && [ $bind_msg_status -eq 0 ] && [ $run_msg_status -eq 0 ]; then
        log_info "Daemon mode socket fix test PASSED"
        return 0
    else
        log_error "Daemon mode socket fix test FAILED"
        return 1
    fi
}

# Main execution
log_info "=== JSONDB Daemon Mode Socket Fix Test ==="

# Run the test
test_daemon_socket_fix
test_result=$?

# Clean up
cleanup

if [ $test_result -eq 0 ]; then
    log_info "All tests PASSED"
    exit 0
else
    log_error "Tests FAILED"
    exit 1
fi