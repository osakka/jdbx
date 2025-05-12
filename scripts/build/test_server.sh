#!/bin/bash
# Test script for JSONdb server
# This script runs a series of tests against the server

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
SERVER_BIN="${BUILD_DIR}/bin/jsondb_server"
SERVER_PORT=8080
SERVER_URL="http://localhost:${SERVER_PORT}"
SERVER_LOG="${BUILD_DIR}/server.log"
TEST_TIMEOUT=5
MAX_STARTUP_WAIT=10

# Print section header
print_section() {
    echo -e "\n${BLUE}===================================="
    echo -e " $1"
    echo -e "====================================${NC}"
}

# Print success message
print_success() {
    echo -e "${GREEN}SUCCESS: $1${NC}"
}

# Print error message
print_error() {
    echo -e "${RED}ERROR: $1${NC}"
}

# Print info message
print_info() {
    echo -e "${YELLOW}INFO: $1${NC}"
}

# Check if the server binary exists
check_server_binary() {
    print_section "Checking server binary"
    
    if [ ! -f "$SERVER_BIN" ]; then
        print_error "Server binary not found: $SERVER_BIN"
        print_info "Please build the server first using: scripts/build/build_server.sh"
        exit 1
    fi
    
    print_success "Server binary found: $SERVER_BIN"
}

# Start the server for testing
start_server() {
    print_section "Starting server"
    
    # Check if server is already running
    if curl -s -o /dev/null -w "%{http_code}" "$SERVER_URL" &> /dev/null; then
        print_error "Server is already running at $SERVER_URL"
        print_info "Please stop the running server first."
        exit 1
    fi
    
    # Start the server in the background
    print_info "Starting server at $SERVER_URL..."
    $SERVER_BIN --port=$SERVER_PORT > "$SERVER_LOG" 2>&1 &
    SERVER_PID=$!
    
    # Wait for the server to start
    print_info "Waiting for server to start (PID: $SERVER_PID)..."
    for i in $(seq 1 $MAX_STARTUP_WAIT); do
        sleep 1
        if curl -s -o /dev/null -w "%{http_code}" "$SERVER_URL" &> /dev/null; then
            print_success "Server started successfully"
            return 0
        fi
        print_info "Waiting... ($i/$MAX_STARTUP_WAIT)"
    done
    
    print_error "Server failed to start within $MAX_STARTUP_WAIT seconds"
    kill -9 $SERVER_PID &> /dev/null
    cat "$SERVER_LOG"
    exit 1
}

# Stop the server
stop_server() {
    print_section "Stopping server"
    
    if [ -z "$SERVER_PID" ]; then
        print_info "No server PID found, trying to find it..."
        SERVER_PID=$(ps aux | grep "$SERVER_BIN" | grep -v grep | awk '{print $2}')
    fi
    
    if [ -n "$SERVER_PID" ]; then
        print_info "Stopping server (PID: $SERVER_PID)..."
        kill $SERVER_PID
        wait $SERVER_PID 2>/dev/null
        print_success "Server stopped"
    else
        print_info "No running server found"
    fi
}

# Test basic server connectivity
test_basic_connectivity() {
    print_section "Testing basic connectivity"
    
    # Test if server responds
    response=$(curl -s -o /dev/null -w "%{http_code}" "$SERVER_URL")
    
    if [ "$response" = "200" ]; then
        print_success "Server responded with 200 OK"
    else
        print_error "Server responded with unexpected status code: $response"
        return 1
    fi
    
    return 0
}

# Test creating a collection
test_create_collection() {
    print_section "Testing collection creation"
    
    # Create a test collection
    response=$(curl -s -X POST "$SERVER_URL/api/collections" \
        -H "Content-Type: application/json" \
        -d '{"name":"test_collection"}')
    
    if echo "$response" | grep -q "test_collection"; then
        print_success "Collection created successfully"
    else
        print_error "Failed to create collection: $response"
        return 1
    fi
    
    return 0
}

# Test creating a document
test_create_document() {
    print_section "Testing document creation"
    
    # Create a test document
    response=$(curl -s -X POST "$SERVER_URL/api/collections/test_collection/documents" \
        -H "Content-Type: application/json" \
        -d '{
            "name": "Test Document",
            "value": 42,
            "tags": ["test", "example"]
        }')
    
    # Extract document ID
    DOC_ID=$(echo "$response" | grep -o '"_id":"[^"]*"' | cut -d'"' -f4)
    
    if [ -n "$DOC_ID" ]; then
        print_success "Document created with ID: $DOC_ID"
        export DOC_ID
    else
        print_error "Failed to create document: $response"
        return 1
    fi
    
    return 0
}

