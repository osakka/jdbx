#!/bin/bash
#
# Comprehensive socket binding diagnostic script for JSONdb
# This script performs a full diagnosis of socket binding issues
#

set -e

# Set colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
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

log_section() {
    echo
    echo -e "${BLUE}=== $1 ===${NC}"
}

# Directory paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
DEBUG_DIR="$PROJECT_ROOT/build/debug"
TOOLS_DIR="$DEBUG_DIR/tools"
LOG_DIR="$DEBUG_DIR/logs"

# Ensure directories exist
mkdir -p "$DEBUG_DIR" "$TOOLS_DIR" "$LOG_DIR"

# Timestamp for logs
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
LOG_FILE="$LOG_DIR/socket_diagnosis_${TIMESTAMP}.log"

# Start logging
exec > >(tee -a "$LOG_FILE")
exec 2>&1

# Header
log_section "JSONdb Socket Binding Diagnostic Tool"
log_info "Started at: $(date)"
log_info "Log file: $LOG_FILE"
echo

# Build debug tools if needed
if [ ! -f "$TOOLS_DIR/test_port_binding" ]; then
    log_info "Building debug tools..."
    bash "$SCRIPT_DIR/build_debug_tools.sh"
fi

# System Information
log_section "System Information"
uname -a
echo
log_info "Checking for container environment"
grep -q docker /proc/1/cgroup 2>/dev/null && echo "Running in Docker container" || echo "Not running in Docker container"
echo

# Check network configuration
log_section "Network Configuration"
ip addr
echo
log_info "Routing table"
route -n || ip route
echo
log_info "Listening ports"
netstat -tuln || ss -tuln
echo
log_info "Localhost resolution"
getent hosts localhost
ping -c 1 localhost || echo "Ping to localhost failed"
echo

# Check system permissions
log_section "System Permissions"
log_info "User information"
id
echo
log_info "Checking port binding permissions"
if [[ $(id -u) -eq 0 ]]; then
    echo "Running as root, can bind to any port"
else
    echo "Running as non-root user, can only bind to ports > 1024 without special privileges"
    
    # Check for CAP_NET_BIND_SERVICE capability
    if command -v getcap &> /dev/null; then
        if getcap "$PROJECT_ROOT/build/bin/jsondb_server" | grep -q "cap_net_bind_service"; then
            echo "Server binary has CAP_NET_BIND_SERVICE capability, can bind to privileged ports"
        else
            echo "Server binary does not have CAP_NET_BIND_SERVICE capability"
        fi
    else
        echo "Cannot check capabilities (getcap not available)"
    fi
fi
echo

# Check firewall status
log_section "Firewall Status"
if command -v iptables &> /dev/null; then
    iptables -L -n | head -n 20
else
    echo "iptables not available"
fi
echo

# Server binary check
log_section "Server Binary Check"
SERVER_BIN="$PROJECT_ROOT/build/bin/jsondb_server"
if [ ! -f "$SERVER_BIN" ]; then
    log_error "Server binary not found at $SERVER_BIN"
    log_info "Attempting to build server"
    (cd "$PROJECT_ROOT/src" && make)
    if [ ! -f "$SERVER_BIN" ]; then
        log_error "Failed to build server"
        exit 1
    fi
fi
log_info "Server binary exists: $SERVER_BIN"
file "$SERVER_BIN"
ldd "$SERVER_BIN" || echo "ldd not available"
echo

# Check for running server instances
log_section "Running Server Instances"
pgrep -a jsondb_server || echo "No server instances running"
echo
log_info "Checking for socket usage"
lsof -i :5000 2>/dev/null || echo "No process is using port 5000"
netstat -tuln | grep :5000 || echo "Port 5000 is not in LISTEN state"
echo

# Kill any existing server instances
log_section "Cleaning Up Environment"
log_info "Stopping any running server instances"
pkill -f jsondb_server 2>/dev/null || echo "No server instances found"
sleep 1

# Test standalone socket binding
log_section "Standalone Socket Binding Test"
log_info "Running standalone socket binding test on port 5000"
"$TOOLS_DIR/test_port_binding" 5000 &
TEST_PID=$!
sleep 3

# Check if the test is running
if ps -p $TEST_PID > /dev/null; then
    log_info "Standalone socket test is running successfully (PID: $TEST_PID)"
    
    # Check netstat
    log_info "Checking netstat with socket test running"
    netstat -tuln | grep :5000 || ss -tuln | grep :5000 || echo "Port 5000 not found in netstat/ss despite running test"
    
    # Try to connect
    log_info "Testing connection to standalone socket"
    if curl -s -m 2 http://localhost:5000 > /dev/null; then
        log_info "Successfully connected to standalone socket"
    else
        log_error "Failed to connect to standalone socket"
    fi
    
    # Kill the test
    kill $TEST_PID
    wait $TEST_PID 2>/dev/null || true
