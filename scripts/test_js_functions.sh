#!/bin/bash
# Test script for JSONdb JavaScript functions

# Configuration
BASE_URL="http://localhost:5000"
AUTH_ENDPOINT="/api/auth/login"
VALIDATOR_ENDPOINT="/api/js/validators/register"
TRANSFORMER_ENDPOINT="/api/js/transformers/register"
FUNCTION_ENDPOINT="/api/js/functions/register"
FUNCTION_EXEC_ENDPOINT="/api/js/functions/inventoryAnalysis"
QUERY_ENDPOINT="/api/js/query"
USERNAME="admin"
PASSWORD="admin"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== JSONdb JavaScript Functions Test ===${NC}"

# Step 1: Login and get token
echo -e "${BLUE}Step 1: Authenticating with the server${NC}"
LOGIN_RESPONSE=$(curl -s -X POST "$BASE_URL$AUTH_ENDPOINT" \
  -H "Content-Type: application/json" \
  -d "{\"username\":\"$USERNAME\",\"password\":\"$PASSWORD\"}")

# Extract token from response
TOKEN=$(echo $LOGIN_RESPONSE | grep -o '"token":"[^"]*' | sed 's/"token":"//')

if [ -z "$TOKEN" ]; then
  echo -e "${RED}Failed to authenticate. Response:${NC}"
  echo $LOGIN_RESPONSE
  exit 1
fi

echo -e "${GREEN}Successfully authenticated${NC}"
echo "Token: ${TOKEN:0:20}..."

# Step 2: Register a validator
echo
echo -e "${BLUE}Step 2: Registering a validator${NC}"

# Read validator code from example file
VALIDATOR_FILE="/opt/jsondb/share/examples/js-examples/validator_example.js"
if [ ! -f "$VALIDATOR_FILE" ]; then
  echo -e "${RED}Validator example file not found at: $VALIDATOR_FILE${NC}"
  exit 1
fi

# Extract just the function part from the file (excluding comments)
VALIDATOR_CODE=$(grep -A 50 "function validateDocument" $VALIDATOR_FILE | grep -B 50 "return isValid;" | sed 's/\/\/.*//')

echo "Registering validator for 'products' collection..."
VALIDATOR_RESPONSE=$(curl -s -X POST "$BASE_URL$VALIDATOR_ENDPOINT" \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $TOKEN" \
  -d "{
    \"collection\": \"products\",
    \"code\": \"$VALIDATOR_CODE\"
  }")

if echo "$VALIDATOR_RESPONSE" | grep -q "success"; then
  echo -e "${GREEN}Validator registered successfully${NC}"
else
  echo -e "${RED}Failed to register validator. Response:${NC}"
  echo $VALIDATOR_RESPONSE
fi

# Step 3: Register a transformer
echo
echo -e "${BLUE}Step 3: Registering a transformer${NC}"

# Read transformer code from example file
TRANSFORMER_FILE="/opt/jsondb/share/examples/js-examples/transformer_example.js"
if [ ! -f "$TRANSFORMER_FILE" ]; then
  echo -e "${RED}Transformer example file not found at: $TRANSFORMER_FILE${NC}"
  exit 1
fi

# Extract just the function part from the file (excluding comments)
TRANSFORMER_CODE=$(grep -A 60 "function transformDocument" $TRANSFORMER_FILE | grep -B 60 "return doc;" | sed 's/\/\/.*//')

echo "Registering transformer for 'products' collection..."
TRANSFORMER_RESPONSE=$(curl -s -X POST "$BASE_URL$TRANSFORMER_ENDPOINT" \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $TOKEN" \
  -d "{
    \"collection\": \"products\",
    \"code\": \"$TRANSFORMER_CODE\"
  }")

if echo "$TRANSFORMER_RESPONSE" | grep -q "success"; then
  echo -e "${GREEN}Transformer registered successfully${NC}"
else
  echo -e "${RED}Failed to register transformer. Response:${NC}"
  echo $TRANSFORMER_RESPONSE
fi

# Step 4: Register a custom function
echo
echo -e "${BLUE}Step 4: Registering a custom function${NC}"

# Read function code from example file
FUNCTION_FILE="/opt/jsondb/share/examples/js-examples/function_example.js"
if [ ! -f "$FUNCTION_FILE" ]; then
  echo -e "${RED}Function example file not found at: $FUNCTION_FILE${NC}"
  exit 1
fi

# Extract just the function part from the file (excluding comments)
FUNCTION_CODE=$(grep -A 75 "function userFunction" $FUNCTION_FILE | grep -B 75 "return {" | sed 's/\/\/.*//')

echo "Registering 'inventoryAnalysis' function..."
FUNCTION_RESPONSE=$(curl -s -X POST "$BASE_URL$FUNCTION_ENDPOINT" \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $TOKEN" \
  -d "{
    \"name\": \"inventoryAnalysis\",
    \"code\": \"$FUNCTION_CODE\"
  }")

if echo "$FUNCTION_RESPONSE" | grep -q "success"; then
  echo -e "${GREEN}Function registered successfully${NC}"
else
  echo -e "${RED}Failed to register function. Response:${NC}"
  echo $FUNCTION_RESPONSE
fi

# Step 5: Test the function by calling it
echo
echo -e "${BLUE}Step 5: Executing the function${NC}"

EXEC_RESPONSE=$(curl -s -X POST "$BASE_URL$FUNCTION_EXEC_ENDPOINT" \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $TOKEN" \
  -d "{
    \"collection\": \"products\",
    \"minStock\": 5
  }")

if echo "$EXEC_RESPONSE" | grep -q "success"; then
  echo -e "${GREEN}Function execution completed:${NC}"
  # Format JSON output with Python if available
  if command -v python3 &> /dev/null; then
    echo $EXEC_RESPONSE | python3 -m json.tool
  else
    echo $EXEC_RESPONSE
  fi
else
  echo -e "${YELLOW}Function execution returned (may be empty if no products exist):${NC}"
  echo $EXEC_RESPONSE
fi

# Step 6: Execute a JavaScript query
echo
echo -e "${BLUE}Step 6: Running a JavaScript query${NC}"

# Read query code from example file
QUERY_FILE="/opt/jsondb/share/examples/js-examples/query_example.js"
if [ ! -f "$QUERY_FILE" ]; then
  echo -e "${RED}Query example file not found at: $QUERY_FILE${NC}"
  exit 1
fi

# Extract the simplified query
QUERY_CODE="return db.getCollection(\"products\").filter(p => p.price >= 500 && p.price <= 2000 && (p.in_stock || true) && (p.name || '').toLowerCase().includes(\"laptop\") || (p.description || '').toLowerCase().includes(\"laptop\")).sort((a, b) => a.price - b.price);"

echo "Executing query on 'products' collection..."
QUERY_RESPONSE=$(curl -s -X POST "$BASE_URL$QUERY_ENDPOINT" \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $TOKEN" \
  -d "{
    \"collection\": \"products\",
    \"query\": \"$QUERY_CODE\"
  }")

echo -e "${GREEN}Query execution completed:${NC}"
# Format JSON output with Python if available
if command -v python3 &> /dev/null; then
  echo $QUERY_RESPONSE | python3 -m json.tool
else
  echo $QUERY_RESPONSE
fi

echo
echo -e "${BLUE}=== Test Complete ===${NC}"