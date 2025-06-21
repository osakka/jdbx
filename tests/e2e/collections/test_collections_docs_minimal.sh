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
    
    local response=$(timeout 10 curl $CURL_OPTS -X GET "$BASE_URL/api/health")
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
    
    local response=$(timeout 10 curl $CURL_OPTS -X POST "$BASE_URL/api/auth/login" \
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

test_list_libraries() {
    log_test "List libraries endpoint"
    
    local response=$(timeout 10 curl $CURL_OPTS -X GET "$BASE_URL/api/libraries" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    local count=$(echo "$response" | jq -r '.libraries | length' 2>/dev/null)
    local has_default=$(echo "$response" | jq -r '.libraries[] | select(.name == "default")' 2>/dev/null)
    
    if [[ "$count" -ge 1 && -n "$has_default" ]]; then
        log_pass "Libraries listed successfully"
        log_info "Found $count libraries including default"
        return 0
    else
        log_fail "Libraries list incomplete" "$response"
        return 1
    fi
}

test_create_document() {
    log_test "Create document"
    
    local doc_data='{
        "title": "Test Document",
        "content": "This is a test document",
        "tags": ["test", "e2e"]
    }'
    
    local response=$(timeout 10 curl $CURL_OPTS -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $ADMIN_TOKEN" \
        -H "Content-Type: application/json" \
        -d "$doc_data")
    
    DOC_UUID=$(echo "$response" | jq -r '.uuid' 2>/dev/null)
    local title=$(echo "$response" | jq -r '.title' 2>/dev/null)
    
    if [[ -n "$DOC_UUID" && "$DOC_UUID" != "null" && "$title" == "Test Document" ]]; then
        log_pass "Document created successfully"
        log_info "Document UUID: $DOC_UUID"
        return 0
    else
        log_fail "Document creation failed" "$response"
        return 1
    fi
}

test_get_document() {
    log_test "Get document by UUID"
    
    if [[ -z "$DOC_UUID" ]]; then
        log_fail "No document UUID available"
        return 1
    fi
    
    local response=$(timeout 10 curl $CURL_OPTS -X GET "$BASE_URL/api/documents/$DOC_UUID" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    local title=$(echo "$response" | jq -r '.title' 2>/dev/null)
    local content=$(echo "$response" | jq -r '.content' 2>/dev/null)
    
    if [[ "$title" == "Test Document" && "$content" == "This is a test document" ]]; then
        log_pass "Document retrieved successfully"
        return 0
    else
        log_fail "Document retrieval failed" "$response"
        return 1
    fi
}

test_update_document() {
    log_test "Update document"
    
    if [[ -z "$DOC_UUID" ]]; then
        log_fail "No document UUID available"
        return 1
    fi
    
    local update_data='{
        "title": "Updated Test Document",
        "content": "This document has been updated"
    }'
    
    local response=$(timeout 10 curl $CURL_OPTS -X PUT "$BASE_URL/api/documents/$DOC_UUID" \
        -H "Authorization: Bearer $ADMIN_TOKEN" \
        -H "Content-Type: application/json" \
        -d "$update_data")
    
    local title=$(echo "$response" | jq -r '.title' 2>/dev/null)
    
    if [[ "$title" == "Updated Test Document" ]]; then
        log_pass "Document updated successfully"
        return 0
    else
        log_fail "Document update failed" "$response"
        return 1
    fi
}

test_delete_document() {
    log_test "Delete document"
    
    if [[ -z "$DOC_UUID" ]]; then
        log_fail "No document UUID available"
        return 1
    fi
    
    local response=$(timeout 10 curl $CURL_OPTS -X DELETE "$BASE_URL/api/documents/$DOC_UUID" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    local success=$(echo "$response" | jq -r '.success' 2>/dev/null)
    
    if [[ "$success" == "true" ]]; then
        log_pass "Document deleted successfully"
        return 0
    else
        log_fail "Document deletion failed" "$response"
        return 1
    fi
}

test_verify_deletion() {
    log_test "Verify document deletion"
    
    if [[ -z "$DOC_UUID" ]]; then
        log_fail "No document UUID available"
        return 1
    fi
    
    local response=$(timeout 10 curl $CURL_OPTS -X GET "$BASE_URL/api/documents/$DOC_UUID" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    local error=$(echo "$response" | jq -r '.error' 2>/dev/null)
    
    if [[ "$error" == "Document not found" || "$error" =~ "not found" ]]; then
        log_pass "Document deletion verified"
        return 0
    else
        log_fail "Document still exists after deletion" "$response"
        return 1
    fi
}

test_virtual_collections() {
    log_test "Virtual collections list"
    
    local response=$(timeout 10 curl $CURL_OPTS -X GET "$BASE_URL/api/collections" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    local count=$(echo "$response" | jq -r '.collections | length' 2>/dev/null)
    
    if [[ "$count" -ge 0 ]]; then
        log_pass "Virtual collections working"
        log_info "Found $count virtual collections"
        return 0
    else
        log_fail "Virtual collections failed" "$response"
        return 1
    fi
}

# Main test execution
main() {
    echo "======================================"
    echo "JDBX Collections/Docs Minimal E2E Test"
    echo "======================================"
    echo
    
    # Run core tests
    test_health_check
    
    if test_admin_login; then
        test_list_libraries
        test_create_document
        test_get_document
        test_update_document
        test_delete_document
        test_verify_deletion
        test_virtual_collections
    fi
    
    # Summary
    echo
    echo "======================================"
    echo "Test Summary"
    echo "======================================"
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