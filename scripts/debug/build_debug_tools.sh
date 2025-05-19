#!/bin/bash
#
# Build and prepare debug tools for JSONdb socket testing
#

set -e

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

# Directory paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
DEBUG_DIR="$PROJECT_ROOT/build/debug"
TOOLS_DIR="$DEBUG_DIR/tools"

# Create required directories
log_info "Creating debug directories"
mkdir -p "$DEBUG_DIR"
mkdir -p "$TOOLS_DIR"
mkdir -p "$DEBUG_DIR/logs"

# Build port binding test utility
log_info "Building port binding test utility"
gcc -o "$TOOLS_DIR/test_port_binding" "$SCRIPT_DIR/test_port_binding.c" -Wall -Wextra

# Build thread monitor
log_info "Building thread monitor utility"
gcc -o "$TOOLS_DIR/thread_monitor" "$PROJECT_ROOT/src/components/core/server_thread_debug.c" -DTHREAD_MONITOR_TEST -Wall -Wextra -pthread

# Build process monitor
log_info "Building process monitor utility"
gcc -o "$TOOLS_DIR/process_monitor" "$PROJECT_ROOT/src/components/core/process_monitor.c" -DPROCESS_MONITOR_TEST -Wall -Wextra

# Create a wrapper script for port binding test
cat > "$TOOLS_DIR/run_port_binding_test.sh" << 'EOF'
#!/bin/bash

# Default port
PORT=5000

# Check if port argument is provided
if [ $# -gt 0 ]; then
    PORT=$1
fi

# Run the test
"$(dirname "$0")/test_port_binding" $PORT

# Display instructions
echo
echo "Port binding test complete. Check the log file at /tmp/port_binding_test_${PORT}.log"
echo "To connect to the server manually, use: curl http://localhost:${PORT}"
echo
EOF
chmod +x "$TOOLS_DIR/run_port_binding_test.sh"

# Create a script to run thread monitor test
cat > "$TOOLS_DIR/run_thread_monitor.sh" << 'EOF'
#!/bin/bash

LOG_FILE="/tmp/thread_monitor_$(date +%Y%m%d_%H%M%S).log"

echo "Starting thread monitor test. Log file: $LOG_FILE"
"$(dirname "$0")/thread_monitor"

echo "Thread monitor test complete. Check the log file: $LOG_FILE"
EOF
chmod +x "$TOOLS_DIR/run_thread_monitor.sh"

# Create a script to run process monitor test
cat > "$TOOLS_DIR/run_process_monitor.sh" << 'EOF'
#!/bin/bash

LOG_FILE="/tmp/process_monitor_$(date +%Y%m%d_%H%M%S).log"

echo "Starting process monitor test. Log file: $LOG_FILE"
"$(dirname "$0")/process_monitor"

echo "Process monitor test complete. Check the log file: $LOG_FILE"
EOF
chmod +x "$TOOLS_DIR/run_process_monitor.sh"

# Create a script to run socket tests
cat > "$TOOLS_DIR/debug_socket_binding.sh" << 'EOF'
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
EOF
chmod +x "$TOOLS_DIR/debug_socket_binding.sh"

# Print summary
log_info "Debug tools built and prepared successfully"
log_info "Tools directory: $TOOLS_DIR"
log_info "Available tools:"
log_info "  - test_port_binding: Tests basic TCP port binding"
log_info "  - thread_monitor: Tests thread creation and monitoring"
log_info "  - process_monitor: Tests process monitoring"
log_info "  - run_port_binding_test.sh: Wrapper to run port binding test"
log_info "  - run_thread_monitor.sh: Wrapper to run thread monitor test"
log_info "  - run_process_monitor.sh: Wrapper to run process monitor test"
log_info "  - debug_socket_binding.sh: Comprehensive socket binding debug"
log_info
log_info "Usage example: $TOOLS_DIR/debug_socket_binding.sh"