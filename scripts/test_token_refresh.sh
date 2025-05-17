#!/bin/bash
# Test script for JSONdb token refresh mechanism

# Configuration
BASE_URL="http://localhost:5000"
AUTH_ENDPOINT="/api/auth/login"
REFRESH_ENDPOINT="/api/auth/refresh"
TEST_ENDPOINT="/api/collections"
USERNAME="admin"
PASSWORD="admin"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== JSONdb Token Refresh Test ===${NC}"
echo "Testing token refresh workflow with user: $USERNAME"
echo

# Step 1: Login and get initial tokens
echo -e "${BLUE}Step 1: Login and obtain token pair${NC}"
LOGIN_RESPONSE=$(curl -s -X POST "$BASE_URL$AUTH_ENDPOINT" \
  -H "Content-Type: application/json" \
  -d "{\"username\":\"$USERNAME\",\"password\":\"$PASSWORD\"}")

# Extract tokens from response
ACCESS_TOKEN=$(echo $LOGIN_RESPONSE | grep -o '"token":"[^"]*' | sed 's/"token":"//')
REFRESH_TOKEN=$(echo $LOGIN_RESPONSE | grep -o '"refresh_token":"[^"]*' | sed 's/"refresh_token":"//')

if [ -z "$ACCESS_TOKEN" ]; then
  echo -e "${RED}Failed to get access token. Response:${NC}"
  echo $LOGIN_RESPONSE
  exit 1
fi

if [ -z "$REFRESH_TOKEN" ]; then
  echo -e "${YELLOW}No refresh token returned. This may indicate the feature is not enabled.${NC}"
else
  echo -e "${GREEN}Successfully obtained token pair${NC}"
  echo "Access token: ${ACCESS_TOKEN:0:20}..."
  echo "Refresh token: ${REFRESH_TOKEN:0:20}..."
fi

# Step 2: Test access with the access token
echo
echo -e "${BLUE}Step 2: Testing API access with access token${NC}"
TEST_RESPONSE=$(curl -s -X GET "$BASE_URL$TEST_ENDPOINT" \
  -H "Authorization: Bearer $ACCESS_TOKEN")

if echo "$TEST_RESPONSE" | grep -q "collections"; then
  echo -e "${GREEN}API access successful with access token${NC}"
else
  echo -e "${RED}API access failed with access token. Response:${NC}"
  echo $TEST_RESPONSE
fi

# Step 3: Refresh the token
echo
echo -e "${BLUE}Step 3: Refreshing token${NC}"

if [ -z "$REFRESH_TOKEN" ]; then
  echo -e "${YELLOW}Skipping token refresh test (no refresh token available)${NC}"
else
  REFRESH_RESPONSE=$(curl -s -X POST "$BASE_URL$REFRESH_ENDPOINT" \
    -H "Content-Type: application/json" \
    -d "{\"refresh_token\":\"$REFRESH_TOKEN\"}")
  
  # Extract new tokens
  NEW_ACCESS_TOKEN=$(echo $REFRESH_RESPONSE | grep -o '"token":"[^"]*' | sed 's/"token":"//')
  NEW_REFRESH_TOKEN=$(echo $REFRESH_RESPONSE | grep -o '"refresh_token":"[^"]*' | sed 's/"refresh_token":"//')
  
  if [ -z "$NEW_ACCESS_TOKEN" ]; then
    echo -e "${RED}Token refresh failed. Response:${NC}"
    echo $REFRESH_RESPONSE
  else
    echo -e "${GREEN}Token refresh successful${NC}"
    echo "New access token: ${NEW_ACCESS_TOKEN:0:20}..."
    echo "New refresh token: ${NEW_REFRESH_TOKEN:0:20}..."
    
    # Step 4: Test access with the new access token
    echo
    echo -e "${BLUE}Step 4: Testing API access with new access token${NC}"
    NEW_TEST_RESPONSE=$(curl -s -X GET "$BASE_URL$TEST_ENDPOINT" \
      -H "Authorization: Bearer $NEW_ACCESS_TOKEN")
    
    if echo "$NEW_TEST_RESPONSE" | grep -q "collections"; then
      echo -e "${GREEN}API access successful with new access token${NC}"
    else
      echo -e "${RED}API access failed with new access token. Response:${NC}"
      echo $NEW_TEST_RESPONSE
    fi
  fi
fi

echo
echo -e "${BLUE}=== Test Complete ===${NC}"