#!/bin/bash

# JDBX Comprehensive End-User Functionality Test Suite
# Testing all features that tenants rely on for 100% satisfaction

set -e

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Test configuration
HOST="https://localhost:5000"
ADMIN_USER="admin"
ADMIN_PASS="admin"
TEST_LIBRARY="tenant1"
TEST_USER="testuser"
TEST_PASS="testpass123"
TEST_COLLECTION="products"

# Test counters
TESTS_PASSED=0
TESTS_FAILED=0

# Helper functions
pass() {
    echo -e "${GREEN}✅ PASS${NC}: $1"
    ((TESTS_PASSED++))
}

fail() {
    echo -e "${RED}❌ FAIL${NC}: $1"
    echo "Response: $2"
    ((TESTS_FAILED++))
}

test_endpoint() {
    local test_name="$1"
    local method="$2"
    local endpoint="$3"
    local data="$4"
    local expected="$5"
    
    echo -e "\n${YELLOW}Testing: $test_name${NC}"
    
    if [ -z "$data" ]; then
        RESPONSE=$(curl -sk -X "$method" "$HOST$endpoint" \
            -H "Authorization: Bearer $TOKEN" \
            -H "Content-Type: application/json" \
            -w "\nHTTP_STATUS:%{http_code}")
    else
        RESPONSE=$(curl -sk -X "$method" "$HOST$endpoint" \
            -H "Authorization: Bearer $TOKEN" \
            -H "Content-Type: application/json" \
            -d "$data" \
            -w "\nHTTP_STATUS:%{http_code}")
    fi
    
    HTTP_STATUS=$(echo "$RESPONSE" | grep "HTTP_STATUS:" | cut -d: -f2)
    BODY=$(echo "$RESPONSE" | sed '/HTTP_STATUS:/d')
    
    if [[ "$HTTP_STATUS" =~ ^2[0-9][0-9]$ ]]; then
        if [ -n "$expected" ] && ! echo "$BODY" | grep -q "$expected"; then
            fail "$test_name - Expected '$expected' not found" "$BODY"
        else
            pass "$test_name (HTTP $HTTP_STATUS)"
        fi
    else
        fail "$test_name (HTTP $HTTP_STATUS)" "$BODY"
    fi
    
    echo "$BODY" | jq . 2>/dev/null || echo "$BODY"
}

echo "======================================"
echo "JDBX End-User Functionality Test Suite"
echo "======================================"

# 1. AUTHENTICATION & SESSION MANAGEMENT
echo -e "\n${YELLOW}=== AUTHENTICATION & SESSION MANAGEMENT ===${NC}"

# Login as admin
echo "Logging in as admin..."
LOGIN_RESPONSE=$(curl -sk -X POST "$HOST/api/auth/login" \
    -H "Content-Type: application/json" \
    -d "{\"username\": \"$ADMIN_USER\", \"password\": \"$ADMIN_PASS\"}")

TOKEN=$(echo "$LOGIN_RESPONSE" | jq -r '.token')
REFRESH_TOKEN=$(echo "$LOGIN_RESPONSE" | jq -r '.refresh_token')

if [ "$TOKEN" != "null" ] && [ -n "$TOKEN" ]; then
    pass "Admin login"
    echo "Token: ${TOKEN:0:20}..."
else
    fail "Admin login" "$LOGIN_RESPONSE"
    echo "Cannot continue without authentication"
    exit 1
fi

# Test session info
test_endpoint "Get session info" "GET" "/api/session" "" "username"

# Test token refresh
test_endpoint "Refresh token" "POST" "/api/auth/refresh" "{\"refresh_token\": \"$REFRESH_TOKEN\"}" "token"

# 2. MULTI-TENANCY (LIBRARIES)
echo -e "\n${YELLOW}=== MULTI-TENANCY (LIBRARIES) ===${NC}"

# List libraries
test_endpoint "List libraries" "GET" "/api/libraries" "" "libraries"

# Create new library for tenant
test_endpoint "Create tenant library" "POST" "/api/libraries" \
    "{\"name\": \"$TEST_LIBRARY\", \"display_name\": \"Tenant 1\", \"description\": \"Test tenant library\"}" \
    "$TEST_LIBRARY"

