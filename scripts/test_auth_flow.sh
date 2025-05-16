#!/bin/bash
# Test script for JSONdb authentication flow
# This script tests the JWT authentication workflow after fixes

echo "JSONdb Authentication Test Script"
echo "================================="
echo

# Configuration
SERVER_HOST="localhost"
SERVER_PORT="5000"
BASE_URL="http://${SERVER_HOST}:${SERVER_PORT}"
TOKEN_FILE="/tmp/jsondb_auth_token.txt"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Helper functions
function print_header() {
  echo -e "${BLUE}=== $1 ===${NC}"
}

function print_success() {
  echo -e "${GREEN}✅ $1${NC}"
}

function print_error() {
  echo -e "${RED}❌ $1${NC}"
}

function print_info() {
  echo -e "${YELLOW}ℹ️ $1${NC}"
}

# Cleanup any previous token
rm -f "$TOKEN_FILE"

# 1. Test registration
print_header "Testing user registration"
# Use a timestamp to create a unique username
UNIQUE_USERNAME="testuser_$(date +%s)"
echo "Registering user $UNIQUE_USERNAME..."
REGISTER_RESPONSE=$(curl -s -X POST "${BASE_URL}/api/auth/register" \
  -H "Content-Type: application/json" \
  -d "{\"username\":\"$UNIQUE_USERNAME\", \"password\":\"password123\"}")

echo "Registration response: $REGISTER_RESPONSE"

if echo "$REGISTER_RESPONSE" | grep -q "user_id"; then
  print_success "Registration successful"
else
  print_error "Registration failed"
  exit 1
fi

# 2. Test login
print_header "Testing login"
echo "Logging in with user $UNIQUE_USERNAME..."
LOGIN_RESPONSE=$(curl -s -X POST "${BASE_URL}/api/auth/login" \
  -H "Content-Type: application/json" \
  -d "{\"username\":\"$UNIQUE_USERNAME\", \"password\":\"password123\"}")

echo "Login response: $LOGIN_RESPONSE"

if echo "$LOGIN_RESPONSE" | grep -q "token"; then
  print_success "Login successful"
  # Extract token
  TOKEN=$(echo "$LOGIN_RESPONSE" | sed -n 's/.*"token":"\([^"]*\)".*/\1/p')
  echo "$TOKEN" > "$TOKEN_FILE"
  print_info "Token saved to $TOKEN_FILE"
else
  print_error "Login failed"
  exit 1
fi

# 3. Test authenticated endpoint
print_header "Testing authenticated endpoint"
TOKEN=$(cat "$TOKEN_FILE")
echo "Using token: ${TOKEN:0:20}..."
echo "Testing collections list endpoint..."

COLLECTIONS_RESPONSE=$(curl -s -X GET "${BASE_URL}/api/collections" \
  -H "Authorization: Bearer $TOKEN")

echo "Collections response: $COLLECTIONS_RESPONSE"

if echo "$COLLECTIONS_RESPONSE" | grep -q "collections"; then
  print_success "Authentication successful, collections endpoint returned data"
elif echo "$COLLECTIONS_RESPONSE" | grep -q "Unauthorized"; then
  print_error "Authentication failed, received Unauthorized"
  exit 1
else
  print_error "Unknown response from collections endpoint"
  exit 1
fi

# 4. Summary
print_header "Authentication Test Summary"
echo "Registration: OK"
echo "Login: OK"
echo "Authenticated Request: OK"
print_success "All tests passed!"
echo
echo "You can use the token for further testing:"
echo "TOKEN=$(cat $TOKEN_FILE)"
echo
echo "Example usage:"
echo "curl -H \"Authorization: Bearer \$TOKEN\" ${BASE_URL}/api/collections"