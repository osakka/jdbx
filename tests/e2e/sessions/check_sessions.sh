
#\!/bin/bash
# Check all sessions

TOKEN=$(curl -k -s -X POST https://localhost:5000/api/auth/login \
  -H "Content-Type: application/json" \
  -d "{\"username\":\"admin\",\"password\":\"secure123456789\"}"  < /dev/null |  jq -r ".token")

echo "=== All sessions ==="
curl -k -s https://localhost:5000/api/sessions \
  -H "Authorization: Bearer $TOKEN" | jq -r ".sessions[] | select(.username == \"admin\") | {uuid, active, token: .token[0:20]}"

