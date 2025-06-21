#!/bin/bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
BASE_URL="https://localhost:5000"
CURL_OPTS="-s -k"
ADMIN_USER="admin"
ADMIN_PASS="secure123456789"

# Test tracking
TESTS_PASSED=0
TESTS_FAILED=0

# Helper functions
log_test() {
    echo -e "${BLUE}[TEST]${NC} $1"
}

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
    ((TESTS_PASSED++))
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    if [[ -n "$2" ]]; then
        echo -e "${RED}[DETAIL]${NC} $2"
    fi
    ((TESTS_FAILED++))
}

log_info() {
    echo -e "${YELLOW}[INFO]${NC} $1"
}

# Test functions
test_health_check() {
    log_test "Health check endpoint"
    
    local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/health")
    local status=$(echo "$response" | jq -r '.status' 2>/dev/null)
    
    if [[ "$status" == "ok" ]]; then
        log_pass "Health check successful"
        return 0
    else
        log_fail "Health check failed" "$response"
        return 1
    fi
}

test_admin_login() {
    log_test "Admin login"
    
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/auth/login" \
        -H "Content-Type: application/json" \
        -d "{\"username\":\"$ADMIN_USER\",\"password\":\"$ADMIN_PASS\"}")
    
    ADMIN_TOKEN=$(echo "$response" | jq -r '.token' 2>/dev/null)
    
    if [[ -n "$ADMIN_TOKEN" && "$ADMIN_TOKEN" != "null" ]]; then
        log_pass "Admin login successful"
        log_info "Token: ${ADMIN_TOKEN:0:20}..."
        return 0
    else
        log_fail "Admin login failed" "$response"
        return 1
    fi
}

test_js_eval_simple() {
    log_test "JavaScript evaluation - simple expression"
    
    local js_code='{"code": "2 + 2"}'
    
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/js/eval" \
        -H "Authorization: Bearer $ADMIN_TOKEN" \
        -H "Content-Type: application/json" \
        -d "$js_code")
    
    local result=$(echo "$response" | jq -r '.result' 2>/dev/null)
    
    if [[ "$result" == "4" ]]; then
        log_pass "Simple JavaScript evaluation successful"
        return 0
    else
        log_fail "JavaScript evaluation failed" "$response"
        return 1
    fi
}

test_js_eval_json() {
    log_test "JavaScript evaluation - JSON manipulation"
    
    local js_code='{"code": "JSON.stringify({name: \"test\", value: 42})"}'
    
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/js/eval" \
        -H "Authorization: Bearer $ADMIN_TOKEN" \
        -H "Content-Type: application/json" \
        -d "$js_code")
    
    local result=$(echo "$response" | jq -r '.result' 2>/dev/null)
    local parsed_name=$(echo "$result" | jq -r '.name' 2>/dev/null)
    
    if [[ "$parsed_name" == "test" ]]; then
        log_pass "JSON manipulation successful"
        return 0
    else
        log_fail "JSON manipulation failed" "$response"
        return 1
    fi
}

test_js_create_validator() {
    log_test "Create JavaScript validator"
    
    local validator_data='{
        "name": "email_validator",
        "code": "function validate(doc) { const emailRegex = /^[^\\s@]+@[^\\s@]+\\.[^\\s@]+$/; return emailRegex.test(doc.email || \"\"); }",
        "description": "Validates email format"
    }'
    
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/js/validators" \
        -H "Authorization: Bearer $ADMIN_TOKEN" \
        -H "Content-Type: application/json" \
        -d "$validator_data")
    
    VALIDATOR_ID=$(echo "$response" | jq -r '.id // .uuid' 2>/dev/null)
    local name=$(echo "$response" | jq -r '.name' 2>/dev/null)
    
    if [[ "$name" == "email_validator" && -n "$VALIDATOR_ID" && "$VALIDATOR_ID" != "null" ]]; then
        log_pass "Validator created successfully"
        log_info "Validator ID: $VALIDATOR_ID"
        return 0
    else
        log_fail "Validator creation failed" "$response"
        return 1
    fi
}

