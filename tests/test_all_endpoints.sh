#!/bin/bash

# JSONdb API Endpoint Test Script
# Tests all registered API endpoints with appropriate timeouts

# Configuration
BASE_URL="http://localhost:5000"
TIMEOUT=5
TOKEN=""

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to test an endpoint
test_endpoint() {
    local method=$1
    local endpoint=$2
    local description=$3
    local data=$4
    local auth=$5
    
    printf "%-8s %-50s" "$method" "$endpoint"
    
    # Build curl command
    local curl_cmd="curl -s -w '\n%{http_code}' -X $method --connect-timeout $TIMEOUT --max-time $TIMEOUT"
    
    # Add auth header if needed
    if [[ "$auth" == "true" && -n "$TOKEN" ]]; then
        curl_cmd="$curl_cmd -H 'Authorization: Bearer $TOKEN'"
    fi
    
    # Add data if provided
    if [[ -n "$data" ]]; then
        curl_cmd="$curl_cmd -H 'Content-Type: application/json' -d '$data'"
    fi
    
    # Execute request
    local response=$(eval "$curl_cmd '$BASE_URL$endpoint' 2>&1")
    local exit_code=$?
    
    if [[ $exit_code -eq 28 ]]; then
        echo -e "${RED}TIMEOUT${NC} - Operation timed out after $TIMEOUT seconds"
    elif [[ $exit_code -ne 0 ]]; then
        echo -e "${RED}ERROR${NC} - curl exit code: $exit_code"
    else
        # Extract HTTP status code (last line)
        local http_code=$(echo "$response" | tail -n1)
        local body=$(echo "$response" | head -n-1)
        
        if [[ "$http_code" =~ ^[0-9]+$ ]]; then
            if [[ $http_code -ge 200 && $http_code -lt 300 ]]; then
                echo -e "${GREEN}OK${NC} ($http_code)"
                if [[ "$endpoint" == "/api/auth/login" && $http_code -eq 200 ]]; then
                    # Extract token from login response
                    TOKEN=$(echo "$body" | grep -o '"token":"[^"]*' | cut -d'"' -f4)
                    if [[ -n "$TOKEN" ]]; then
                        echo "    Token obtained: ${TOKEN:0:20}..."
                    fi
                fi
            elif [[ $http_code -ge 400 && $http_code -lt 500 ]]; then
                echo -e "${YELLOW}CLIENT ERROR${NC} ($http_code)"
            else
                echo -e "${RED}SERVER ERROR${NC} ($http_code)"
            fi
        else
            echo -e "${RED}INVALID RESPONSE${NC}"
        fi
    fi
}

echo "JSONdb API Endpoint Testing"
echo "==========================="
echo "Server: $BASE_URL"
echo "Timeout: $TIMEOUT seconds"
echo ""

# Test health endpoints first (no auth required)
echo "Health & System Endpoints (No Auth):"
echo "------------------------------------"
test_endpoint "GET" "/api/health" "Health check" "" "false"
test_endpoint "GET" "/api/system/info" "System information" "" "false"
test_endpoint "GET" "/api/openapi.json" "OpenAPI specification" "" "false"

echo ""
echo "Authentication Endpoints:"
echo "------------------------"
# Try login first to get token
test_endpoint "POST" "/api/auth/login" "Login" '{"username":"admin","password":"admin123"}' "false"
test_endpoint "POST" "/api/auth/register" "Register new user" '{"username":"testuser","password":"test123","email":"test@example.com"}' "false"
test_endpoint "POST" "/api/login" "Legacy login endpoint" '{"username":"admin","password":"admin123"}' "false"

# If we don't have a token yet, try the legacy admin login
if [[ -z "$TOKEN" ]]; then
    test_endpoint "POST" "/api/admin/login" "Admin login" '{"username":"admin","password":"admin"}' "false"
fi

