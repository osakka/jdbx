#!/bin/bash

# Simple test for QuickJS integration
# This script runs a minimal JavaScript file to verify basic functionality

echo "Testing QuickJS Basic Functionality"
echo "=================================="

# Run the JavaScript hello world test
echo "Running hello_world.js..."
OUTPUT=$(timeout 10s ../bin/jsondb_server -js js/hello_world.js 2>&1)
RESULT=$?

# Display the output
echo
echo "=== Output ==="
echo "$OUTPUT"
echo "=============="
echo

# Check for expected output
if echo "$OUTPUT" | grep -q "Hello from QuickJS"; then
  echo "✅ Found 'Hello from QuickJS' in output"
else
  echo "❌ Did not find 'Hello from QuickJS' in output"
fi

if echo "$OUTPUT" | grep -q "JSON test:"; then
  echo "✅ Found JSON test output"
else
  echo "❌ Did not find JSON test output"
fi

# Check the return code
if [ $RESULT -eq 0 ]; then
  echo "✅ Command exited successfully"
elif [ $RESULT -eq 124 ]; then
  echo "⚠️ Command timed out after 10 seconds"
else
  echo "❌ Command failed with exit code $RESULT"
fi

# Final assessment
if echo "$OUTPUT" | grep -q "Hello from QuickJS" && [ $RESULT -ne 124 ]; then
  echo
  echo "✅ QuickJS integration appears to be working"
  exit 0
else
  echo
  echo "❌ QuickJS integration test failed"
  exit 1
fi