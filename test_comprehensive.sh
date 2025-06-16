#!/bin/bash

# Comprehensive JDBX Server Functionality Test Suite
# Tests all major server components and features

set -e

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test results tracking
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Helper function to run a test
run_test() {
    local test_name="$1"
    local test_command="$2"
    local expected_result="${3:-0}"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    echo -n "Testing $test_name... "
    
    if eval "$test_command" > /tmp/test_output_$$.txt 2>&1; then
        local exit_code=0
    else
        local exit_code=$?
    fi
    
    if [ $exit_code -eq $expected_result ]; then
        echo -e "${GREEN}PASSED${NC}"
        PASSED_TESTS=$((PASSED_TESTS + 1))
        return 0
    else
        echo -e "${RED}FAILED${NC}"
        echo "  Output:"
        cat /tmp/test_output_$$.txt | head -20
        FAILED_TESTS=$((FAILED_TESTS + 1))
        return 1
    fi
}

# Ensure server is running
echo "=== JDBX Comprehensive Server Test Suite ==="
echo
echo "Starting JDBX server..."
cd /opt/jdbx
./build/jdbx_runtime.sh stop 2>/dev/null || true
sleep 2
./build/jdbx_runtime.sh start
sleep 3

# Check server is running
if ! pgrep -f jdbxd > /dev/null; then
    echo -e "${RED}ERROR: Server failed to start${NC}"
    exit 1
fi

echo -e "${GREEN}Server started successfully${NC}"
echo

# Get admin credentials from environment
ADMIN_USER="${JDBX_BOOTSTRAP_ADMIN_USER:-admin}"
ADMIN_PASS="${JDBX_BOOTSTRAP_ADMIN_PASS:-admin123456789}"

echo "=== 1. Authentication System Tests ==="

# 1.1 Login with correct credentials
run_test "Admin login" \
    "curl -s -k -X POST https://localhost:5000/api/auth/login \
    -H 'Content-Type: application/json' \
    -d '{\"username\":\"$ADMIN_USER\",\"password\":\"$ADMIN_PASS\"}' \
    | grep -q '\"token\"'"

# Extract token for subsequent tests
TOKEN=$(curl -s -k -X POST https://localhost:5000/api/auth/login \
    -H "Content-Type: application/json" \
    -d "{\"username\":\"$ADMIN_USER\",\"password\":\"$ADMIN_PASS\"}" \
    | grep -o '"token":"[^"]*' | cut -d'"' -f4)

# 1.2 Login with incorrect credentials
run_test "Failed login (wrong password)" \
    "curl -s -k -X POST https://localhost:5000/api/auth/login \
    -H 'Content-Type: application/json' \
    -d '{\"username\":\"$ADMIN_USER\",\"password\":\"wrongpass\"}' \
    | grep -q '\"error\"'"

# 1.3 Token validation
run_test "Token validation" \
    "curl -s -k -H 'Authorization: Bearer $TOKEN' \
    https://localhost:5000/api/sessions \
    -o /dev/null -w '%{http_code}' | grep -q '200'"

# 1.4 Invalid token rejection
run_test "Invalid token rejection" \
    "curl -s -k -H 'Authorization: Bearer invalid_token_123' \
    https://localhost:5000/api/sessions \
    -o /dev/null -w '%{http_code}' | grep -q '401'"

echo
echo "=== 2. Library Management Tests ==="

# 2.1 List libraries
run_test "List libraries" \
    "curl -s -k -H 'Authorization: Bearer $TOKEN' \
    https://localhost:5000/api/libraries \
    | grep -q '\"libraries\"'"

# 2.2 Create new library
LIBRARY_NAME="test_library_$$"
run_test "Create library" \
    "curl -s -k -X POST https://localhost:5000/api/libraries \
    -H 'Authorization: Bearer $TOKEN' \
    -H 'Content-Type: application/json' \
    -d '{\"name\":\"$LIBRARY_NAME\",\"description\":\"Test library\"}' \
    | grep -q '\"id\"'"

# 2.3 Get library details
run_test "Get library details" \
    "curl -s -k -H 'Authorization: Bearer $TOKEN' \
    https://localhost:5000/api/libraries \
    | grep -q '$LIBRARY_NAME'"

echo
echo "=== 3. Collection Management Tests ==="

# 3.1 List collections
run_test "List collections" \
    "curl -s -k -H 'Authorization: Bearer $TOKEN' \
    https://localhost:5000/api/collections \
    | grep -q '\"collections\"'"

