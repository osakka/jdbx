#!/bin/bash

echo "=== JDBX Error Response Stability Test Suite ==="
echo "Testing server stability across multiple error scenarios..."
echo

BASE_URL="https://localhost:5000"
PASS=0
FAIL=0

# Function to check if server is still running
check_server() {
    if /opt/jdbx/build/jdbx_runtime.sh status | grep -q "is running"; then
        echo "✅ Server still running"
        return 0
    else
        echo "❌ SERVER CRASHED!"
        return 1
    fi
}

# Function to run a test
run_test() {
    local test_name="$1"
    local curl_cmd="$2"
    
    echo -n "Testing: $test_name... "
    
    # Run the curl command
    eval "$curl_cmd" > /dev/null 2>&1
    
    # Check if server is still running
    if check_server > /dev/null; then
        echo "PASS"
        ((PASS++))
    else
        echo "FAIL - Server crashed"
        ((FAIL++))
        exit 1
    fi
}

echo "=== 1. Authentication Error Tests ==="
run_test "Wrong password" \
    'curl -s -k $BASE_URL/api/auth/login -X POST -H "Content-Type: application/json" -d "{\"username\":\"admin\",\"password\":\"wrongpass\"}"'

run_test "Missing username" \
    'curl -s -k $BASE_URL/api/auth/login -X POST -H "Content-Type: application/json" -d "{\"password\":\"test\"}"'

run_test "Missing password" \
    'curl -s -k $BASE_URL/api/auth/login -X POST -H "Content-Type: application/json" -d "{\"username\":\"admin\"}"'

run_test "Empty JSON" \
    'curl -s -k $BASE_URL/api/auth/login -X POST -H "Content-Type: application/json" -d "{}"'

run_test "Malformed JSON" \
    'curl -s -k $BASE_URL/api/auth/login -X POST -H "Content-Type: application/json" -d "{\"username\":\"admin"'

run_test "Invalid JSON" \
    'curl -s -k $BASE_URL/api/auth/login -X POST -H "Content-Type: application/json" -d "not json at all"'

echo
echo "=== 2. Rapid Sequential Error Tests ==="
for i in {1..10}; do
    run_test "Rapid wrong password #$i" \
        'curl -s -k $BASE_URL/api/auth/login -X POST -H "Content-Type: application/json" -d "{\"username\":\"admin\",\"password\":\"wrong'$i'\"}"'
done

echo
echo "=== 3. Mixed Success/Error Pattern Tests ==="
# Get a valid token first
TOKEN=$(curl -s -k $BASE_URL/api/auth/login -X POST -H "Content-Type: application/json" -d '{"username":"admin","password":"secure123456789"}' | jq -r .token)
run_test "Successful login" 'true'  # Already done above

run_test "Wrong password after success" \
    'curl -s -k $BASE_URL/api/auth/login -X POST -H "Content-Type: application/json" -d "{\"username\":\"admin\",\"password\":\"wrongpass\"}"'

run_test "Valid API call with token" \
    'curl -s -k $BASE_URL/api/health -H "Authorization: Bearer '$TOKEN'"'

run_test "Invalid token API call" \
    'curl -s -k $BASE_URL/api/libraries -H "Authorization: Bearer invalid_token_here"'

run_test "Another successful login" \
    'curl -s -k $BASE_URL/api/auth/login -X POST -H "Content-Type: application/json" -d "{\"username\":\"admin\",\"password\":\"secure123456789\"}"'

echo
echo "=== 4. Various HTTP Error Tests ==="
run_test "404 Not Found" \
    'curl -s -k $BASE_URL/api/nonexistent/endpoint'

run_test "405 Method Not Allowed" \
    'curl -s -k $BASE_URL/api/auth/login -X GET'

run_test "400 Bad Request - No body" \
    'curl -s -k $BASE_URL/api/auth/login -X POST -H "Content-Type: application/json"'

run_test "400 Bad Request - Wrong content type" \
    'curl -s -k $BASE_URL/api/auth/login -X POST -H "Content-Type: text/plain" -d "not json"'

echo
echo "=== 5. Stress Test - 50 Random Errors ==="
for i in {1..50}; do
    # Randomly pick an error scenario
    case $((RANDOM % 6)) in
        0) run_test "Stress: Wrong password #$i" \
            'curl -s -k $BASE_URL/api/auth/login -X POST -H "Content-Type: application/json" -d "{\"username\":\"admin\",\"password\":\"wrong'$i'\"}"'
            ;;
        1) run_test "Stress: Malformed JSON #$i" \
            'curl -s -k $BASE_URL/api/auth/login -X POST -H "Content-Type: application/json" -d "{\"incomplete"'
            ;;
        2) run_test "Stress: 404 #$i" \
            'curl -s -k $BASE_URL/api/random/endpoint'$i''
            ;;
        3) run_test "Stress: Invalid token #$i" \
            'curl -s -k $BASE_URL/api/documents -H "Authorization: Bearer invalid'$i'"'
            ;;
        4) run_test "Stress: Empty body #$i" \
            'curl -s -k $BASE_URL/api/documents -X POST -H "Content-Type: application/json" -H "Authorization: Bearer '$TOKEN'"'
            ;;
        5) run_test "Stress: Method not allowed #$i" \
            'curl -s -k $BASE_URL/api/auth/login -X DELETE'
            ;;
    esac
done

echo
echo "=== 6. Concurrent Error Tests ==="
echo "Launching 20 concurrent error requests..."
for i in {1..20}; do
    curl -s -k $BASE_URL/api/auth/login -X POST -H "Content-Type: application/json" -d "{\"username\":\"admin\",\"password\":\"concurrent$i\"}" &
done
wait
sleep 1
if check_server; then
    echo "✅ Server survived concurrent errors"
    ((PASS++))
else
    echo "❌ Server crashed during concurrent errors"
    ((FAIL++))
fi

echo
echo "=== Final Server Status ==="
check_server

echo
echo "=== Test Summary ==="
echo "Total tests: $((PASS + FAIL))"
echo "Passed: $PASS"
echo "Failed: $FAIL"
echo
if [ $FAIL -eq 0 ]; then
    echo "✅ ALL TESTS PASSED! Server is stable."
else
    echo "❌ Some tests failed. Server needs investigation."
fi