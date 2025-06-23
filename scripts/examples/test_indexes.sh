#!/bin/bash

# Test script for JSONdb index functionality
# This script demonstrates creating, using, and querying with indexes

# Configuration
SERVER_URL="http://localhost:5000"
AUTH_TOKEN=""
TEST_COLLECTION="index_test"

# Colors for terminal output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

function log_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

function log_error() {
    echo -e "${RED}✗ $1${NC}"
    echo "Response: $2"
    exit 1
}

function log_info() {
    echo -e "${YELLOW}ℹ $1${NC}"
}

# Function to authenticate and get token
function authenticate() {
    log_info "Authenticating to the server..."
    
    # Replace with your server's authentication credentials
    response=$(curl -s -X POST "${SERVER_URL}/api/auth/login" \
        -H "Content-Type: application/json" \
        -d '{
            "username": "admin",
            "password": "admin123"
        }')
    
    # Extract token from response
    AUTH_TOKEN=$(echo $response | grep -o '"token":"[^"]*' | grep -o '[^"]*$')
    
    if [ -z "$AUTH_TOKEN" ]; then
        log_error "Authentication failed" "$response"
    else
        log_success "Authentication successful"
    fi
}

# Function to create a test collection
function create_collection() {
    log_info "Creating test collection '$TEST_COLLECTION'..."
    
    response=$(curl -s -X POST "${SERVER_URL}/api/collections" \
        -H "Authorization: Bearer ${AUTH_TOKEN}" \
        -H "Content-Type: application/json" \
        -d "{
            \"name\": \"${TEST_COLLECTION}\"
        }")
    
    if echo "$response" | grep -q "error"; then
        # Check if it's because collection already exists
        if echo "$response" | grep -q "already exists"; then
            log_info "Collection already exists, continuing..."
        else
            log_error "Failed to create collection" "$response"
        fi
    else
        log_success "Collection created successfully"
    fi
}

# Function to add test data
function add_test_data() {
    log_info "Adding test documents to collection..."
    
    # Add user 1
    curl -s -X POST "${SERVER_URL}/api/collections/${TEST_COLLECTION}" \
        -H "Authorization: Bearer ${AUTH_TOKEN}" \
        -H "Content-Type: application/json" \
        -d '{
            "name": "John Doe",
            "email": "john.doe@example.com",
            "age": 30,
            "status": "active",
            "role": "admin",
            "created_at": "2023-01-15T10:30:00Z"
        }' > /dev/null

    # Add user 2
    curl -s -X POST "${SERVER_URL}/api/collections/${TEST_COLLECTION}" \
        -H "Authorization: Bearer ${AUTH_TOKEN}" \
        -H "Content-Type: application/json" \
        -d '{
            "name": "Jane Smith",
            "email": "jane.smith@example.com",
            "age": 25,
            "status": "active",
            "role": "user",
            "created_at": "2023-02-20T14:45:00Z"
        }' > /dev/null

    # Add user 3
    curl -s -X POST "${SERVER_URL}/api/collections/${TEST_COLLECTION}" \
        -H "Authorization: Bearer ${AUTH_TOKEN}" \
        -H "Content-Type: application/json" \
        -d '{
            "name": "Bob Johnson",
            "email": "bob.johnson@example.com",
            "age": 42,
            "status": "inactive",
            "role": "user",
            "created_at": "2022-11-05T09:15:00Z"
        }' > /dev/null

    # Add user 4
    curl -s -X POST "${SERVER_URL}/api/collections/${TEST_COLLECTION}" \
        -H "Authorization: Bearer ${AUTH_TOKEN}" \
        -H "Content-Type: application/json" \
        -d '{
            "name": "Alice Brown",
            "email": "alice.brown@example.com",
            "age": 35,
            "status": "active",
            "role": "admin",
            "created_at": "2023-03-10T16:20:00Z"
        }' > /dev/null

    # Add user 5
    curl -s -X POST "${SERVER_URL}/api/collections/${TEST_COLLECTION}" \
        -H "Authorization: Bearer ${AUTH_TOKEN}" \
        -H "Content-Type: application/json" \
        -d '{
            "name": "Charlie Wilson",
            "email": "charlie.wilson@example.com",
            "age": 28,
            "status": "pending",
            "role": "user",
            "created_at": "2023-04-05T11:10:00Z"
        }' > /dev/null
        
    log_success "Added 5 test documents"
}

