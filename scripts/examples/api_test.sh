#!/bin/bash
# API Test Script for JSONdb
# This script tests various API endpoints

SERVER_URL="http://localhost:5000"

# Function to print colored output
print_colored() {
  local color=$1
  local text=$2
  case $color in
    "green") echo -e "\033[0;32m$text\033[0m" ;;
    "red") echo -e "\033[0;31m$text\033[0m" ;;
    "yellow") echo -e "\033[0;33m$text\033[0m" ;;
    "blue") echo -e "\033[0;34m$text\033[0m" ;;
    *) echo "$text" ;;
  esac
}

# Function to print test header
test_header() {
  echo ""
  print_colored "blue" "======================================"
  print_colored "blue" "Testing: $1"
  print_colored "blue" "======================================"
}

# Function to run a test and display result
run_test() {
  local description=$1
  local command=$2
  
  echo ""
  print_colored "yellow" "-> $description"
  echo "$command"
  echo ""
  
  # Run the command
  eval $command
  
  if [ $? -eq 0 ]; then
    print_colored "green" "Test completed."
  else
    print_colored "red" "Test failed!"
  fi
}

# Test Authentication
test_header "Authentication"

run_test "Login with default credentials" "curl -s -X POST -H 'Content-Type: application/json' -d '{\"username\":\"admin\",\"password\":\"admin\"}' $SERVER_URL/api/auth/login"

# Store token from login for subsequent requests
TOKEN=$(curl -s -X POST -H "Content-Type: application/json" -d '{"username":"admin","password":"admin"}' $SERVER_URL/api/auth/login | grep -o '"token":"[^"]*"' | cut -d'"' -f4)

if [ -n "$TOKEN" ]; then
  print_colored "green" "Token received: ${TOKEN:0:20}..."
else
  print_colored "red" "No token received. Subsequent authenticated tests may fail."
fi

# Test Collections
test_header "Collections"

run_test "Create a test collection" "curl -s -X POST -H 'Content-Type: application/json' -H 'Authorization: Bearer $TOKEN' -d '{\"name\":\"test_collection\"}' $SERVER_URL/api/collections"

run_test "List all collections" "curl -s -H 'Authorization: Bearer $TOKEN' $SERVER_URL/api/collections"

# Test Documents
test_header "Documents"

run_test "Insert a document" "curl -s -X POST -H 'Content-Type: application/json' -H 'Authorization: Bearer $TOKEN' -d '{\"name\":\"Test Document\",\"value\":42}' $SERVER_URL/api/collections/test_collection/documents"

run_test "Query documents" "curl -s -H 'Authorization: Bearer $TOKEN' $SERVER_URL/api/collections/test_collection/documents"

# Clean up
test_header "Cleanup"

run_test "Delete the test collection" "curl -s -X DELETE -H 'Authorization: Bearer $TOKEN' $SERVER_URL/api/collections/test_collection"

echo ""
print_colored "blue" "======================================"
print_colored "blue" "API Tests Completed"
print_colored "blue" "======================================"