# Switch to tenant library
test_endpoint "Switch to tenant library" "POST" "/api/session/library" \
    "{\"library\": \"$TEST_LIBRARY\"}" \
    "$TEST_LIBRARY"

# 3. USER MANAGEMENT
echo -e "\n${YELLOW}=== USER MANAGEMENT ===${NC}"

# Create tenant user
test_endpoint "Create tenant user" "POST" "/api/users" \
    "{\"username\": \"$TEST_USER\", \"password\": \"$TEST_PASS\", \"email\": \"test@tenant1.com\", \"roles\": [\"user\"]}" \
    "$TEST_USER"

# List users in tenant
test_endpoint "List tenant users" "GET" "/api/users" "" "users"

# Get specific user
test_endpoint "Get user details" "GET" "/api/users/$TEST_USER" "" "$TEST_USER"

# Update user
test_endpoint "Update user email" "PUT" "/api/users/$TEST_USER" \
    "{\"email\": \"updated@tenant1.com\"}" \
    "updated@tenant1.com"

# 4. ROLE-BASED ACCESS CONTROL
echo -e "\n${YELLOW}=== ROLE-BASED ACCESS CONTROL ===${NC}"

# List roles
test_endpoint "List roles" "GET" "/api/roles" "" "roles"

# Create custom role
test_endpoint "Create editor role" "POST" "/api/roles" \
    "{\"name\": \"editor\", \"display_name\": \"Content Editor\", \"permissions\": {\"collections.read\": true, \"collections.write\": true, \"collections.delete\": false}}" \
    "editor"

# Assign role to user
test_endpoint "Assign editor role" "PUT" "/api/users/$TEST_USER" \
    "{\"roles\": [\"user\", \"editor\"]}" \
    "editor"

# 5. COLLECTIONS & DOCUMENTS
echo -e "\n${YELLOW}=== COLLECTIONS & DOCUMENTS ===${NC}"

# List collections
test_endpoint "List collections" "GET" "/api/collections" "" "collections"

# Create collection
test_endpoint "Create products collection" "POST" "/api/collections" \
    "{\"name\": \"$TEST_COLLECTION\", \"display_name\": \"Products\", \"description\": \"Product catalog\"}" \
    "$TEST_COLLECTION"

# Insert document
test_endpoint "Insert product" "POST" "/api/collections/$TEST_COLLECTION/documents" \
    "{\"name\": \"Widget\", \"price\": 19.99, \"sku\": \"WDG-001\", \"in_stock\": true}" \
    "Widget"

# Query documents
test_endpoint "Query products" "GET" "/api/collections/$TEST_COLLECTION/documents" "" "Widget"

# Query with filter
test_endpoint "Query in-stock products" "GET" "/api/collections/$TEST_COLLECTION/documents?query={\"in_stock\":true}" "" "Widget"

# Get document by ID (we'll need to extract ID from previous response)
PRODUCT_ID=$(curl -sk -X GET "$HOST/api/collections/$TEST_COLLECTION/documents" \
    -H "Authorization: Bearer $TOKEN" | jq -r '.documents[0].uuid')

if [ "$PRODUCT_ID" != "null" ] && [ -n "$PRODUCT_ID" ]; then
    test_endpoint "Get product by ID" "GET" "/api/collections/$TEST_COLLECTION/documents/$PRODUCT_ID" "" "Widget"
    
    # Update document
    test_endpoint "Update product price" "PUT" "/api/collections/$TEST_COLLECTION/documents/$PRODUCT_ID" \
        "{\"price\": 24.99}" \
        "24.99"
    
    # Field-level operations
    test_endpoint "Update single field" "PUT" "/api/collections/$TEST_COLLECTION/documents/$PRODUCT_ID/fields/price" \
        "29.99" \
        ""
fi

# 6. JAVASCRIPT FUNCTIONS
echo -e "\n${YELLOW}=== JAVASCRIPT FUNCTIONS ===${NC}"

# List JavaScript functions
test_endpoint "List JS functions" "GET" "/api/js/functions" "" "functions"