else
    log_error "Standalone socket test failed to run or exited prematurely"
fi
echo

# Test server in foreground mode
log_section "Server Foreground Mode Test"
log_info "Starting server in foreground mode on port 5001"
"$SERVER_BIN" --port 5001 --foreground > "$LOG_DIR/server_foreground_${TIMESTAMP}.log" 2>&1 &
SERVER_PID=$!
sleep 3

# Check if server is running
if ps -p $SERVER_PID > /dev/null; then
    log_info "Server is running in foreground mode (PID: $SERVER_PID)"
    
    # Check netstat
    log_info "Checking netstat for server port"
    netstat -tuln | grep :5001 || ss -tuln | grep :5001 || echo "Server port 5001 not found in netstat/ss"
    
    # Try to connect
    log_info "Testing connection to server"
    if curl -s -m 2 http://localhost:5001/health > /dev/null; then
        log_info "Successfully connected to server"
    else
        log_warning "Failed to connect to server health endpoint, trying root path"
        if curl -s -m 2 http://localhost:5001 > /dev/null; then
            log_info "Successfully connected to server root path"
        else
            log_error "Failed to connect to server"
        fi
    fi
    
    # Check server process info
    log_info "Server process information"
    ps -p $SERVER_PID -o pid,ppid,stat,cmd
    
    # Stop server
    log_info "Stopping server"
    kill $SERVER_PID
    wait $SERVER_PID 2>/dev/null || true
else
    log_error "Server failed to start in foreground mode"
    log_info "Server foreground log:"
    cat "$LOG_DIR/server_foreground_${TIMESTAMP}.log"
fi
echo

# Test server in daemon mode
log_section "Server Daemon Mode Test"
log_info "Starting server in daemon mode on port 5002"
"$SERVER_BIN" --port 5002 --log-file "$LOG_DIR/server_daemon_${TIMESTAMP}.log" --pid-file "$LOG_DIR/server_daemon_${TIMESTAMP}.pid"
sleep 3

# Check if server is running
if [ -f "$LOG_DIR/server_daemon_${TIMESTAMP}.pid" ]; then
    DAEMON_PID=$(cat "$LOG_DIR/server_daemon_${TIMESTAMP}.pid")
    if ps -p $DAEMON_PID > /dev/null; then
        log_info "Server is running in daemon mode (PID: $DAEMON_PID)"
        
        # Check netstat
        log_info "Checking netstat for server port"
        netstat -tuln | grep :5002 || ss -tuln | grep :5002 || echo "Server port 5002 not found in netstat/ss"
        
        # Try to connect
        log_info "Testing connection to server"
        if curl -s -m 2 http://localhost:5002/health > /dev/null; then
            log_info "Successfully connected to server"
        else
            log_warning "Failed to connect to server health endpoint, trying root path"
            if curl -s -m 2 http://localhost:5002 > /dev/null; then
                log_info "Successfully connected to server root path"
            else
                log_error "Failed to connect to server"
            fi
        fi
        
        # Check server process info
        log_info "Server process information"
        ps -p $DAEMON_PID -o pid,ppid,stat,cmd
        
        # Stop server
        log_info "Stopping server"
        kill $DAEMON_PID
    else
        log_error "Server PID file exists but process is not running"
    fi
else
    log_error "Server failed to create PID file in daemon mode"
fi

# Check daemon log
if [ -f "$LOG_DIR/server_daemon_${TIMESTAMP}.log" ]; then
    log_info "Server daemon log excerpt:"
    head -n 20 "$LOG_DIR/server_daemon_${TIMESTAMP}.log"
    echo "..."
    tail -n 20 "$LOG_DIR/server_daemon_${TIMESTAMP}.log"
else
    log_error "Server daemon log file was not created"
fi
echo