test_js_list_validators() {
    log_test "List JavaScript validators"
    
    local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/js/validators" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    local count=$(echo "$response" | jq -r '.validators | length' 2>/dev/null)
    
    if [[ "$count" -ge 0 ]]; then
        log_pass "Validators listed successfully"
        log_info "Found $count validators"
        return 0
    else
        log_fail "Validators list failed" "$response"
        return 1
    fi
}

test_js_create_function() {
    log_test "Create JavaScript function"
    
    local function_data='{
        "name": "calculate_discount",
        "code": "function calculate_discount(price, percentage) { return price * (1 - percentage / 100); }",
        "description": "Calculates discounted price"
    }'
    
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/js/functions" \
        -H "Authorization: Bearer $ADMIN_TOKEN" \
        -H "Content-Type: application/json" \
        -d "$function_data")
    
    FUNCTION_ID=$(echo "$response" | jq -r '.id // .uuid' 2>/dev/null)
    local name=$(echo "$response" | jq -r '.name' 2>/dev/null)
    
    if [[ "$name" == "calculate_discount" && -n "$FUNCTION_ID" && "$FUNCTION_ID" != "null" ]]; then
        log_pass "Function created successfully"
        log_info "Function ID: $FUNCTION_ID"
        return 0
    else
        log_fail "Function creation failed" "$response"
        return 1
    fi
}

test_js_execute_function() {
    log_test "Execute JavaScript function"
    
    if [[ -z "$FUNCTION_ID" ]]; then
        log_fail "No function ID available"
        return 1
    fi
    
    local exec_data='{
        "function": "calculate_discount",
        "args": [100, 25]
    }'
    
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/js/execute" \
        -H "Authorization: Bearer $ADMIN_TOKEN" \
        -H "Content-Type: application/json" \
        -d "$exec_data")
    
    local result=$(echo "$response" | jq -r '.result' 2>/dev/null)
    
    if [[ "$result" == "75" ]]; then
        log_pass "Function execution successful"
        return 0
    else
        log_fail "Function execution failed" "$response"
        return 1
    fi
}

test_js_error_handling() {
    log_test "JavaScript error handling"
    
    local bad_code='{"code": "throw new Error(\"Test error\");"}'
    
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/js/eval" \
        -H "Authorization: Bearer $ADMIN_TOKEN" \
        -H "Content-Type: application/json" \
        -d "$bad_code")
    
    local error=$(echo "$response" | jq -r '.error' 2>/dev/null)
    
    if [[ "$error" =~ "Test error" || "$error" =~ "Error" ]]; then
        log_pass "Error handling works correctly"
        return 0
    else
        log_fail "Error handling failed" "$response"
        return 1
    fi
}

# Main test execution
main() {
    echo "===================================="
    echo "JDBX JavaScript E2E Test Suite"
    echo "===================================="
    echo
    
    # Run all tests
    test_health_check
    
    if test_admin_login; then
        test_js_eval_simple
        test_js_eval_json
        test_js_create_validator
        test_js_list_validators
        test_js_create_function
        test_js_execute_function
        test_js_error_handling
    fi
    
    # Summary
    echo
    echo "===================================="
    echo "Test Summary"
    echo "===================================="
    local total=$((TESTS_PASSED + TESTS_FAILED))
    echo "Total Tests: $total"
    echo -e "Passed: ${GREEN}$TESTS_PASSED${NC}"
    echo -e "Failed: ${RED}$TESTS_FAILED${NC}"
    
    local success_rate=0
    if [[ $total -gt 0 ]]; then
        success_rate=$((TESTS_PASSED * 100 / total))
    fi
    
    echo "Success Rate: ${success_rate}%"
    
    if [[ $TESTS_FAILED -eq 0 ]]; then
        echo -e "${GREEN}✅ ALL TESTS PASSED!${NC}"
        exit 0
    else
        echo -e "${RED}❌ TESTS FAILED - Need fixes${NC}"
        exit 1
    fi
}

# Run the tests
main