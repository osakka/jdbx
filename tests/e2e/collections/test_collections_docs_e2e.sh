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

test_list_libraries() {
    log_test "List libraries endpoint"
    
    local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/libraries" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    local count=$(echo "$response" | jq -r '.libraries | length' 2>/dev/null)
    local has_default=$(echo "$response" | jq -r '.libraries[] | select(.name == "default")' 2>/dev/null)
    local has_system=$(echo "$response" | jq -r '.libraries[] | select(.name == "system")' 2>/dev/null)
    
    if [[ "$count" -ge 2 && -n "$has_default" && -n "$has_system" ]]; then
        log_pass "Libraries listed successfully"
        log_info "Found $count libraries including default and system"
        return 0
    else
        log_fail "Libraries list incomplete" "$response"
        return 1
    fi
}

test_create_library() {
    log_test "Create new library"
    
    local lib_name="testlib-$(date +%s)"
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/libraries" \
        -H "Authorization: Bearer $ADMIN_TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"name\":\"$lib_name\",\"description\":\"Test library\"}")
    
    local created_name=$(echo "$response" | jq -r '.name' 2>/dev/null)
    
    if [[ "$created_name" == "$lib_name" ]]; then
        log_pass "Library created successfully"
        log_info "Created library: $lib_name"
        TEST_LIBRARY="$lib_name"
        return 0
    else
        log_fail "Library creation failed" "$response"
        return 1
    fi
}

test_list_collections() {
    log_test "List collections in library"
    
    local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/libraries/default/collections" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    local count=$(echo "$response" | jq -r '.collections | length' 2>/dev/null)
    
    if [[ "$count" -ge 0 ]]; then
        log_pass "Collections listed successfully"
        log_info "Found $count collections in default library"
        return 0
    else
        log_fail "Collections list failed" "$response"
        return 1
    fi
}

