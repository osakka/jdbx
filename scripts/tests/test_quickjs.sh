#!/bin/bash

# Test script for QuickJS integration
# This script runs a JavaScript test file to verify the QuickJS integration is working properly

echo "Testing QuickJS JavaScript Engine Integration"
echo "============================================="

# Check if jsondb_server binary exists
if [ ! -f "../bin/jsondb_server" ]; then
  echo "Error: jsondb_server binary not found. Please build the project first."
  exit 1
fi

# Set the environment for testing
export JSONDB_TEST_MODE=1
export JSONDB_LOG_LEVEL=debug

# Run the JavaScript test file with a timeout
echo "Running JavaScript test file..."
timeout 30s ../bin/jsondb_server -js js/test_quickjs.js

# Check the exit code
RESULT=$?
if [ $RESULT -eq 0 ]; then
  echo "✅ QuickJS integration test PASSED"
  exit 0
elif [ $RESULT -eq 124 ]; then
  echo "⚠️ Test timed out after 30 seconds. This might indicate the server doesn't exit properly after running a JavaScript file."
  echo "However, if output was produced, the JavaScript integration itself might still be working."
  exit 1
else
  echo "❌ QuickJS integration test FAILED with exit code $RESULT"
  exit 1
fi