# 3.2 Create collection
COLLECTION_NAME="test_collection_$$"
run_test "Create collection" \
    "curl -s -k -X POST https://localhost:5000/api/collections \
    -H 'Authorization: Bearer $TOKEN' \
    -H 'Content-Type: application/json' \
    -d '{\"name\":\"$COLLECTION_NAME\",\"library\":\"default\"}' \
    | grep -q '\"success\"'"

# 3.3 List virtual collections
run_test "List virtual collections" \
    "curl -s -k -H 'Authorization: Bearer $TOKEN' \
    https://localhost:5000/api/virtual-collections \
    | grep -q '\"collections\"'"

echo
echo "=== 4. Document Operations Tests ==="

# 4.1 Create document
DOC_ID=""
run_test "Create document" \
    "curl -s -k -X POST https://localhost:5000/api/documents \
    -H 'Authorization: Bearer $TOKEN' \
    -H 'Content-Type: application/json' \
    -d '{\"library\":\"default\",\"collection\":\"$COLLECTION_NAME\",\"document\":{\"title\":\"Test Document\",\"content\":\"Test content\",\"tags\":[\"test\",\"demo\"]}}' \
    | tee /tmp/create_doc_$$.json | grep -q '\"uuid\"'"

# Extract document ID
if [ -f /tmp/create_doc_$$.json ]; then
    DOC_ID=$(grep -o '"uuid":"[^"]*' /tmp/create_doc_$$.json | cut -d'"' -f4)
fi

# 4.2 Get document by ID
if [ ! -z "$DOC_ID" ]; then
    run_test "Get document by ID" \
        "curl -s -k -H 'Authorization: Bearer $TOKEN' \
        https://localhost:5000/api/documents/$DOC_ID \
        | grep -q '\"title\":\"Test Document\"'"
fi

# 4.3 Query documents
run_test "Query documents" \
    "curl -s -k -X POST https://localhost:5000/api/documents/query \
    -H 'Authorization: Bearer $TOKEN' \
    -H 'Content-Type: application/json' \
    -d '{\"library\":\"default\",\"collection\":\"$COLLECTION_NAME\",\"query\":{}}' \
    | grep -q '\"documents\"'"

# 4.4 Update document
if [ ! -z "$DOC_ID" ]; then
    run_test "Update document" \
        "curl -s -k -X PUT https://localhost:5000/api/documents/$DOC_ID \
        -H 'Authorization: Bearer $TOKEN' \
        -H 'Content-Type: application/json' \
        -d '{\"library\":\"default\",\"collection\":\"$COLLECTION_NAME\",\"document\":{\"title\":\"Updated Test Document\",\"content\":\"Updated content\",\"modified\":true}}' \
        | grep -q '\"success\"'"
fi

# 4.5 Delete document
if [ ! -z "$DOC_ID" ]; then
    run_test "Delete document" \
        "curl -s -k -X DELETE https://localhost:5000/api/documents/$DOC_ID?library=default&collection=$COLLECTION_NAME \
        -H 'Authorization: Bearer $TOKEN' \
        -o /dev/null -w '%{http_code}' | grep -q '200'"
fi

echo
echo "=== 5. User Management Tests ==="

# 5.1 List users
run_test "List users" \
    "curl -s -k -H 'Authorization: Bearer $TOKEN' \
    https://localhost:5000/api/users \
    | grep -q '\"users\"'"

# 5.2 Create new user
TEST_USER="testuser_$$"
run_test "Create user" \
    "curl -s -k -X POST https://localhost:5000/api/auth/register \
    -H 'Content-Type: application/json' \
    -d '{\"username\":\"$TEST_USER\",\"password\":\"testpass123\",\"email\":\"$TEST_USER@test.com\"}' \
    | grep -q '\"id\"'"

# 5.3 Get user details
run_test "Get user details" \
    "curl -s -k -H 'Authorization: Bearer $TOKEN' \
    https://localhost:5000/api/users \
    | grep -q '$TEST_USER'"

echo
echo "=== 6. RBAC (Role-Based Access Control) Tests ==="

# 6.1 List roles
run_test "List roles" \
    "curl -s -k -H 'Authorization: Bearer $TOKEN' \
    https://localhost:5000/api/roles \
    | grep -q '\"roles\"'"

# 6.2 Create role
ROLE_NAME="test_role_$$"
run_test "Create role" \
    "curl -s -k -X POST https://localhost:5000/api/roles \
    -H 'Authorization: Bearer $TOKEN' \
    -H 'Content-Type: application/json' \
    -d '{\"name\":\"$ROLE_NAME\",\"permissions\":[\"read:documents\",\"write:documents\"]}' \
    | grep -q '\"id\"'"

