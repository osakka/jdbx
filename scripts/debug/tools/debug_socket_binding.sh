#!/bin/bash

# Colors for output
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

# Main directory paths
DEBUG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_ROOT="$(cd "$DEBUG_DIR/.." && pwd)"
LOG_DIR="$DEBUG_DIR/logs"

# Create logs directory if not exists
mkdir -p "$LOG_DIR"

# Log file
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
LOG_FILE="$LOG_DIR/socket_debug_${TIMESTAMP}.log"

# Redirect stdout and stderr to both console and log file
exec > >(tee -a "$LOG_FILE")
exec 2>&1

# Header
log_info "=== JSONdb Socket Binding Debug Tool ==="
log_info "Started at: $(date)"
log_info "Log file: $LOG_FILE"

# System check
log_info "Checking system environment"
uname -a
echo

# Check for active processes using ports
log_info "Checking for active processes using port 5000"
netstat -tuln | grep :5000 || echo "No processes found using port 5000"
lsof -i :5000 2>/dev/null || echo "No processes found using port 5000 (lsof)"
echo

# Kill any existing jsondb_server instances
log_info "Stopping any running jsondb_server instances"
pkill -f jsondb_server 2>/dev/null || echo "No server instances found"
sleep 1

# Run standalone socket test
log_info "Running standalone socket test"
"$(dirname "${BASH_SOURCE[0]}")/test_port_binding" 5000 &
TEST_PID=$!
sleep 3

# Check if the process is still running
if ps -p $TEST_PID > /dev/null; then
    log_info "Socket test is running successfully"
    # Check netstat again
    log_info "Checking netstat with socket test running"
    netstat -tuln | grep :5000
    kill $TEST_PID
else
    log_error "Socket test failed to run"
fi

# Test the server socket binding
log_info "Testing server socket binding in foreground mode"
SERVER_BIN="$PROJECT_ROOT/build/bin/jsondb_server"

# Check if server binary exists
if [ ! -f "$SERVER_BIN" ]; then
    log_error "Server binary not found at $SERVER_BIN"
    log_info "Attempting to build server"
    (cd "$PROJECT_ROOT/src" && make)
    if [ ! -f "$SERVER_BIN" ]; then
        log_error "Failed to build server"
        exit 1
    fi
fi

# Run server in foreground mode with verbose output
log_info "Starting server in foreground mode on port 5001"
$SERVER_BIN --port 5001 --foreground &
SERVER_PID=$!

# Wait a moment for server to start
sleep 3

# Check if server is running
if ps -p $SERVER_PID > /dev/null; then
    log_info "Server process is running (PID: $SERVER_PID)"
    
    # Check netstat
    log_info "Checking netstat for server port"
    netstat -tuln | grep :5001 || echo "Server port 5001 not found in netstat"
    
    # Try to connect
    log_info "Testing connection to server"
    if curl -s -m 2 http://localhost:5001 > /dev/null; then
        log_info "Successfully connected to server"
    else
        log_error "Failed to connect to server"
    fi
    
    # Stop server
    log_info "Stopping server"
    kill $SERVER_PID
else
    log_error "Server process is not running"
fi

# Final verification
log_info "All debug tests completed"
log_info "Results saved to $LOG_FILE"
