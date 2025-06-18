#!/bin/bash
# Focus on real JDBX bugs, not client SSL issues

echo "🎯 Testing Real JDBX Issues"
echo "=========================="

# Get token
TOKEN=$(curl -k -s -X POST https://localhost:5000/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"secure123456789"}' | jq -r .token)

echo -e "\n1. Testing large document response"
echo "   Creating 10KB document..."

# Create a large document with padding to avoid N-1 issue
LARGE_DATA=$(python3 -c "print('x' * 10000)")
RESPONSE=$(curl -k -s -X POST https://localhost:5000/api/documents \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d "{\"title\":\"Large Test\",\"type\":\"large-test\",\"data\":\"$LARGE_DATA\",\"padding\":\"xxx\"}")

echo "   Raw response (first 200 chars):"
echo "   $RESPONSE" | head -c 200
echo -e "\n"

# Try to parse as JSON
if echo "$RESPONSE" | jq . >/dev/null 2>&1; then
  echo "   ✅ Valid JSON response"
  UUID=$(echo "$RESPONSE" | jq -r .uuid)
  echo "   UUID: $UUID"
else
  echo "   ❌ Invalid JSON response"
fi

echo -e "\n2. Testing query results"
# Create multiple documents first
for i in {1..5}; do
  curl -k -s -X POST https://localhost:5000/api/documents \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "{\"title\":\"Query Test $i\",\"type\":\"query-test\",\"index\":$i,\"pad\":\"xx\"}" >/dev/null
done

echo "   Created 5 documents, now querying..."
RESPONSE=$(curl -k -s -X POST https://localhost:5000/api/documents/query \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"type":"query-test"}')

if echo "$RESPONSE" | jq . >/dev/null 2>&1; then
  COUNT=$(echo "$RESPONSE" | jq '.documents | length')
  echo "   Found $COUNT documents (expected 5)"
  
  # Check if we got all documents
  if [ "$COUNT" -eq 5 ]; then
    echo "   ✅ Query returned all documents"
  else
    echo "   ❌ Query missing documents"
    echo "   Response: $RESPONSE" | head -c 500
  fi
else
  echo "   ❌ Invalid query response"
fi

echo -e "\n3. Testing concurrent document creation"
echo "   Creating 10 documents concurrently..."

# Function to create a document
create_doc() {
  local id=$1
  curl -k -s -X POST https://localhost:5000/api/documents \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "{\"title\":\"Concurrent $id\",\"type\":\"concurrent-test\",\"id\":$id,\"p\":\"x\"}" 2>&1
}

# Run concurrent creates
SUCCESS=0
for i in {1..10}; do
  create_doc $i &
done

# Wait for all to complete
wait

# Count successes
RESPONSE=$(curl -k -s -X POST https://localhost:5000/api/documents/query \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"type":"concurrent-test"}')

if echo "$RESPONSE" | jq . >/dev/null 2>&1; then
  COUNT=$(echo "$RESPONSE" | jq '.documents | length')
  echo "   Created $COUNT documents (expected 10)"
  
  if [ "$COUNT" -eq 10 ]; then
    echo "   ✅ All concurrent creates succeeded"
  else
    echo "   ❌ Some concurrent creates failed"
  fi
fi

echo -e "\n4. Testing API endpoints"
echo "   Testing /api/libraries..."
RESPONSE=$(curl -k -s https://localhost:5000/api/libraries \
  -H "Authorization: Bearer $TOKEN")

echo "   Response: $RESPONSE" | head -c 200
if echo "$RESPONSE" | jq . >/dev/null 2>&1; then
  echo "   ✅ Valid JSON"
else
  echo "   ❌ Invalid JSON"
fi

echo -e "\n   Testing /api/collections..."
RESPONSE=$(curl -k -s https://localhost:5000/api/collections \
  -H "Authorization: Bearer $TOKEN")

echo "   Response: $RESPONSE" | head -c 200
if echo "$RESPONSE" | jq . >/dev/null 2>&1; then
  echo "   ✅ Valid JSON"
else
  echo "   ❌ Invalid JSON"
fi