#!/bin/bash

# SSL Thread Safety Debug Test Script
# This script performs individual operations with detailed logging to identify
# exactly when and how the SSL-related crashes occur

# Configuration
HOST="https://localhost:5000"
USERNAME="admin"
PASSWORD="secure123456789"
LOG_FILE="/opt/jdbx/tests/ssl_debug_$(date +%Y%m%d_%H%M%S).log"
SERVER_PID_FILE="/opt/jdbx/build/var/jdbxd.pid"
SERVER_LOG="/opt/jdbx/build/var/jdbxd.log"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to log with timestamp
log() {
    local message="$1"
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S.%3N')
    echo -e "${timestamp} - ${message}" | tee -a "$LOG_FILE"
}

# Function to check if server is running
check_server_status() {
    if [ -f "$SERVER_PID_FILE" ]; then
        local pid=$(cat "$SERVER_PID_FILE")
        if kill -0 "$pid" 2>/dev/null; then
            return 0
        else
            return 1
        fi
    else
        return 1
    fi
}

# Function to get last few lines of server log
get_server_log_tail() {
    if [ -f "$SERVER_LOG" ]; then
        echo "=== Last 20 lines of server log ==="
        tail -20 "$SERVER_LOG"
        echo "==================================="
    fi
}

# Function to perform safe curl with detailed error reporting
safe_curl() {
    local method="$1"
    local url="$2"
    local data="$3"
    local token="$4"
    
    local curl_cmd="curl -k -s -w '\n%{http_code}' -X $method"
    
    if [ -n "$token" ]; then
        curl_cmd="$curl_cmd -H 'Authorization: Bearer $token'"
    fi
    
    if [ -n "$data" ]; then
        curl_cmd="$curl_cmd -H 'Content-Type: application/json' -d '$data'"
    fi
    
    curl_cmd="$curl_cmd '$url'"
    
    # Execute curl and capture both output and exit code
    local start_time=$(date +%s.%N)
    local response=$(eval $curl_cmd 2>&1)
    local curl_exit_code=$?
    local end_time=$(date +%s.%N)
    local duration=$(echo "$end_time - $start_time" | bc)
    
    # Extract HTTP code from response (last line)
    local http_code=$(echo "$response" | tail -1)
    local body=$(echo "$response" | sed '$d')
    
    log "  Curl exit code: $curl_exit_code, HTTP code: $http_code, Duration: ${duration}s"
    
    if [ $curl_exit_code -ne 0 ]; then
        log "  ${RED}CURL ERROR${NC}: Exit code $curl_exit_code"
        if [[ "$response" == *"SSL"* ]] || [[ "$response" == *"TLS"* ]]; then
            log "  ${RED}SSL/TLS Error detected${NC}: $response"
        fi
        return 1
    fi
    
    echo "$body"
    return 0
}

# Function to create a document
create_document() {
    local token="$1"
    local doc_num="$2"
    local timestamp=$(date -u +"%Y-%m-%dT%H:%M:%S.%3NZ")
    
    local doc_data='{
        "title": "SSL Debug Test Document '"$doc_num"'",
        "content": "This is a test document created to debug SSL thread safety issues. Document number: '"$doc_num"'. Created at: '"$timestamp"'",
        "metadata": {
            "test_run": "ssl_debug_test",
            "operation_number": '"$doc_num"',
            "timestamp": "'"$timestamp"'"
        }
    }'
    
    safe_curl "POST" "$HOST/api/documents" "$doc_data" "$token"
}

# Function to delete a document
delete_document() {
    local token="$1"
    local doc_id="$2"
    
    safe_curl "DELETE" "$HOST/api/documents/$doc_id" "" "$token"
}

