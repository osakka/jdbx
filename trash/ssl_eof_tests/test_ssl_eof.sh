#!/bin/bash
# Test specifically for the N-1 byte SSL EOF issue

echo "🔍 Testing SSL EOF handling with medium-sized document..."

# Get auth token
TOKEN=$(curl -k -s -X POST https://localhost:5000/api/auth/login \
    -H "Content-Type: application/json" \
    -d '{"username": "admin", "password": "secure123456789"}' | \
    python3 -c "import sys, json; print(json.load(sys.stdin).get('token', ''))")

if [ -z "$TOKEN" ]; then
    echo "❌ Failed to authenticate"
    exit 1
fi

echo "✅ Authenticated successfully"

# Create a 5KB document
CONTENT=$(python3 -c "print('x' * 5000)")
JSON_DOC=$(python3 -c "
import json
doc = {
    'title': 'Test Medium Document',
    'content': '$CONTENT',
    'test': 'ssl_eof'
}
print(json.dumps(doc))
")

echo "📝 Document size: $(echo -n "$JSON_DOC" | wc -c) bytes"

# Test with curl (which has the OpenSSL 3.x EOF issue)
echo "🧪 Testing with curl..."
RESPONSE=$(curl -k -s -w "\n%{http_code}" -X POST \
    https://localhost:5000/api/collections/test_ssl/documents \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "$JSON_DOC")

HTTP_CODE=$(echo "$RESPONSE" | tail -1)
BODY=$(echo "$RESPONSE" | head -n -1)

echo "📊 Response code: $HTTP_CODE"
if [ "$HTTP_CODE" = "201" ]; then
    echo "✅ SUCCESS: Document created successfully!"
    echo "🎉 SSL EOF handling is working!"
else
    echo "❌ FAILED: Document creation failed"
    echo "Response: $BODY"
fi

# Also test with Python requests to see if it has the same issue
echo ""
echo "🧪 Testing with Python requests..."
python3 - <<EOF
import requests
import json
import urllib3
urllib3.disable_warnings()

doc = {
    'title': 'Test Python Document',
    'content': 'x' * 5000,
    'test': 'python_ssl_eof'
}

try:
    resp = requests.post(
        'https://localhost:5000/api/collections/test_python/documents',
        headers={
            'Authorization': 'Bearer $TOKEN',
            'Content-Type': 'application/json'
        },
        json=doc,
        verify=False
    )
    print(f"📊 Response code: {resp.status_code}")
    if resp.status_code == 201:
        print("✅ SUCCESS: Python requests document created!")
    else:
        print(f"❌ FAILED: {resp.text}")
except Exception as e:
    print(f"❌ ERROR: {e}")
EOF