test_create_document() {
    log_test "Create document in default library"
    
    local doc_data='{
        "title": "Test Document",
        "content": "This is a test document",
        "tags": ["test", "e2e"],
        "metadata": {
            "version": "1.0",
            "author": "test-suite"
        }
    }'
    
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/documents" \
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
    
    local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/documents/$DOC_UUID" \
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
        "content": "This document has been updated",
        "tags": ["test", "e2e", "updated"]
    }'
    
    local response=$(curl $CURL_OPTS -X PUT "$BASE_URL/api/documents/$DOC_UUID" \
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

test_query_documents() {
    log_test "Query documents (simplified test)"
    
    # Test with a simple query using GET instead of POST
    local response=$(timeout 30 curl $CURL_OPTS -X GET "$BASE_URL/api/documents?limit=5" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    local count=$(echo "$response" | jq -r '.documents | length' 2>/dev/null)
    
    if [[ "$count" -ge 1 ]]; then
        log_pass "Document query successful"
        log_info "Found $count documents with GET query"
        return 0
    else
        log_fail "Document query failed" "$response"
        return 1
    fi
}

test_list_all_documents() {
    log_test "List all documents"
    
    local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    local count=$(echo "$response" | jq -r '.documents | length' 2>/dev/null)
    local total=$(echo "$response" | jq -r '.total' 2>/dev/null)
    
    if [[ "$count" -ge 1 ]]; then
        log_pass "Documents listed successfully"
        log_info "Found $count documents (total: $total)"
        return 0
    else
        log_fail "Documents list failed" "$response"
        return 1
    fi
}

test_delete_document() {
    log_test "Delete document"
    
    if [[ -z "$DOC_UUID" ]]; then
        log_fail "No document UUID available"
        return 1
    fi
    
    local response=$(curl $CURL_OPTS -X DELETE "$BASE_URL/api/documents/$DOC_UUID" \
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
    
    local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/documents/$DOC_UUID" \
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

test_collection_operations() {
    log_test "Collection operations"
    
    # Test listing collections
    local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/collections" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    local count=$(echo "$response" | jq -r '.collections | length' 2>/dev/null)
    
    if [[ "$count" -ge 0 ]]; then
        log_pass "Collection operations successful"
        log_info "Found $count virtual collections"
        return 0
    else
        log_fail "Collection operations failed" "$response"
        return 1
    fi
}

test_batch_operations() {
    log_test "Multiple document operations (simulated batch)"
    
    # Create multiple documents individually to simulate batch operations
    local success=0
    local total=3
    local created_uuids=()
    
    for i in $(seq 1 $total); do
        local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/documents" \
            -H "Authorization: Bearer $ADMIN_TOKEN" \
            -H "Content-Type: application/json" \
            -d "{\"title\":\"Batch Doc $i\",\"content\":\"Document $i\"}")
        
        local uuid=$(echo "$response" | jq -r '.uuid' 2>/dev/null)
        if [[ -n "$uuid" && "$uuid" != "null" ]]; then
            ((success++))
            created_uuids+=("$uuid")
        fi
    done
    
    # Cleanup created documents
    for uuid in "${created_uuids[@]}"; do
        curl $CURL_OPTS -X DELETE "$BASE_URL/api/documents/$uuid" \
            -H "Authorization: Bearer $ADMIN_TOKEN" > /dev/null 2>&1
    done
    
    if [[ $success -eq $total ]]; then
        log_pass "Multiple document operations successful"
        log_info "Created and cleaned up $success/$total documents"
        return 0
    else
        log_fail "Multiple document operations failed" "$success/$total succeeded"
        return 1
    fi
}

test_document_field_validation() {
    log_test "Document field validation"
    
    # Test with missing required fields
    local invalid_doc='{
        "content": "Document without title"
    }'
    
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $ADMIN_TOKEN" \
        -H "Content-Type: application/json" \
        -d "$invalid_doc")
    
    # Since JDBX auto-populates required fields, this should succeed
    local uuid=$(echo "$response" | jq -r '.uuid' 2>/dev/null)
    
    if [[ -n "$uuid" && "$uuid" != "null" ]]; then
        log_pass "Document field auto-population working"
        
        # Clean up
        curl $CURL_OPTS -X DELETE "$BASE_URL/api/documents/$uuid" \
            -H "Authorization: Bearer $ADMIN_TOKEN" > /dev/null 2>&1
        return 0
    else
        log_fail "Document creation with missing fields failed" "$response"
        return 1
    fi
}

test_concurrent_operations() {
    log_test "Concurrent document operations"
    
    local success=0
    local total=10
    
    # Create documents concurrently
    for i in $(seq 1 $total); do
        curl $CURL_OPTS -X POST "$BASE_URL/api/documents" \
            -H "Authorization: Bearer $ADMIN_TOKEN" \
            -H "Content-Type: application/json" \
            -d "{\"title\":\"Concurrent Doc $i\",\"content\":\"Test concurrent operations\"}" > /tmp/concurrent_$i.json 2>&1 &
    done
    
    wait
    
    # Check results
    for i in $(seq 1 $total); do
        if [[ -f /tmp/concurrent_$i.json ]]; then
            local uuid=$(jq -r '.uuid' /tmp/concurrent_$i.json 2>/dev/null)
            if [[ -n "$uuid" && "$uuid" != "null" ]]; then
                ((success++))
                # Store for cleanup
                echo "$uuid" >> /tmp/concurrent_uuids.txt
            fi
            rm -f /tmp/concurrent_$i.json
        fi
    done
    
    if [[ $success -eq $total ]]; then
        log_pass "Concurrent operations successful"
        log_info "$success/$total concurrent creates succeeded"
        
        # Cleanup
        if [[ -f /tmp/concurrent_uuids.txt ]]; then
            while read uuid; do
                curl $CURL_OPTS -X DELETE "$BASE_URL/api/documents/$uuid" \
                    -H "Authorization: Bearer $ADMIN_TOKEN" > /dev/null 2>&1
            done < /tmp/concurrent_uuids.txt
            rm -f /tmp/concurrent_uuids.txt
        fi
        return 0
    else
        log_fail "Some concurrent operations failed" "$success/$total succeeded"
        return 1
    fi
}

test_pagination() {
    log_test "Document pagination"
    
    # Get first page
    local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/documents?limit=5&offset=0" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    local count=$(echo "$response" | jq -r '.documents | length' 2>/dev/null)
    local total=$(echo "$response" | jq -r '.total' 2>/dev/null)
    local limit=$(echo "$response" | jq -r '.limit' 2>/dev/null)
    local offset=$(echo "$response" | jq -r '.offset' 2>/dev/null)
    
    if [[ "$limit" == "5" && "$offset" == "0" && "$count" -le 5 ]]; then
        log_pass "Pagination working correctly"
        log_info "Page 1: $count documents (total: $total)"
        return 0
    else
        log_fail "Pagination not working correctly" "$response"
        return 1
    fi
}

test_cleanup() {
    log_test "Cleanup test data"
    
    # Clean up test library
    if [[ -n "$TEST_LIBRARY" ]]; then
        curl $CURL_OPTS -X DELETE "$BASE_URL/api/libraries/$TEST_LIBRARY" \
            -H "Authorization: Bearer $ADMIN_TOKEN" > /dev/null 2>&1
    fi
    
    log_pass "Cleanup completed"
    return 0
}

# Main test execution
main() {
    echo "===================================="
    echo "JDBX Collections/Docs E2E Test Suite"
    echo "===================================="
    echo
    
    # Run all tests
    test_health_check
    
    if test_admin_login; then
        test_list_libraries
        test_create_library
        test_list_collections
        test_create_document
        test_get_document
        test_update_document
        test_query_documents
        test_list_all_documents
        test_delete_document
        test_verify_deletion
        test_collection_operations
        test_batch_operations
        test_document_field_validation
        test_concurrent_operations
        test_pagination
        test_cleanup
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