# Test retrieving a document
test_get_document() {
    print_section "Testing document retrieval"
    
    if [ -z "$DOC_ID" ]; then
        print_error "No document ID found. Please run test_create_document first."
        return 1
    fi
    
    # Get the document
    response=$(curl -s "$SERVER_URL/api/collections/test_collection/documents/$DOC_ID")
    
    if echo "$response" | grep -q "$DOC_ID"; then
        print_success "Document retrieved successfully"
    else
        print_error "Failed to retrieve document: $response"
        return 1
    fi
    
    return 0
}

# Test query functionality
test_query() {
    print_section "Testing query functionality"
    
    # Query all documents
    response=$(curl -s "$SERVER_URL/api/collections/test_collection/documents")
    
    if echo "$response" | grep -q "documents"; then
        print_success "Query executed successfully"
    else
        print_error "Failed to query documents: $response"
        return 1
    fi
    
    return 0
}

# Test update functionality
test_update_document() {
    print_section "Testing document update"
    
    if [ -z "$DOC_ID" ]; then
        print_error "No document ID found. Please run test_create_document first."
        return 1
    fi
    
    # Update the document
    response=$(curl -s -X PUT "$SERVER_URL/api/collections/test_collection/documents/$DOC_ID" \
        -H "Content-Type: application/json" \
        -d '{
            "name": "Updated Document",
            "value": 100,
            "tags": ["test", "example", "updated"]
        }')
    
    if echo "$response" | grep -q "Updated Document"; then
        print_success "Document updated successfully"
    else
        print_error "Failed to update document: $response"
        return 1
    fi
    
    return 0
}

# Test delete functionality
test_delete_document() {
    print_section "Testing document deletion"
    
    if [ -z "$DOC_ID" ]; then
        print_error "No document ID found. Please run test_create_document first."
        return 1
    fi
    
    # Delete the document
    response=$(curl -s -X DELETE -w "%{http_code}" "$SERVER_URL/api/collections/test_collection/documents/$DOC_ID" -o /dev/null)
    
    if [ "$response" = "200" ] || [ "$response" = "204" ]; then
        print_success "Document deleted successfully"
    else
        print_error "Failed to delete document: HTTP $response"
        return 1
    fi
    
    return 0
}

# Test JavaScript execution if enabled
test_javascript() {
    print_section "Testing JavaScript execution"
    
    # Check if JavaScript is enabled
    response=$(curl -s "$SERVER_URL/api/js/status")
    
    if echo "$response" | grep -q "enabled"; then
        print_info "JavaScript is enabled, testing execution..."
        
        # Test executing a simple JavaScript function
        response=$(curl -s -X POST "$SERVER_URL/api/js/execute" \
            -H "Content-Type: application/json" \
            -d '{
                "code": "function test() { return { result: 42, message: \"Hello from JavaScript\" }; }; test();"
            }')
        
        if echo "$response" | grep -q "Hello from JavaScript"; then
            print_success "JavaScript execution successful"
        else
            print_error "JavaScript execution failed: $response"
            return 1
        fi
    else
        print_info "JavaScript is disabled, skipping test"
    fi
    
    return 0
}

# Run all tests
run_all_tests() {
    print_section "Running all tests"
    
    # Keep track of failed tests
    failed_tests=0
    
    # Run each test
    test_basic_connectivity || ((failed_tests++))
    test_create_collection || ((failed_tests++))
    test_create_document || ((failed_tests++))
    test_get_document || ((failed_tests++))
    test_query || ((failed_tests++))
    test_update_document || ((failed_tests++))
    test_delete_document || ((failed_tests++))
    test_javascript || ((failed_tests++))
    
    # Print summary
    print_section "Test Summary"
    
    if [ $failed_tests -eq 0 ]; then
        print_success "All tests passed successfully!"
    else
        print_error "$failed_tests test(s) failed"
    fi
    
    return $failed_tests
}

# Main function
main() {
    print_section "JSONdb Server Test Suite"
    
    # Set up trap to ensure server is stopped
    trap stop_server EXIT
    
    # Check if server binary exists
    check_server_binary
    
    # Start the server
    start_server
    
    # Run all tests
    run_all_tests
    
    # Display logs if tests failed
    if [ $? -ne 0 ]; then
        print_section "Server Logs"
        cat "$SERVER_LOG"
    fi
}

# Execute the main function
main