echo ""
echo "Session Management (Auth Required):"
echo "----------------------------------"
test_endpoint "GET" "/api/sessions" "List sessions" "" "true"
test_endpoint "GET" "/api/sessions/active" "Active sessions" "" "true"
test_endpoint "POST" "/api/auth/refresh" "Refresh token" "" "true"
test_endpoint "POST" "/api/auth/logout" "Logout" "" "true"

echo ""
echo "Library Management:"
echo "------------------"
test_endpoint "GET" "/api/libraries" "List libraries" "" "true"
test_endpoint "POST" "/api/libraries" "Create library" '{"name":"test_library","description":"Test library"}' "true"
test_endpoint "GET" "/api/library-templates" "Library templates" "" "true"
test_endpoint "GET" "/api/libraries/default" "Get default library" "" "true"
test_endpoint "GET" "/api/libraries/test_library" "Get test library" "" "true"

echo ""
echo "Collection Management:"
echo "---------------------"
test_endpoint "GET" "/api/collections" "List collections" "" "true"
test_endpoint "POST" "/api/collections" "Create collection" '{"library":"default","name":"test_collection"}' "true"
test_endpoint "GET" "/api/collections?library=default" "List collections in library" "" "true"

echo ""
echo "Document Operations (No Auth - Testing):"
echo "---------------------------------------"
test_endpoint "GET" "/api/collections/default/test_collection/documents" "List documents" "" "false"
test_endpoint "POST" "/api/collections/default/test_collection" "Create document" '{"name":"test","value":123}' "false"
test_endpoint "GET" "/api/collections/default/test_collection/doc-123" "Get document" "" "false"
test_endpoint "PUT" "/api/collections/default/test_collection/doc-123" "Update document" '{"name":"test","value":456}' "false"
test_endpoint "DELETE" "/api/collections/default/test_collection/doc-123" "Delete document" "" "false"

echo ""
echo "Unified Documents API:"
echo "--------------------"
test_endpoint "GET" "/api/documents" "Query all documents" "" "true"
test_endpoint "GET" "/api/documents?type=user" "Query by type" "" "true"
test_endpoint "POST" "/api/documents" "Create unified document" '{"type":"test","name":"unified_test"}' "true"
test_endpoint "GET" "/api/documents/doc-123" "Get unified document" "" "true"

echo ""
echo "RBAC Management:"
echo "---------------"
test_endpoint "GET" "/api/users" "List users" "" "true"
test_endpoint "GET" "/api/roles" "List roles" "" "true"
test_endpoint "POST" "/api/users" "Create user" '{"username":"newuser","password":"pass123","email":"new@example.com"}' "true"
test_endpoint "POST" "/api/roles" "Create role" '{"name":"test_role","permissions":[]}' "true"

echo ""
echo "Metrics & Monitoring:"
echo "-------------------"
test_endpoint "GET" "/api/metrics" "Get metrics" "" "true"
test_endpoint "GET" "/api/metrics/stats" "Metrics statistics" "" "true"
test_endpoint "GET" "/api/metrics/activity" "Activity metrics" "" "true"
test_endpoint "GET" "/api/metrics/history" "Metrics history" "" "true"

echo ""
echo "Configuration Management:"
echo "-----------------------"
test_endpoint "GET" "/api/config" "Get configuration" "" "true"
test_endpoint "GET" "/api/system/log-control" "Get log control" "" "true"

echo ""
echo "Data Visualization:"
echo "------------------"
test_endpoint "GET" "/api/visualization/collection-stats" "Collection statistics" "" "true"
test_endpoint "GET" "/api/visualization/document-types" "Document types" "" "true"
test_endpoint "GET" "/api/visualization/field-distribution" "Field distribution" "" "true"

echo ""
echo "Import/Export:"
echo "-------------"
test_endpoint "POST" "/api/export" "Export data" '{"collections":["test_collection"]}' "true"
test_endpoint "POST" "/api/import" "Import data" '{"data":{}}' "true"

echo ""
echo "Testing complete!"