# 6.3 Assign role to user
run_test "Assign role to user" \
    "curl -s -k -X POST https://localhost:5000/api/users/$TEST_USER/roles \
    -H 'Authorization: Bearer $TOKEN' \
    -H 'Content-Type: application/json' \
    -d '{\"role\":\"$ROLE_NAME\"}' \
    | grep -q '\"success\"'"

echo
echo "=== 7. Metrics and Monitoring Tests ==="

# 7.1 Get server metrics
run_test "Get server metrics" \
    "curl -s -k -H 'Authorization: Bearer $TOKEN' \
    https://localhost:5000/api/metrics \
    | grep -q '\"metrics\"'"

# 7.2 Get library metrics
run_test "Get library metrics" \
    "curl -s -k -H 'Authorization: Bearer $TOKEN' \
    https://localhost:5000/api/libraries/default/metrics \
    | grep -q '\"operations\"'"

# 7.3 Get server info
run_test "Get server info" \
    "curl -s -k -H 'Authorization: Bearer $TOKEN' \
    https://localhost:5000/api/system/info \
    | grep -q '\"version\"'"

echo
echo "=== 8. JavaScript Function Tests ==="

# 8.1 List JS functions
run_test "List JavaScript functions" \
    "curl -s -k -H 'Authorization: Bearer $TOKEN' \
    https://localhost:5000/api/js/functions \
    | grep -q '\"functions\"'"

# 8.2 Create JS function
run_test "Create JavaScript function" \
    "curl -s -k -X POST https://localhost:5000/api/js/functions \
    -H 'Authorization: Bearer $TOKEN' \
    -H 'Content-Type: application/json' \
    -d '{\"name\":\"test_function\",\"code\":\"function test() { return 42; }\",\"type\":\"custom\"}' \
    | grep -q '\"id\"'"

# 8.3 Execute JS function
run_test "Execute JavaScript function" \
    "curl -s -k -X POST https://localhost:5000/api/js/execute \
    -H 'Authorization: Bearer $TOKEN' \
    -H 'Content-Type: application/json' \
    -d '{\"function\":\"test_function\",\"args\":[]}' \
    | grep -q '\"result\"'"

echo
echo "=== 9. Session Management Tests ==="

# 9.1 Get active sessions
run_test "Get active sessions" \
    "curl -s -k -H 'Authorization: Bearer $TOKEN' \
    https://localhost:5000/api/sessions/active \
    | grep -q '\"sessions\"'"

# 9.2 Logout
run_test "Logout" \
    "curl -s -k -X POST https://localhost:5000/api/auth/logout \
    -H 'Authorization: Bearer $TOKEN' \
    | grep -q '\"success\"'"

# 9.3 Verify token invalidated
run_test "Token invalidated after logout" \
    "curl -s -k -H 'Authorization: Bearer $TOKEN' \
    https://localhost:5000/api/sessions \
    -o /dev/null -w '%{http_code}' | grep -q '401'"

echo
echo "=== 10. Performance and Concurrency Tests ==="

# 10.1 Concurrent read operations
echo -n "Testing concurrent read operations (20 parallel)... "
SUCCESS_COUNT=0
for i in {1..20}; do
    curl -s -k https://localhost:5000/api/health > /tmp/perf_test_$i.out 2>&1 &
done
wait

for i in {1..20}; do
    if grep -q "ok" /tmp/perf_test_$i.out 2>/dev/null; then
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
    fi
    rm -f /tmp/perf_test_$i.out
done

if [ $SUCCESS_COUNT -eq 20 ]; then
    echo -e "${GREEN}PASSED${NC} (20/20 successful)"
    PASSED_TESTS=$((PASSED_TESTS + 1))
else
    echo -e "${RED}FAILED${NC} ($SUCCESS_COUNT/20 successful)"
    FAILED_TESTS=$((FAILED_TESTS + 1))
fi
TOTAL_TESTS=$((TOTAL_TESTS + 1))

# 10.2 Server still running after all tests
run_test "Server stability check" \
    "pgrep -f jdbxd > /dev/null"

echo
echo "=== Test Summary ==="
echo "Total Tests: $TOTAL_TESTS"
echo -e "Passed: ${GREEN}$PASSED_TESTS${NC}"
echo -e "Failed: ${RED}$FAILED_TESTS${NC}"

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "\n${GREEN}✅ ALL TESTS PASSED!${NC}"
    exit 0
else
    echo -e "\n${RED}❌ Some tests failed${NC}"
    exit 1
fi