# Compile and run a low-level socket monitor
log_section "Low-Level Socket Monitoring"
cat > "$DEBUG_DIR/socket_monitor.c" << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {
    int port = 5000;
    if (argc > 1) port = atoi(argv[1]);
    
    /* Create socket */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("Socket creation failed: %s (errno=%d)\n", strerror(errno), errno);
        return 1;
    }
    printf("Socket created successfully (fd=%d)\n", sockfd);
    
    /* Set socket options */
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        printf("Failed to set SO_REUSEADDR: %s (errno=%d)\n", strerror(errno), errno);
    } else {
        printf("Set SO_REUSEADDR successfully\n");
    }
    
    /* Create address structure */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    /* Bind socket */
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("Bind failed: %s (errno=%d)\n", strerror(errno), errno);
        close(sockfd);
        return 1;
    }
    printf("Socket bound successfully to port %d\n", port);
    
    /* Listen */
    if (listen(sockfd, 5) < 0) {
        printf("Listen failed: %s (errno=%d)\n", strerror(errno), errno);
        close(sockfd);
        return 1;
    }
    printf("Socket listening on port %d\n", port);
    
    /* Check socket status */
    int status = 0;
    socklen_t len = sizeof(status);
    if (getsockopt(sockfd, SOL_SOCKET, SO_ACCEPTCONN, &status, &len) < 0) {
        printf("Failed to check socket status: %s (errno=%d)\n", strerror(errno), errno);
    } else {
        printf("Socket listening state: %s\n", status ? "LISTENING" : "NOT LISTENING");
    }
    
    /* Accept connections */
    printf("Waiting for connections... (will exit after first connection)\n");
    
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    /* Set up a timeout using select */
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);
    
    struct timeval tv;
    tv.tv_sec = 30;  /* 30 second timeout */
    tv.tv_usec = 0;
    
    int result = select(sockfd + 1, &read_fds, NULL, NULL, &tv);
    
    if (result < 0) {
        printf("Select failed: %s (errno=%d)\n", strerror(errno), errno);
    } else if (result == 0) {
        printf("Timeout waiting for connection\n");
    } else {
        int client_fd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            printf("Accept failed: %s (errno=%d)\n", strerror(errno), errno);
        } else {
            printf("Accepted connection from %s:%d\n", 
                  inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            
            char response[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 14\r\n\r\nSocket Monitor\r\n";
            write(client_fd, response, strlen(response));
            
            close(client_fd);
        }
    }
    
    printf("Closing socket\n");
    close(sockfd);
    
    return 0;
}
EOF

log_info "Compiling socket monitor"
gcc -o "$DEBUG_DIR/socket_monitor" "$DEBUG_DIR/socket_monitor.c"

log_info "Running socket monitor on port 5003"
"$DEBUG_DIR/socket_monitor" 5003 &
MONITOR_PID=$!
sleep 2

# Test socket connection
log_info "Testing connection to socket monitor"
if curl -s -m 2 http://localhost:5003 > /dev/null; then
    log_info "Successfully connected to socket monitor"
else
    log_warning "Failed to connect to socket monitor"
fi

# Wait for socket monitor to complete
wait $MONITOR_PID 2>/dev/null || true
echo

# Analyze server code
log_section "Server Code Analysis"

# Check socket binding in server.c
log_info "Analyzing socket binding code"
grep -n "bind(" "$PROJECT_ROOT/src/components/core/server.c" --color=auto

# Check thread creation in server.c
log_info "Analyzing thread creation code"
grep -n "pthread_create" "$PROJECT_ROOT/src/components/core/server.c" --color=auto

# Analyze daemon mode in main.c
log_info "Analyzing daemon mode code"
grep -n "daemon" "$PROJECT_ROOT/src/components/main.c" --color=auto || echo "No daemon references found in main.c"

# Summary
log_section "Diagnostic Summary"
log_info "Diagnostic completed at: $(date)"
log_info "Full report saved to: $LOG_FILE"

# Check for common issues
ISSUES_FOUND=0

# Check if standalone socket binding works
if ! grep -q "Socket bound successfully to port 5000" "$LOG_FILE"; then
    log_error "Issue detected: Standalone socket binding failed"
    ISSUES_FOUND=1
fi

# Check if server foreground mode works
if ! grep -q "Successfully connected to server" "$LOG_FILE"; then
    log_error "Issue detected: Server in foreground mode not accepting connections"
    ISSUES_FOUND=1
fi

# Check if server daemon mode works
if ! grep -q "Server is running in daemon mode" "$LOG_FILE"; then
    log_error "Issue detected: Server not starting properly in daemon mode"
    ISSUES_FOUND=1
fi

# Check socket monitoring
if ! grep -q "Socket listening on port 5003" "$LOG_FILE"; then
    log_error "Issue detected: Socket monitor failed to bind/listen"
    ISSUES_FOUND=1
fi

if [ $ISSUES_FOUND -eq 0 ]; then
    log_info "No critical issues detected"
else
    log_warning "Issues detected during diagnosis, see above for details"
fi

echo
log_info "Recommendations:"
log_info "1. Review the full log file for detailed diagnostics"
log_info "2. Check server code for race conditions in socket binding"
log_info "3. Verify thread creation and socket handling in server_start function"
log_info "4. Check for file descriptor preservation during fork in daemon mode"

echo
log_info "To run server with debug logging, use:"
log_info "$SERVER_BIN --foreground --port 5000 --log-level DEBUG"