# Function to create indexes
function create_indexes() {
    log_info "Creating indexes on collection..."
    
    # Create email index (unique)
    response=$(curl -s -X POST "${SERVER_URL}/api/indexes/${TEST_COLLECTION}" \
        -H "Authorization: Bearer ${AUTH_TOKEN}" \
        -H "Content-Type: application/json" \
        -d '{
            "name": "email_idx",
            "field": "email",
            "type": "unique"
        }')
    
    if echo "$response" | grep -q "error"; then
        log_error "Failed to create email index" "$response"
    else
        log_success "Email index created successfully"
    fi
    
    # Create age index
    response=$(curl -s -X POST "${SERVER_URL}/api/indexes/${TEST_COLLECTION}" \
        -H "Authorization: Bearer ${AUTH_TOKEN}" \
        -H "Content-Type: application/json" \
        -d '{
            "name": "age_idx",
            "field": "age",
            "type": "non_unique"
        }')
    
    if echo "$response" | grep -q "error"; then
        log_error "Failed to create age index" "$response"
    else
        log_success "Age index created successfully"
    fi
    
    # Create status index
    response=$(curl -s -X POST "${SERVER_URL}/api/indexes/${TEST_COLLECTION}" \
        -H "Authorization: Bearer ${AUTH_TOKEN}" \
        -H "Content-Type: application/json" \
        -d '{
            "name": "status_idx",
            "field": "status",
            "type": "non_unique"
        }')
    
    if echo "$response" | grep -q "error"; then
        log_error "Failed to create status index" "$response"
    else
        log_success "Status index created successfully"
    fi
    
    # Create role index
    response=$(curl -s -X POST "${SERVER_URL}/api/indexes/${TEST_COLLECTION}" \
        -H "Authorization: Bearer ${AUTH_TOKEN}" \
        -H "Content-Type: application/json" \
        -d '{
            "name": "role_idx",
            "field": "role",
            "type": "non_unique"
        }')
    
    if echo "$response" | grep -q "error"; then
        log_error "Failed to create role index" "$response"
    else
        log_success "Role index created successfully"
    fi
}

# Function to list indexes
function list_indexes() {
    log_info "Listing indexes for collection..."
    
    response=$(curl -s -X GET "${SERVER_URL}/api/indexes/${TEST_COLLECTION}" \
        -H "Authorization: Bearer ${AUTH_TOKEN}")
    
    if echo "$response" | grep -q "error"; then
        log_error "Failed to list indexes" "$response"
    else
        log_success "Indexes retrieved successfully:"
        echo "$response" | python3 -m json.tool
    fi
}

# Function to query using an index
function test_index_query() {
    log_info "Testing index query for email..."
    
    response=$(curl -s -X POST "${SERVER_URL}/api/indexes/query/${TEST_COLLECTION}" \
        -H "Authorization: Bearer ${AUTH_TOKEN}" \
        -H "Content-Type: application/json" \
        -d '{
            "field": "email",
            "value": "john.doe@example.com"
        }')
    
    if echo "$response" | grep -q "error"; then
        log_error "Failed to query by email index" "$response"
    else
        log_success "Email index query successful:"
        echo "$response" | python3 -m json.tool
    fi
    
    log_info "Testing index query for age..."
    
    response=$(curl -s -X POST "${SERVER_URL}/api/indexes/query/${TEST_COLLECTION}" \
        -H "Authorization: Bearer ${AUTH_TOKEN}" \
        -H "Content-Type: application/json" \
        -d '{
            "field": "age",
            "value": 30
        }')
    
    if echo "$response" | grep -q "error"; then
        log_error "Failed to query by age index" "$response"
    else
        log_success "Age index query successful:"
        echo "$response" | python3 -m json.tool
    fi
}

# Function to test compound queries
function test_compound_query() {
    log_info "Testing compound query (role=admin AND status=active)..."
    
    response=$(curl -s -X POST "${SERVER_URL}/api/indexes/compound/${TEST_COLLECTION}" \
        -H "Authorization: Bearer ${AUTH_TOKEN}" \
        -H "Content-Type: application/json" \
        -d '{
            "operation": "AND",
            "queries": [
                {
                    "field": "role",
                    "value": "admin"
                },
                {
                    "field": "status",
                    "value": "active"
                }
            ]
        }')
    
    if echo "$response" | grep -q "error"; then
        log_error "Failed to perform compound query" "$response"
    else
        log_success "Compound query successful:"
        echo "$response" | python3 -m json.tool
    fi
}

# Function to get index statistics
function test_index_stats() {
    log_info "Getting statistics for email index..."
    
    response=$(curl -s -X GET "${SERVER_URL}/api/indexes/stats/${TEST_COLLECTION}/email_idx" \
        -H "Authorization: Bearer ${AUTH_TOKEN}")
    
    if echo "$response" | grep -q "error"; then
        log_error "Failed to get index statistics" "$response"
    else
        log_success "Index statistics retrieved successfully:"
        echo "$response" | python3 -m json.tool
    fi
}

# Function to rebuild an index
function test_index_rebuild() {
    log_info "Testing index rebuild..."
    
    response=$(curl -s -X POST "${SERVER_URL}/api/indexes/rebuild/${TEST_COLLECTION}/email_idx" \
        -H "Authorization: Bearer ${AUTH_TOKEN}")
    
    if echo "$response" | grep -q "error"; then
        log_error "Failed to rebuild index" "$response"
    else
        log_success "Index rebuilt successfully:"
        echo "$response" | python3 -m json.tool
    fi
}

# Run tests
authenticate
create_collection
add_test_data
create_indexes
list_indexes
test_index_query
test_compound_query
test_index_stats
test_index_rebuild

log_info "All tests completed successfully!"