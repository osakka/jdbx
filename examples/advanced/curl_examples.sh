#!/bin/bash
# JSON Database Server - cURL Examples
# This script demonstrates how to interact with the JSON database server using cURL

BASE_URL="http://localhost:8080"
AUTH_TOKEN=""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print section header
print_section() {
    echo -e "\n${BLUE}=================================================="
    echo -e " $1"
    echo -e "==================================================${NC}"
}

# Print success message
print_success() {
    echo -e "${GREEN}SUCCESS: $1${NC}"
}

# Print error message
print_error() {
    echo -e "${RED}ERROR: $1${NC}"
}

# Print response
print_response() {
    echo -e "${YELLOW}RESPONSE:${NC}"
    echo "$1" | jq .
}

# Check if jq is installed
if ! command -v jq &> /dev/null; then
    echo "This script requires jq for JSON formatting. Please install it first."
    echo "On Debian/Ubuntu: sudo apt-get install jq"
    echo "On CentOS/RHEL: sudo yum install jq"
    echo "On macOS: brew install jq"
    exit 1
fi

# Check if the server is running
print_section "Checking Server Status"
if ! curl -s --connect-timeout 2 "$BASE_URL" > /dev/null; then
    print_error "Cannot connect to server at $BASE_URL"
    echo "Make sure the server is running and the URL is correct."
    exit 1
fi
print_success "Server is running at $BASE_URL"

# Authenticate (Login)
authenticate() {
    print_section "Authentication - Login"
    
    local username="admin"
    local password="admin"
    
    echo "Logging in as $username..."
    
    response=$(curl -s -X POST "$BASE_URL/api/auth/login" \
        -H "Content-Type: application/json" \
        -d "{\"username\":\"$username\",\"password\":\"$password\"}")
    
    print_response "$response"
    
    # Extract token
    AUTH_TOKEN=$(echo "$response" | jq -r '.token // empty')
    
    if [ -n "$AUTH_TOKEN" ]; then
        print_success "Authentication successful! Token: ${AUTH_TOKEN:0:10}..."
    else
        print_error "Authentication failed! Trying to register..."
        register
    fi
}

# Register new user
register() {
    print_section "Authentication - Register"
    
    local username="admin"
    local password="admin"
    
    echo "Registering user $username..."
    
    response=$(curl -s -X POST "$BASE_URL/api/auth/register" \
        -H "Content-Type: application/json" \
        -d "{\"username\":\"$username\",\"password\":\"$password\"}")
    
    print_response "$response"
    
    # Try login again
    authenticate
}

# Create collection
create_collection() {
    print_section "Creating Collection"
    
    local name="test_collection"
    
    echo "Creating collection '$name'..."
    
    response=$(curl -s -X POST "$BASE_URL/api/collections" \
        -H "Content-Type: application/json" \
        -H "Authorization: Bearer $AUTH_TOKEN" \
        -d "{\"name\":\"$name\"}")
    
    print_response "$response"
    
    if echo "$response" | jq -e '.name' > /dev/null; then
        print_success "Collection created successfully!"
    else
        print_error "Failed to create collection!"
    fi
}

# List collections
list_collections() {
    print_section "Listing Collections"
    
    echo "Getting all collections..."
    
    response=$(curl -s -X GET "$BASE_URL/api/collections" \
        -H "Authorization: Bearer $AUTH_TOKEN")
    
    print_response "$response"
    
    collection_count=$(echo "$response" | jq '.collections | length')
    print_success "Found $collection_count collections."
}

# Create document
create_document() {
    print_section "Creating Document"
    
    local collection="test_collection"
    
    echo "Creating document in '$collection'..."
    
    response=$(curl -s -X POST "$BASE_URL/api/collections/$collection/documents" \
        -H "Content-Type: application/json" \
        -H "Authorization: Bearer $AUTH_TOKEN" \
        -d '{
            "name": "Test Document",
            "value": 42,
            "tags": ["test", "example", "json"],
            "nested": {
                "property": "This is a nested property"
            }
        }')
    
    print_response "$response"
    
    # Extract document ID
    DOC_ID=$(echo "$response" | jq -r '._id // empty')
    
    if [ -n "$DOC_ID" ]; then
        print_success "Document created with ID: $DOC_ID"
    else
        print_error "Failed to create document!"
    fi
}

# Get document
get_document() {
    print_section "Getting Document"
    
    local collection="test_collection"
    
    if [ -z "$DOC_ID" ]; then
        print_error "No document ID available. Create a document first."
        return 1
    fi
    
    echo "Getting document with ID '$DOC_ID' from '$collection'..."
    
    response=$(curl -s -X GET "$BASE_URL/api/collections/$collection/documents/$DOC_ID" \
        -H "Authorization: Bearer $AUTH_TOKEN")
    
    print_response "$response"
    
    if echo "$response" | jq -e '._id' > /dev/null; then
        print_success "Document retrieved successfully!"
    else
        print_error "Failed to get document!"
    fi
}

# Update document
update_document() {
    print_section "Updating Document"
    
    local collection="test_collection"
    
    if [ -z "$DOC_ID" ]; then
        print_error "No document ID available. Create a document first."
        return 1
    fi
    
    echo "Updating document with ID '$DOC_ID' in '$collection'..."
    
    response=$(curl -s -X PUT "$BASE_URL/api/collections/$collection/documents/$DOC_ID" \
        -H "Content-Type: application/json" \
        -H "Authorization: Bearer $AUTH_TOKEN" \
        -d '{
            "name": "Updated Document",
            "value": 100,
            "tags": ["test", "example", "json", "updated"],
            "nested": {
                "property": "This is an updated nested property"
            }
        }')
    
    print_response "$response"
    
    if echo "$response" | jq -e '._id' > /dev/null; then
        print_success "Document updated successfully!"
    else
        print_error "Failed to update document!"
    fi
}

# Query documents
query_documents() {
    print_section "Querying Documents"
    
    local collection="test_collection"
    
    echo "Querying all documents in '$collection'..."
    
    response=$(curl -s -X GET "$BASE_URL/api/collections/$collection/documents" \
        -H "Authorization: Bearer $AUTH_TOKEN")
    
    print_response "$response"
    
    doc_count=$(echo "$response" | jq '.documents | length')
    print_success "Found $doc_count documents."
}

# Delete document
delete_document() {
    print_section "Deleting Document"
    
    local collection="test_collection"
    
    if [ -z "$DOC_ID" ]; then
        print_error "No document ID available. Create a document first."
        return 1
    fi
    
    echo "Deleting document with ID '$DOC_ID' from '$collection'..."
    
    response=$(curl -s -w "%{http_code}" -X DELETE "$BASE_URL/api/collections/$collection/documents/$DOC_ID" \
        -H "Authorization: Bearer $AUTH_TOKEN" -o /dev/null)
    
    if [ "$response" -eq 204 ]; then
        print_success "Document deleted successfully!"
    else
        print_error "Failed to delete document! Status code: $response"
    fi
}

# Main execution
echo -e "${BLUE}JSON Database Server - cURL Examples${NC}"
echo "This script demonstrates how to interact with the JSON database server."

# Run the examples
authenticate
create_collection
list_collections
create_document
get_document
update_document
query_documents
delete_document
query_documents  # Verify deletion

echo -e "\n${GREEN}Finished running examples!${NC}"