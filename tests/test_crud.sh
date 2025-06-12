#!/bin/bash
# Test CRUD operations with JDBX-only implementation

BASE_URL="https://localhost:5000"
CURL_OPTS="-k -s"

echo "=== Testing JDBX CRUD Operations ==="
echo

# 1. Test Health endpoint
echo "1. Testing health endpoint..."
curl $CURL_OPTS $BASE_URL/api/health | jq . || echo "FAILED: Health check"
echo

# 2. Create a test collection
echo "2. Creating test collection..."
curl $CURL_OPTS -X POST $BASE_URL/api/collections/test_collection \
  -H "Content-Type: application/json" | jq . || echo "FAILED: Create collection"
echo

# 3. Insert a document
echo "3. Inserting a document..."
curl $CURL_OPTS -X POST $BASE_URL/api/collections/test_collection/documents \
  -H "Content-Type: application/json" \
  -d '{"name": "Test Item", "value": 42, "tags": ["test", "jdbx"]}' | jq . || echo "FAILED: Insert document"
echo

# 4. List documents
echo "4. Listing documents..."
curl $CURL_OPTS $BASE_URL/api/collections/test_collection/documents | jq . || echo "FAILED: List documents"
echo

# 5. Query documents
echo "5. Querying documents..."
curl $CURL_OPTS -X POST $BASE_URL/api/collections/test_collection/query \
  -H "Content-Type: application/json" \
  -d '{"name": "Test Item"}' | jq . || echo "FAILED: Query documents"
echo

# 6. Update a document (if we got an ID)
echo "6. Update test - need to get ID first"
DOC_ID=$(curl $CURL_OPTS $BASE_URL/api/collections/test_collection/documents | jq -r '.data[0]._id' 2>/dev/null)
if [ ! -z "$DOC_ID" ] && [ "$DOC_ID" != "null" ]; then
  echo "   Updating document $DOC_ID..."
  curl $CURL_OPTS -X PUT $BASE_URL/api/collections/test_collection/documents/$DOC_ID \
    -H "Content-Type: application/json" \
    -d '{"value": 100, "updated": true}' | jq . || echo "FAILED: Update document"
else
  echo "   No document ID found to update"
fi
echo

# 7. Delete test
if [ ! -z "$DOC_ID" ] && [ "$DOC_ID" != "null" ]; then
  echo "7. Deleting document $DOC_ID..."
  curl $CURL_OPTS -X DELETE $BASE_URL/api/collections/test_collection/documents/$DOC_ID | jq . || echo "FAILED: Delete document"
else
  echo "7. No document to delete"
fi
echo

# 8. Check JDBX file
echo "8. Checking JDBX file..."
ls -la /opt/jsondb/build/var/*.jdbx
echo

# 9. Check for any .idx or .mmap files (should be none)
echo "9. Checking for legacy files (should be 0)..."
find /opt/jsondb/build/var -name "*.idx" -o -name "*.mmap" 2>/dev/null | wc -l

echo
echo "=== CRUD Test Complete ==="