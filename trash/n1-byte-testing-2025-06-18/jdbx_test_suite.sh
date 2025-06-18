#!/bin/bash
# JDBX User Testing Suite
# Uses curl to avoid Python SSL bugs

BASE_URL="https://localhost:5000"
USERNAME="admin"
PASSWORD="secure123456789"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

echo "🧪 JDBX User Testing Suite"
echo "=========================="

# Get auth token
echo -e "\n${BLUE}1. Authentication Test${NC}"
TOKEN=$(curl -k -s -X POST "$BASE_URL/api/auth/login" \
  -H "Content-Type: application/json" \
  -d "{\"username\":\"$USERNAME\",\"password\":\"$PASSWORD\"}" | jq -r .token)

if [ -n "$TOKEN" ]; then
  echo -e "   ${GREEN}✅ Login successful${NC}"
else
  echo -e "   ${RED}❌ Login failed${NC}"
  exit 1
fi

# Test document creation
echo -e "\n${BLUE}2. Document Creation Tests${NC}"

# Small document
echo "   Creating small document..."
RESPONSE=$(curl -k -s -X POST "$BASE_URL/api/documents" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"title":"Small TODO","type":"todo","priority":"high"}')
  
UUID1=$(echo "$RESPONSE" | jq -r .uuid)
if [ "$UUID1" != "null" ]; then
  echo -e "   ${GREEN}✅ Created: $UUID1${NC}"
else
  echo -e "   ${RED}❌ Failed: $RESPONSE${NC}"
fi

# Medium document (5KB)
echo "   Creating 5KB document..."
DATA=$(python3 -c "print('x' * 5000)")
RESPONSE=$(curl -k -s -X POST "$BASE_URL/api/documents" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d "{\"title\":\"Medium TODO\",\"type\":\"todo\",\"data\":\"$DATA\"}")
  
UUID2=$(echo "$RESPONSE" | jq -r .uuid)
if [ "$UUID2" != "null" ]; then
  echo -e "   ${GREEN}✅ Created: $UUID2${NC}"
else
  echo -e "   ${RED}❌ Failed: $RESPONSE${NC}"
fi

# Large document (50KB)
echo "   Creating 50KB document..."
DATA=$(python3 -c "print('x' * 50000)")
RESPONSE=$(curl -k -s -X POST "$BASE_URL/api/documents" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d "{\"title\":\"Large TODO\",\"type\":\"todo\",\"data\":\"$DATA\"}")
  
UUID3=$(echo "$RESPONSE" | jq -r .uuid)
if [ "$UUID3" != "null" ]; then
  echo -e "   ${GREEN}✅ Created: $UUID3${NC}"
else
  echo -e "   ${RED}❌ Failed: $RESPONSE${NC}"
fi

# Test querying
echo -e "\n${BLUE}3. Query Tests${NC}"

echo "   Querying TODO documents..."
RESPONSE=$(curl -k -s -X POST "$BASE_URL/api/documents/query" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"type":"todo"}')

COUNT=$(echo "$RESPONSE" | jq '.documents | length')
if [ "$COUNT" -gt 0 ]; then
  echo -e "   ${GREEN}✅ Found $COUNT documents${NC}"
  echo "$RESPONSE" | jq -r '.documents[] | "      - \(.title) (\(.uuid))"' | head -5
else
  echo -e "   ${RED}❌ No documents found${NC}"
  echo "   Response: $RESPONSE"
fi

# Test updates
echo -e "\n${BLUE}4. Update Tests${NC}"

if [ -n "$UUID1" ] && [ "$UUID1" != "null" ]; then
  echo "   Updating document $UUID1..."
  RESPONSE=$(curl -k -s -X PUT "$BASE_URL/api/documents/$UUID1" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{"status":"completed","priority":"low"}')
    
  if [ "$(echo "$RESPONSE" | jq -r .status)" = "completed" ]; then
    echo -e "   ${GREEN}✅ Update successful${NC}"
  else
    echo -e "   ${RED}❌ Update failed: $RESPONSE${NC}"
  fi
fi

# Test deletion
echo -e "\n${BLUE}5. Delete Tests${NC}"

if [ -n "$UUID3" ] && [ "$UUID3" != "null" ]; then
  echo "   Deleting document $UUID3..."
  STATUS=$(curl -k -s -o /dev/null -w "%{http_code}" -X DELETE "$BASE_URL/api/documents/$UUID3" \
    -H "Authorization: Bearer $TOKEN")
    
  if [ "$STATUS" = "200" ] || [ "$STATUS" = "204" ]; then
    echo -e "   ${GREEN}✅ Delete successful${NC}"
  else
    echo -e "   ${RED}❌ Delete failed: HTTP $STATUS${NC}"
  fi
fi

# Stress test
echo -e "\n${BLUE}6. Stress Tests${NC}"

echo "   Creating 20 documents rapidly..."
SUCCESS=0
FAILED=0

for i in {1..20}; do
  RESPONSE=$(curl -k -s -X POST "$BASE_URL/api/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "{\"title\":\"Stress Test $i\",\"type\":\"stress-test\",\"index\":$i}" 2>&1)
    
  if echo "$RESPONSE" | grep -q "uuid"; then
    ((SUCCESS++))
  else
    ((FAILED++))
    echo "      Failed #$i: $RESPONSE"
  fi
done

echo -e "   Results: ${GREEN}$SUCCESS succeeded${NC}, ${RED}$FAILED failed${NC}"

# API discovery test
echo -e "\n${BLUE}7. API Discovery Tests${NC}"

echo "   Testing /api/libraries endpoint..."
RESPONSE=$(curl -k -s -X GET "$BASE_URL/api/libraries" \
  -H "Authorization: Bearer $TOKEN")
  
if echo "$RESPONSE" | jq . >/dev/null 2>&1; then
  COUNT=$(echo "$RESPONSE" | jq '.libraries | length')
  echo -e "   ${GREEN}✅ Found $COUNT libraries${NC}"
else
  echo -e "   ${RED}❌ Invalid response: $RESPONSE${NC}"
fi

echo "   Testing /api/collections endpoint..."
RESPONSE=$(curl -k -s -X GET "$BASE_URL/api/collections" \
  -H "Authorization: Bearer $TOKEN")
  
if echo "$RESPONSE" | jq . >/dev/null 2>&1; then
  COUNT=$(echo "$RESPONSE" | jq '.collections | length')
  echo -e "   ${GREEN}✅ Found $COUNT collections${NC}"
else
  echo -e "   ${RED}❌ Invalid response: $RESPONSE${NC}"
fi

echo -e "\n${GREEN}✅ Testing complete!${NC}"