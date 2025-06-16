#!/bin/bash

# JDBX Core Endpoint Test - Focused on tenant satisfaction

set -e

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

HOST="https://localhost:5000"

echo "======================================"
echo "JDBX Core Functionality Test"
echo "======================================"

# 1. Health Check
echo -e "\n${YELLOW}Testing: Health Check${NC}"
HEALTH=$(curl -sk "$HOST/health")
if echo "$HEALTH" | grep -q "status.*ok"; then
    echo -e "${GREEN}✅ PASS: Health check${NC}"
    echo "$HEALTH" | jq . 2>/dev/null || echo "$HEALTH"
else
    echo -e "${RED}❌ FAIL: Health check${NC}"
    echo "$HEALTH"
fi

# 2. Authentication
echo -e "\n${YELLOW}Testing: Authentication${NC}"
LOGIN=$(curl -sk -X POST "$HOST/api/auth/login" \
    -H "Content-Type: application/json" \
    -d '{"username": "admin", "password": "admin"}')

TOKEN=$(echo "$LOGIN" | jq -r '.token' 2>/dev/null)
if [ "$TOKEN" != "null" ] && [ -n "$TOKEN" ]; then
    echo -e "${GREEN}✅ PASS: Authentication${NC}"
    echo "Token obtained: ${TOKEN:0:20}..."
else
    echo -e "${RED}❌ FAIL: Authentication${NC}"
    echo "$LOGIN"
    exit 1
fi

# 3. Libraries (Multi-tenancy)
echo -e "\n${YELLOW}Testing: Multi-tenancy (Libraries)${NC}"
LIBS=$(curl -sk "$HOST/api/libraries" -H "Authorization: Bearer $TOKEN")
if echo "$LIBS" | grep -q "libraries"; then
    echo -e "${GREEN}✅ PASS: List libraries${NC}"
    echo "$LIBS" | jq '.libraries[] | {name, description}' 2>/dev/null
else
    echo -e "${RED}❌ FAIL: List libraries${NC}"
    echo "$LIBS"
fi

# 4. Create Test Library
echo -e "\n${YELLOW}Testing: Create Tenant Library${NC}"
CREATE_LIB=$(curl -sk -X POST "$HOST/api/libraries" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{"name": "tenant_test", "display_name": "Test Tenant", "description": "Testing tenant operations"}')

if echo "$CREATE_LIB" | grep -q "tenant_test"; then
    echo -e "${GREEN}✅ PASS: Create library${NC}"
else
    echo -e "${RED}❌ FAIL: Create library${NC}"
    echo "$CREATE_LIB"
fi

# 5. Collections
echo -e "\n${YELLOW}Testing: Collections${NC}"
COLLECTIONS=$(curl -sk "$HOST/api/collections" -H "Authorization: Bearer $TOKEN")
if echo "$COLLECTIONS" | grep -q "collections"; then
    echo -e "${GREEN}✅ PASS: List collections${NC}"
    echo "$COLLECTIONS" | jq '.collections[] | .name' 2>/dev/null | head -5
else
    echo -e "${RED}❌ FAIL: List collections${NC}"
    echo "$COLLECTIONS"
fi

# 6. Create Collection
echo -e "\n${YELLOW}Testing: Create Collection${NC}"
CREATE_COL=$(curl -sk -X POST "$HOST/api/collections" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{"name": "test_products", "display_name": "Test Products"}')

if echo "$CREATE_COL" | grep -q "test_products" || echo "$CREATE_COL" | grep -q "already exists"; then
    echo -e "${GREEN}✅ PASS: Create collection${NC}"
else
    echo -e "${RED}❌ FAIL: Create collection${NC}"
    echo "$CREATE_COL"
fi

# 7. Document Operations
echo -e "\n${YELLOW}Testing: Document CRUD${NC}"

# Insert
INSERT=$(curl -sk -X POST "$HOST/api/collections/test_products/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{"name": "Test Widget", "price": 19.99, "sku": "TEST-001"}')

DOC_ID=$(echo "$INSERT" | jq -r '.uuid' 2>/dev/null)
if [ "$DOC_ID" != "null" ] && [ -n "$DOC_ID" ]; then
    echo -e "${GREEN}✅ PASS: Insert document${NC}"
    echo "Document ID: $DOC_ID"
else
    echo -e "${RED}❌ FAIL: Insert document${NC}"
    echo "$INSERT"
fi

# Query
QUERY=$(curl -sk "$HOST/api/collections/test_products/documents" \
    -H "Authorization: Bearer $TOKEN")

if echo "$QUERY" | grep -q "Test Widget"; then
    echo -e "${GREEN}✅ PASS: Query documents${NC}"
else
    echo -e "${RED}❌ FAIL: Query documents${NC}"
    echo "$QUERY"
fi

# 8. Users
echo -e "\n${YELLOW}Testing: User Management${NC}"
USERS=$(curl -sk "$HOST/api/users" -H "Authorization: Bearer $TOKEN")
if echo "$USERS" | grep -q "admin"; then
    echo -e "${GREEN}✅ PASS: List users${NC}"
    echo "$USERS" | jq '.users[] | {username, library}' 2>/dev/null
else
    echo -e "${RED}❌ FAIL: List users${NC}"
    echo "$USERS"
fi

# 9. Roles
echo -e "\n${YELLOW}Testing: RBAC${NC}"
ROLES=$(curl -sk "$HOST/api/roles" -H "Authorization: Bearer $TOKEN")
if echo "$ROLES" | grep -q "admin"; then
    echo -e "${GREEN}✅ PASS: List roles${NC}"
    echo "$ROLES" | jq '.roles[] | .name' 2>/dev/null
else
    echo -e "${RED}❌ FAIL: List roles${NC}"
    echo "$ROLES"
fi

# 10. Metrics
echo -e "\n${YELLOW}Testing: Metrics${NC}"
METRICS=$(curl -sk "$HOST/api/metrics" -H "Authorization: Bearer $TOKEN")
if echo "$METRICS" | grep -q "operations"; then
    echo -e "${GREEN}✅ PASS: Get metrics${NC}"
    echo "$METRICS" | jq '.operations' 2>/dev/null
else
    echo -e "${RED}❌ FAIL: Get metrics${NC}"
    echo "$METRICS"
fi

echo -e "\n${GREEN}======================================"
echo "Core functionality test complete!"
echo "======================================${NC}"