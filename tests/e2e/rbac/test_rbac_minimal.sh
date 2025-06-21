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

test_list_users() {
    log_test "List users"
    
    local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/users" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    local count=$(echo "$response" | jq -r '.users | length' 2>/dev/null)
    local has_admin=$(echo "$response" | jq -r '.users[] | select(.username == "admin")' 2>/dev/null)
    
    if [[ "$count" -ge 1 && -n "$has_admin" ]]; then
        log_pass "Users listed successfully"
        log_info "Found $count users"
        return 0
    else
        log_fail "Users list failed" "$response"
        return 1
    fi
}

test_create_user() {
    log_test "Create new user"
    
    local username="testuser-$(date +%s)"
    local user_data="{
        \"username\": \"$username\",
        \"password\": \"testpass123\"
    }"
    
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/users" \
        -H "Authorization: Bearer $ADMIN_TOKEN" \
        -H "Content-Type: application/json" \
        -d "$user_data")
    
    # Check if response has user wrapper
    local user_obj=$(echo "$response" | jq -r '.user' 2>/dev/null)
    if [[ "$user_obj" != "null" ]]; then
        local created_username=$(echo "$response" | jq -r '.user.username' 2>/dev/null)
        TEST_USER_UUID=$(echo "$response" | jq -r '.user.id' 2>/dev/null)
    else
        local created_username=$(echo "$response" | jq -r '.username' 2>/dev/null)
        TEST_USER_UUID=$(echo "$response" | jq -r '.id // .uuid' 2>/dev/null)
    fi
    TEST_USERNAME="$username"
    
    if [[ "$created_username" == "$username" && -n "$TEST_USER_UUID" && "$TEST_USER_UUID" != "null" ]]; then
        log_pass "User created successfully"
        log_info "Created user: $username"
        return 0
    else
        log_fail "User creation failed" "$response"
        return 1
    fi
}

test_user_can_login() {
    log_test "Test user can login"
    
    if [[ -z "$TEST_USERNAME" ]]; then
        log_fail "No test user available"
        return 1
    fi
    
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/auth/login" \
        -H "Content-Type: application/json" \
        -d "{\"username\":\"$TEST_USERNAME\",\"password\":\"testpass123\"}")
    
    USER_TOKEN=$(echo "$response" | jq -r '.token' 2>/dev/null)
    
    if [[ -n "$USER_TOKEN" && "$USER_TOKEN" != "null" ]]; then
        log_pass "Test user login successful"
        return 0
    else
        log_fail "Test user login failed" "$response"
        return 1
    fi
}

test_list_roles() {
    log_test "List roles"
    
    local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/roles" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    local count=$(echo "$response" | jq -r '.roles | length' 2>/dev/null)
    local has_admin=$(echo "$response" | jq -r '.roles[] | select(.name == "admin")' 2>/dev/null)
    
    if [[ "$count" -ge 1 && -n "$has_admin" ]]; then
        log_pass "Roles listed successfully"
        log_info "Found $count roles"
        return 0
    else
        log_fail "Roles list failed" "$response"
        return 1
    fi
}

test_create_role() {
    log_test "Create new role"
    
    local role_name="testrole-$(date +%s)"
    local role_data="{
        \"name\": \"$role_name\",
        \"description\": \"Test role\"
    }"
    
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/roles" \
        -H "Authorization: Bearer $ADMIN_TOKEN" \
        -H "Content-Type: application/json" \
        -d "$role_data")
    
    # Check if response has role wrapper
    local role_obj=$(echo "$response" | jq -r '.role' 2>/dev/null)
    if [[ "$role_obj" != "null" ]]; then
        local created_name=$(echo "$response" | jq -r '.role.name' 2>/dev/null)
    else
        local created_name=$(echo "$response" | jq -r '.name' 2>/dev/null)
    fi
    
    if [[ "$created_name" == "$role_name" ]]; then
        log_pass "Role created successfully"
        log_info "Created role: $role_name"
        return 0
    else
        log_fail "Role creation failed" "$response"
        return 1
    fi
}

test_user_permissions() {
    log_test "Default user permissions"
    
    if [[ -z "$USER_TOKEN" ]]; then
        log_fail "No user token available"
        return 1
    fi
    
    # Try to create a document with the user's token
    local doc_data='{"title": "User test doc", "content": "Testing default permissions"}'
    
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $USER_TOKEN" \
        -H "Content-Type: application/json" \
        -d "$doc_data")
    
    local doc_uuid=$(echo "$response" | jq -r '.uuid' 2>/dev/null)
    
    if [[ -n "$doc_uuid" && "$doc_uuid" != "null" ]]; then
        log_pass "Users can create documents by default"
        
        # Clean up
        curl $CURL_OPTS -X DELETE "$BASE_URL/api/documents/$doc_uuid" \
            -H "Authorization: Bearer $USER_TOKEN" > /dev/null 2>&1
        return 0
    else
        log_fail "User cannot create documents" "$response"
        return 1
    fi
}

# Main test execution
main() {
    echo "================================="
    echo "JDBX RBAC Minimal E2E Test Suite"
    echo "================================="
    echo
    
    # Run core tests
    test_health_check
    
    if test_admin_login; then
        test_list_users
        test_create_user
        test_user_can_login
        test_list_roles
        test_create_role
        test_user_permissions
    fi
    
    # Summary
    echo
    echo "================================="
    echo "Test Summary"
    echo "================================="
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