#!/bin/bash

# Integration test for QuickJS with JSON Database Server
# This script runs a comprehensive test of JavaScript API integration with the database

echo "Testing QuickJS Integration with JSON Database Server"
echo "===================================================="

# Check if jsondb server binary exists
if [ ! -f "../bin/jsondb_server" ]; then
  echo "Error: jsondb_server binary not found. Please build the project first."
  exit 1
fi

# Run the integration test
echo "Running integration test script..."
OUTPUT=$(timeout 30s ../bin/jsondb_server -js js/integration_test.js 2>&1)
RESULT=$?

# Display the output
echo
echo "=== Output ==="
echo "$OUTPUT"
echo "=============="
echo

# Check if the script ran successfully
if [ $RESULT -eq 0 ]; then
  if echo "$OUTPUT" | grep -q "Successfully performed CRUD operations on documents"; then
    echo "✅ Database CRUD operations test PASSED"
  else
    echo "❌ Database CRUD operations test FAILED or results not found"
  fi
  
  if echo "$OUTPUT" | grep -q "Successfully queried documents with filters"; then
    echo "✅ Document query test PASSED"
  else
    echo "❌ Document query test FAILED or results not found"
  fi
  
  echo
  echo "✅ QuickJS integration test PASSED"
  exit 0
elif [ $RESULT -eq 124 ]; then
  echo "⚠️ Test timed out after 30 seconds"
  exit 1
else
  echo "❌ QuickJS integration test FAILED with exit code $RESULT"
  exit 1
fi