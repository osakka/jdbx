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
        log_info "Found $count users including admin"
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
        \"password\": \"testpass123\",
        \"email\": \"$username@test.com\"
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
        log_info "Created user: $username (UUID: $TEST_USER_UUID)"
        return 0
    else
        log_fail "User creation failed" "$response"
        return 1
    fi
}

test_user_login() {
    log_test "Test user login"
    
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
        log_info "Found $count roles including admin"
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
        \"description\": \"Test role for E2E testing\",
        \"permissions\": [
            \"documents:read\",
            \"documents:create\",
            \"documents:update\"
        ]
    }"
    
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/roles" \
        -H "Authorization: Bearer $ADMIN_TOKEN" \
        -H "Content-Type: application/json" \
        -d "$role_data")
    
    # Check if response has role wrapper
    local role_obj=$(echo "$response" | jq -r '.role' 2>/dev/null)
    if [[ "$role_obj" != "null" ]]; then
        local created_name=$(echo "$response" | jq -r '.role.name' 2>/dev/null)
        TEST_ROLE_UUID=$(echo "$response" | jq -r '.role.id' 2>/dev/null)
    else
        local created_name=$(echo "$response" | jq -r '.name' 2>/dev/null)
        TEST_ROLE_UUID=$(echo "$response" | jq -r '.id // .uuid' 2>/dev/null)
    fi
    TEST_ROLE_NAME="$role_name"
    
    if [[ "$created_name" == "$role_name" && -n "$TEST_ROLE_UUID" && "$TEST_ROLE_UUID" != "null" ]]; then
        log_pass "Role created successfully"
        log_info "Created role: $role_name (UUID: $TEST_ROLE_UUID)"
        return 0
    else
        log_fail "Role creation failed" "$response"
        return 1
    fi
}

test_assign_role() {
    log_test "Assign role to user"
    
    if [[ -z "$TEST_USER_UUID" || -z "$TEST_ROLE_NAME" ]]; then
        log_fail "No test user or role available"
        return 1
    fi
    
    local assignment_data="{
        \"user_id\": \"$TEST_USER_UUID\",
        \"role_name\": \"$TEST_ROLE_NAME\"
    }"
    
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/rbac/assign-role" \
        -H "Authorization: Bearer $ADMIN_TOKEN" \
        -H "Content-Type: application/json" \
        -d "$assignment_data")
    
    local success=$(echo "$response" | jq -r '.success' 2>/dev/null)
    
    if [[ "$success" == "true" ]]; then
        log_pass "Role assigned successfully"
        return 0
    else
        log_fail "Role assignment failed" "$response"
        return 1
    fi
}

test_check_permission() {
    log_test "Check user permission"
    
    if [[ -z "$USER_TOKEN" ]]; then
        log_fail "No user token available"
        return 1
    fi
    
    # Try to create a document with the user's token
    local doc_data='{"title": "Permission test doc", "content": "Testing permissions"}'
    
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $USER_TOKEN" \
        -H "Content-Type: application/json" \
        -d "$doc_data")
    
    local doc_uuid=$(echo "$response" | jq -r '.uuid' 2>/dev/null)
    
    if [[ -n "$doc_uuid" && "$doc_uuid" != "null" ]]; then
        log_pass "User permission check successful - can create documents"
        
        # Clean up
        curl $CURL_OPTS -X DELETE "$BASE_URL/api/documents/$doc_uuid" \
            -H "Authorization: Bearer $USER_TOKEN" > /dev/null 2>&1
        return 0
    else
        log_fail "User permission check failed" "$response"
        return 1
    fi
}

test_update_role() {
    log_test "Update role permissions"
    
    if [[ -z "$TEST_ROLE_UUID" ]]; then
        log_fail "No test role available"
        return 1
    fi
    
    local update_data="{
        \"description\": \"Updated test role\",
        \"permissions\": [
            \"documents:read\",
            \"documents:create\",
            \"documents:update\",
            \"documents:delete\"
        ]
    }"
    
    local response=$(curl $CURL_OPTS -X PUT "$BASE_URL/api/roles/$TEST_ROLE_UUID" \
        -H "Authorization: Bearer $ADMIN_TOKEN" \
        -H "Content-Type: application/json" \
        -d "$update_data")
    
    local description=$(echo "$response" | jq -r '.description' 2>/dev/null)
    local perms_count=$(echo "$response" | jq -r '.permissions | length' 2>/dev/null)
    
    if [[ "$description" == "Updated test role" && "$perms_count" == "4" ]]; then
        log_pass "Role updated successfully"
        return 0
    else
        log_fail "Role update failed" "$response"
        return 1
    fi
}

test_delete_user() {
    log_test "Delete test user"
    
    if [[ -z "$TEST_USER_UUID" ]]; then
        log_fail "No test user available"
        return 1
    fi
    
    local response=$(curl $CURL_OPTS -X DELETE "$BASE_URL/api/users/$TEST_USER_UUID" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    local success=$(echo "$response" | jq -r '.success' 2>/dev/null)
    
    if [[ "$success" == "true" ]]; then
        log_pass "User deleted successfully"
        return 0
    else
        log_fail "User deletion failed" "$response"
        return 1
    fi
}

test_delete_role() {
    log_test "Delete test role"
    
    if [[ -z "$TEST_ROLE_UUID" ]]; then
        log_fail "No test role available"
        return 1
    fi
    
    local response=$(curl $CURL_OPTS -X DELETE "$BASE_URL/api/roles/$TEST_ROLE_UUID" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    local success=$(echo "$response" | jq -r '.success' 2>/dev/null)
    
    if [[ "$success" == "true" ]]; then
        log_pass "Role deleted successfully"
        return 0
    else
        log_fail "Role deletion failed" "$response"
        return 1
    fi
}

test_unauthorized_access() {
    log_test "Unauthorized access protection"
    
    # Try to access users without token
    local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/users")
    local error=$(echo "$response" | jq -r '.error' 2>/dev/null)
    
    if [[ "$error" =~ "Unauthorized" || "$error" =~ "No token" ]]; then
        log_pass "Unauthorized access properly blocked"
        return 0
    else
        log_fail "Unauthorized access not blocked" "$response"
        return 1
    fi
}

# Main test execution
main() {
    echo "============================="
    echo "JDBX RBAC E2E Test Suite"
    echo "============================="
    echo
    
    # Run all tests
    test_health_check
    
    if test_admin_login; then
        test_list_users
        test_create_user
        test_user_login
        test_list_roles
        test_create_role
        test_assign_role
        test_check_permission
        test_update_role
        test_delete_user
        test_delete_role
        test_unauthorized_access
    fi
    
    # Summary
    echo
    echo "============================="
    echo "Test Summary"
    echo "============================="
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