# Main test execution
main() {
    log "${BLUE}=== SSL Thread Safety Debug Test Starting ===${NC}"
    log "Log file: $LOG_FILE"
    log "Server PID file: $SERVER_PID_FILE"
    log "Server log: $SERVER_LOG"
    
    # Check initial server status
    if check_server_status; then
        log "${GREEN}✓ Server is running${NC}"
    else
        log "${RED}✗ Server is not running!${NC}"
        exit 1
    fi
    
    # Mark server log position
    if [ -f "$SERVER_LOG" ]; then
        local log_start_line=$(wc -l < "$SERVER_LOG")
        log "Server log starting at line: $log_start_line"
    fi
    
    # Login to get JWT token
    log "\n${YELLOW}Step 1: Authenticating...${NC}"
    local auth_response=$(safe_curl "POST" "$HOST/api/auth/login" '{"username":"'"$USERNAME"'","password":"'"$PASSWORD"'"}' "")
    
    if [ $? -ne 0 ]; then
        log "${RED}✗ Authentication failed - curl error${NC}"
        check_server_status || log "${RED}SERVER CRASHED during authentication!${NC}"
        get_server_log_tail
        exit 1
    fi
    
    local token=$(echo "$auth_response" | grep -o '"token":"[^"]*' | cut -d'"' -f4)
    if [ -z "$token" ]; then
        log "${RED}✗ Failed to extract JWT token${NC}"
        log "Response: $auth_response"
        exit 1
    fi
    
    log "${GREEN}✓ Authentication successful${NC}"
    log "Token (first 20 chars): ${token:0:20}..."
    
    # Array to store created document IDs
    declare -a created_docs
    
    # Perform operations with detailed tracking
    log "\n${YELLOW}Step 2: Performing create/delete operations...${NC}"
    
    for i in {1..20}; do
        log "\n${BLUE}--- Operation $i/20 ---${NC}"
        
        # Check server status before operation
        if ! check_server_status; then
            log "${RED}✗ SERVER CRASHED before operation $i!${NC}"
            log "Last successful operation: $((i-1))"
            get_server_log_tail
            break
        fi
        
        # Create document
        log "Creating document $i..."
        local create_start=$(date +%s.%N)
        local create_response=$(create_document "$token" "$i")
        local create_exit=$?
        local create_end=$(date +%s.%N)
        local create_duration=$(echo "$create_end - $create_start" | bc)
        
        if [ $create_exit -ne 0 ]; then
            log "${RED}✗ Create operation $i failed${NC}"
            
            # Check if server crashed
            if ! check_server_status; then
                log "${RED}✗ SERVER CRASHED during create operation $i!${NC}"
                log "Operation duration before crash: ${create_duration}s"
                get_server_log_tail
                break
            fi
        else
            local doc_id=$(echo "$create_response" | grep -o '"uuid":"[^"]*' | cut -d'"' -f4)
            if [ -n "$doc_id" ]; then
                created_docs+=("$doc_id")
                log "${GREEN}✓ Created document: $doc_id${NC} (${create_duration}s)"
            else
                log "${YELLOW}⚠ Document created but no ID returned${NC}"
            fi
        fi
        
        # Small delay between create and delete
        sleep 0.1
        
        # Delete the document if we have an ID
        if [ -n "$doc_id" ]; then
            log "Deleting document $doc_id..."
            local delete_start=$(date +%s.%N)
            local delete_response=$(delete_document "$token" "$doc_id")
            local delete_exit=$?
            local delete_end=$(date +%s.%N)
            local delete_duration=$(echo "$delete_end - $delete_start" | bc)
            
            if [ $delete_exit -ne 0 ]; then
                log "${RED}✗ Delete operation $i failed${NC}"
                
                # Check if server crashed
                if ! check_server_status; then
                    log "${RED}✗ SERVER CRASHED during delete operation $i!${NC}"
                    log "Operation duration before crash: ${delete_duration}s"
                    get_server_log_tail
                    break
                fi
            else
                log "${GREEN}✓ Deleted document${NC} (${delete_duration}s)"
            fi
        fi
        
        # Check memory usage periodically
        if [ $((i % 5)) -eq 0 ]; then
            if [ -f "$SERVER_PID_FILE" ]; then
                local pid=$(cat "$SERVER_PID_FILE")
                local mem_usage=$(ps -o rss= -p "$pid" 2>/dev/null || echo "N/A")
                log "Server memory usage: ${mem_usage} KB"
            fi
        fi
        
        # Small delay between operations
        sleep 0.2
    done
    
    # Final status check
    log "\n${YELLOW}Step 3: Final status check...${NC}"
    if check_server_status; then
        log "${GREEN}✓ Server is still running after all operations${NC}"
        
        # Count successful operations
        local success_count=${#created_docs[@]}
        log "Successfully created $success_count documents"
        
        # Get memory usage
        if [ -f "$SERVER_PID_FILE" ]; then
            local pid=$(cat "$SERVER_PID_FILE")
            local final_mem=$(ps -o rss= -p "$pid" 2>/dev/null || echo "N/A")
            log "Final server memory usage: ${final_mem} KB"
        fi
    else
        log "${RED}✗ Server has crashed${NC}"
    fi
    
    # Extract any segfault or error messages from server log
    log "\n${YELLOW}Step 4: Analyzing server log for errors...${NC}"
    if [ -f "$SERVER_LOG" ]; then
        log "Checking for segmentation faults..."
        grep -i "segmentation\|general protection\|fault\|error\|SSL\|crypto" "$SERVER_LOG" | tail -20 | while read -r line; do
            log "  ${RED}ERROR${NC}: $line"
        done
        
        # Get full crash context if server crashed
        if ! check_server_status; then
            log "\n${RED}Full crash context:${NC}"
            get_server_log_tail
        fi
    fi
    
    log "\n${BLUE}=== Test Complete ===${NC}"
    log "Full log saved to: $LOG_FILE"
    
    # Return appropriate exit code
    if check_server_status; then
        exit 0
    else
        exit 1
    fi
}

# Execute main function
main