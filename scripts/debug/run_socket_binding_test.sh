#!/bin/bash

# Socket binding test script
# This script builds and runs the socket binding test programs
# to diagnose issues with server socket binding.

set -e

# Ensure we're in the scripts/debug directory
cd "$(dirname "$0")"

# ANSI color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Log function
log() {
    echo -e "${BLUE}[$(date '+%Y-%m-%d %H:%M:%S')] $*${NC}"
}

# Success log
success() {
    echo -e "${GREEN}[SUCCESS] $*${NC}"
}

# Error log
error() {
    echo -e "${RED}[ERROR] $*${NC}"
}

# Warning log
warning() {
    echo -e "${YELLOW}[WARNING] $*${NC}"
}

# Build test programs
build_tests() {
    log "Building test programs..."
    
    # Build simple socket test
    log "Building test_socket_binding..."
    gcc -Wall -o test_socket_binding test_socket_binding.c
    
    # Build comprehensive server socket test
    log "Building test_server_socket_binding..."
    gcc -Wall -o test_server_socket_binding test_server_socket_binding.c -lpthread
    
    success "All test programs built successfully"
}

# Run hostname resolution test
test_hostname_resolution() {
    log "Testing hostname resolution..."
    
    # Get system hostname
    HOSTNAME=$(hostname)
    log "System hostname: $HOSTNAME"
    
    # Try to resolve hostname
    log "Resolving hostname to IP..."
    HOST_IP=$(host "$HOSTNAME" | grep "has address" | awk '{print $4}')
    
    if [ -z "$HOST_IP" ]; then
        warning "Could not resolve hostname to IP. This may cause binding issues."
        warning "Check /etc/hosts file for proper hostname mapping."
        
        # Check /etc/hosts
        log "Current /etc/hosts file:"
        cat /etc/hosts
    else
        success "Hostname '$HOSTNAME' resolved to IP: $HOST_IP"
        
        # Check if IP is 127.0.0.1
        if [[ "$HOST_IP" == "127.0.0.1" || "$HOST_IP" == "127."* ]]; then
            warning "Hostname resolves to loopback address. This may limit external connections."
        fi
    fi
}

# Run basic socket binding test
run_basic_test() {
    log "Running basic socket binding test..."
    
    # Test with port 5001
    log "Testing port 5001 (press Enter when prompted to continue)..."
    echo -ne "\n" | ./test_socket_binding 5001
    
    # Check result
    if [ $? -eq 0 ]; then
        success "Basic socket binding test passed"
    else
        error "Basic socket binding test failed"
    fi
}

# Run comprehensive server socket test
run_server_test() {
    log "Running comprehensive server socket test..."
    
    # Test with different addresses
    for addr in "0.0.0.0" "127.0.0.1" "$(hostname)"; do
        log "Testing binding to $addr:6789 (will run for 5 seconds)..."
        timeout 5 ./test_server_socket_binding "$addr" 6789 &
        PID=$!
        
        # Wait a moment
        sleep 2
        
        # Check if test is still running
        if kill -0 $PID 2>/dev/null; then
            log "Test on $addr:6789 appears to be running correctly"
            
            # Check if port is actually listening
            if netstat -tuln | grep -q ":6789"; then
                success "Socket is properly bound and listening on $addr:6789"
            else
                error "Socket is not visible in netstat output"
            fi
            
            # Terminate test
            kill $PID
        else
            error "Test on $addr:6789 failed or terminated prematurely"
        fi
        
        # Allow time for port to be released
        sleep 1
    done
}

# Run socket binding tests to diagnose issues
run_all_tests() {
    log "Starting socket binding diagnostic tests..."
    
    # Test hostname resolution
    test_hostname_resolution
    
    # Run socket binding tests
    run_basic_test
    
    # Run server socket tests
    run_server_test
    
    log "All tests completed"
}

# Main function
main() {
    echo "============================"
    echo "Socket Binding Diagnostic Tool"
    echo "============================"
    
    # Build tests
    build_tests
    
    # Run tests
    run_all_tests
    
    echo "============================"
    echo "Diagnostics Complete"
    echo "============================"
}

# Execute main function
main