# Create validator function
test_endpoint "Create price validator" "POST" "/api/js/validators" \
    "{\"name\": \"validatePrice\", \"description\": \"Validate product price\", \"code\": \"function validate(doc) { if (doc.price && doc.price < 0) { addError('price', 'Price must be positive'); } return isValid; }\"}" \
    "validatePrice"

# Execute custom function
test_endpoint "Execute validator" "POST" "/api/js/functions/validatePrice" \
    "{\"doc\": {\"price\": -10}}" \
    ""

# 7. METRICS & MONITORING
echo -e "\n${YELLOW}=== METRICS & MONITORING ===${NC}"

# Get metrics
test_endpoint "Get system metrics" "GET" "/api/metrics" "" "operations"

# Get detailed stats
test_endpoint "Get collection stats" "GET" "/api/visualization/collection-stats?collection=$TEST_COLLECTION" "" "stats"

# 8. SEARCH & INDEXING
echo -e "\n${YELLOW}=== SEARCH & INDEXING ===${NC}"

# Create index
test_endpoint "Create price index" "POST" "/api/indexes/$TEST_COLLECTION" \
    "{\"field\": \"price\", \"type\": \"btree\", \"name\": \"price_idx\"}" \
    "price_idx"

# List indexes
test_endpoint "List collection indexes" "GET" "/api/indexes/$TEST_COLLECTION" "" "indexes"

# 9. BATCH OPERATIONS
echo -e "\n${YELLOW}=== BATCH OPERATIONS ===${NC}"

# Batch insert
test_endpoint "Batch insert products" "POST" "/api/batch/$TEST_COLLECTION/documents" \
    "{\"documents\": [{\"name\": \"Gadget\", \"price\": 39.99, \"sku\": \"GDG-001\"}, {\"name\": \"Doohickey\", \"price\": 14.99, \"sku\": \"DHK-001\"}]}" \
    "inserted"

# Batch query
test_endpoint "Batch query by SKU" "POST" "/api/batch/$TEST_COLLECTION/query" \
    "{\"queries\": [{\"sku\": \"WDG-001\"}, {\"sku\": \"GDG-001\"}]}" \
    ""

# 10. CONFIGURATION & SETTINGS
echo -e "\n${YELLOW}=== CONFIGURATION & SETTINGS ===${NC}"

# Get configuration
test_endpoint "Get system config" "GET" "/api/config" "" "config"

# Update configuration (admin only)
test_endpoint "Update log level" "PUT" "/api/config" \
    "{\"log_level\": \"debug\"}" \
    ""

# 11. DATA EXPORT/IMPORT
echo -e "\n${YELLOW}=== DATA EXPORT/IMPORT ===${NC}"

# Export collection
test_endpoint "Export products" "GET" "/api/collections/$TEST_COLLECTION/export" "" ""

# Get backup status
test_endpoint "Backup status" "GET" "/api/backup/status" "" ""

# 12. CLEANUP (Optional)
echo -e "\n${YELLOW}=== CLEANUP ===${NC}"

# Delete test document
if [ "$PRODUCT_ID" != "null" ] && [ -n "$PRODUCT_ID" ]; then
    test_endpoint "Delete test product" "DELETE" "/api/collections/$TEST_COLLECTION/documents/$PRODUCT_ID" "" ""
fi

# Delete test user
test_endpoint "Delete test user" "DELETE" "/api/users/$TEST_USER" "" ""

# Delete test role
test_endpoint "Delete test role" "DELETE" "/api/roles/editor" "" ""

# Delete test collection
test_endpoint "Delete test collection" "DELETE" "/api/collections/$TEST_COLLECTION" "" ""

# FINAL REPORT
echo -e "\n======================================"
echo "TEST RESULTS SUMMARY"
echo "======================================"
echo -e "${GREEN}Passed: $TESTS_PASSED${NC}"
echo -e "${RED}Failed: $TESTS_FAILED${NC}"
echo -e "Total: $((TESTS_PASSED + TESTS_FAILED))"
echo "======================================"

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}🎉 ALL TESTS PASSED! Tenants will be 100% satisfied!${NC}"
    exit 0
else
    echo -e "${RED}⚠️  Some tests failed. Need fixes for tenant satisfaction.${NC}"
    exit 1
fi