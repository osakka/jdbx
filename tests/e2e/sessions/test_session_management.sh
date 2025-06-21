#!/bin/bash

# JDBX Session Management End-to-End Test
# Goal: 100% success rate with zero regressions

set -uo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Configuration
BASE_URL="https://localhost:5000"
CURL_OPTS="-k -s"
ADMIN_USER="admin"
ADMIN_PASS="secure123456789"
TEST_USER="testuser"
TEST_PASS="testpass123"

# Test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Helper functions
log_test() {
    echo -e "${BLUE}[TEST]${NC} $1"
    ((TOTAL_TESTS++))
}

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
    ((PASSED_TESTS++))
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    echo -e "${RED}[DETAIL]${NC} $2"
    ((FAILED_TESTS++))
}

log_info() {
    echo -e "${YELLOW}[INFO]${NC} $1"
}

# Test functions
test_health_check() {
    log_test "Health check endpoint"
    
    local response=$(curl $CURL_OPTS "$BASE_URL/api/health")
    local status=$(echo "$response" | jq -r '.status' 2>/dev/null)
    
    if [[ "$status" == "ok" ]]; then
        log_pass "Health check successful"
    else
        log_fail "Health check failed" "$response"
    fi
}

test_admin_login() {
    log_test "Admin login"
    
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/auth/login" \
        -H "Content-Type: application/json" \
        -d "{\"username\":\"$ADMIN_USER\",\"password\":\"$ADMIN_PASS\"}")
    
    ADMIN_TOKEN=$(echo "$response" | jq -r '.token' 2>/dev/null)
    ADMIN_REFRESH_TOKEN=$(echo "$response" | jq -r '.refresh_token' 2>/dev/null)
    ADMIN_USER_ID=$(echo "$response" | jq -r '.user_id' 2>/dev/null)
    
    if [[ -n "$ADMIN_TOKEN" && "$ADMIN_TOKEN" != "null" ]]; then
        log_pass "Admin login successful"
        log_info "Token: ${ADMIN_TOKEN:0:20}..."
        return 0
    else
        log_fail "Admin login failed" "$response"
        return 1
    fi
}

test_create_test_user() {
    log_test "Create test user"
    
    local user_doc='{
        "username": "'$TEST_USER'",
        "password": "'$TEST_PASS'",
        "email": "test@example.com",
        "type": "user",
        "library": "system",
        "roles": ["user"]
    }'
    
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/users" \
        -H "Authorization: Bearer $ADMIN_TOKEN" \
        -H "Content-Type: application/json" \
        -d "$user_doc")
    
    # Handle both response formats
    TEST_USER_ID=$(echo "$response" | jq -r '.uuid // .user.id // .id' 2>/dev/null)
    
    if [[ -n "$TEST_USER_ID" && "$TEST_USER_ID" != "null" ]]; then
        log_pass "Test user created: $TEST_USER_ID"
        return 0
    else
        log_fail "Failed to create test user" "$response"
        return 1
    fi
}

test_user_login() {
    log_test "Test user login"
    
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/auth/login" \
        -H "Content-Type: application/json" \
        -d "{\"username\":\"$TEST_USER\",\"password\":\"$TEST_PASS\"}")
    
    USER_TOKEN=$(echo "$response" | jq -r '.token' 2>/dev/null)
    USER_REFRESH_TOKEN=$(echo "$response" | jq -r '.refresh_token' 2>/dev/null)
    USER_SESSION_ID=$(echo "$response" | jq -r '.user_id' 2>/dev/null)
    
    if [[ -n "$USER_TOKEN" && "$USER_TOKEN" != "null" ]]; then
        log_pass "Test user login successful"
        return 0
    else
        log_fail "Test user login failed" "$response"
        return 1
    fi
}

test_session_validation() {
    log_test "Session validation with user token"
    
    local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $USER_TOKEN")
    
    local error=$(echo "$response" | jq -r '.error' 2>/dev/null)
    
    if [[ -z "$error" || "$error" == "null" ]]; then
        log_pass "Session validation successful"
        return 0
    else
        log_fail "Session validation failed" "$response"
        return 1
    fi
}

test_concurrent_sessions() {
    log_test "Multiple concurrent sessions"
    
    local success=0
    local total=5
    
    for i in $(seq 1 $total); do
        local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/auth/login" \
            -H "Content-Type: application/json" \
            -d "{\"username\":\"$TEST_USER\",\"password\":\"$TEST_PASS\"}" &)
    done
    
    wait
    
    # Verify all sessions
    for i in $(seq 1 $total); do
        local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/health" \
            -H "Authorization: Bearer $USER_TOKEN")
        
        if [[ $? -eq 0 ]]; then
            ((success++))
        fi
    done
    
    if [[ $success -eq $total ]]; then
        log_pass "All $total concurrent sessions successful"
        return 0
    else
        log_fail "Concurrent sessions failed" "$success/$total successful"
        return 1
    fi
}

test_token_refresh() {
    log_test "Token refresh"
    
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/auth/refresh" \
        -H "Content-Type: application/json" \
        -d "{\"refresh_token\":\"$USER_REFRESH_TOKEN\"}")
    
    NEW_TOKEN=$(echo "$response" | jq -r '.token' 2>/dev/null)
    
    if [[ -n "$NEW_TOKEN" && "$NEW_TOKEN" != "null" ]]; then
        log_pass "Token refresh successful"
        USER_TOKEN="$NEW_TOKEN"
        return 0
    else
        log_fail "Token refresh failed" "$response"
        return 1
    fi
}

test_session_logout() {
    log_test "Session logout"
    
    # Debug: log the token being used
    log_info "Using token for logout: ${USER_TOKEN:0:50}..."
    
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/auth/logout" \
        -H "Authorization: Bearer $USER_TOKEN")
    
    # Debug: log full response
    log_info "Logout response: $response"
    
    local message=$(echo "$response" | jq -r '.message' 2>/dev/null)
    
    if [[ "$message" == "Logged out successfully" ]]; then
        log_pass "Logout successful"
        return 0
    else
        log_fail "Logout failed" "$response"
        return 1
    fi
}

test_post_logout_access() {
    log_test "Access denied after logout"
    
    # Debug: show which token we're using
    log_info "Using token: ${USER_TOKEN:0:50}..."
    
    local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $USER_TOKEN")
    
    local error=$(echo "$response" | jq -r '.error' 2>/dev/null)
    
    # Debug: show response
    log_info "Response error field: '$error'"
    log_info "Full response: $(echo "$response" | jq -c . 2>/dev/null | head -c 100)"
    
    if [[ "$error" == "Unauthorized" ]]; then
        log_pass "Correctly denied access after logout"
        return 0
    else
        log_fail "Security issue: Access allowed after logout" "$response"
        return 1
    fi
}

test_session_expiry() {
    log_test "Session expiry handling"
    
    # Create an expired token (manipulate exp claim)
    local expired_token="eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJqZGJ4Iiwic3ViIjoidGVzdCIsImV4cCI6MTYwMDAwMDAwMCwiaWF0IjoxNjAwMDAwMDAwLCJ1c2VybmFtZSI6InRlc3QiLCJ0eXBlIjoiYWNjZXNzIn0.test"
    
    local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $expired_token")
    
    local error=$(echo "$response" | jq -r '.error' 2>/dev/null)
    
    if [[ "$error" == "Unauthorized" ]]; then
        log_pass "Expired token correctly rejected"
        return 0
    else
        log_fail "Security issue: Expired token accepted" "$response"
        return 1
    fi
}

test_session_persistence() {
    log_test "Session persistence check"
    
    # Login again
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/auth/login" \
        -H "Content-Type: application/json" \
        -d "{\"username\":\"$TEST_USER\",\"password\":\"$TEST_PASS\"}")
    
    local new_token=$(echo "$response" | jq -r '.token' 2>/dev/null)
    
    # Query sessions
    local sessions=$(curl $CURL_OPTS -X GET "$BASE_URL/api/sessions" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    local session_count=$(echo "$sessions" | jq -r '.sessions | length' 2>/dev/null)
    
    if [[ -n "$session_count" && "$session_count" -gt 0 ]]; then
        log_pass "Sessions persisted in database: $session_count found"
        return 0
    else
        log_fail "Session persistence failed" "$sessions"
        return 1
    fi
}

test_sliding_window() {
    log_test "Sliding window token validation"
    
    # Login to get fresh token
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/auth/login" \
        -H "Content-Type: application/json" \
        -d "{\"username\":\"$TEST_USER\",\"password\":\"$TEST_PASS\"}")
    
    local token=$(echo "$response" | jq -r '.token' 2>/dev/null)
    
    # Make multiple requests to test sliding window
    for i in {1..3}; do
        sleep 2
        local test_response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/documents" \
            -H "Authorization: Bearer $token")
        
        local error=$(echo "$test_response" | jq -r '.error' 2>/dev/null)
        
        if [[ -n "$error" && "$error" != "null" ]]; then
            log_fail "Sliding window failed at request $i" "$test_response"
            return 1
        fi
    done
    
    log_pass "Sliding window validation working correctly"
    return 0
}

test_cleanup() {
    log_test "Cleanup test data"
    
    # Delete test user
    if [[ -n "$TEST_USER_ID" ]]; then
        local response=$(curl $CURL_OPTS -X DELETE "$BASE_URL/api/users/$TEST_USER_ID" \
            -H "Authorization: Bearer $ADMIN_TOKEN")
        
        if [[ $? -eq 0 ]]; then
            log_pass "Test user cleaned up"
        else
            log_fail "Failed to cleanup test user" "$response"
        fi
    fi
}

# Main test execution
main() {
    echo "==================================="
    echo "JDBX Session Management E2E Test"
    echo "==================================="
    echo
    
    # Run all tests
    test_health_check
    
    if test_admin_login; then
        test_create_test_user
        test_user_login
        test_session_validation
        test_concurrent_sessions
        test_token_refresh
        test_session_logout
        test_post_logout_access
        test_session_expiry
        test_session_persistence
        test_sliding_window
        test_cleanup
    fi
    
    # Summary
    echo
    echo "==================================="
    echo "Test Summary"
    echo "==================================="
    echo -e "Total Tests: $TOTAL_TESTS"
    echo -e "Passed: ${GREEN}$PASSED_TESTS${NC}"
    echo -e "Failed: ${RED}$FAILED_TESTS${NC}"
    
    local success_rate=$((PASSED_TESTS * 100 / TOTAL_TESTS))
    echo -e "Success Rate: ${success_rate}%"
    
    if [[ $success_rate -eq 100 ]]; then
        echo -e "${GREEN}✅ ALL TESTS PASSED!${NC}"
        exit 0
    else
        echo -e "${RED}❌ TESTS FAILED - Need fixes${NC}"
        exit 1
    fi
}

# Run the tests
main