#!/bin/bash

# Minimal test for QuickJS integration
echo "Testing QuickJS with minimal script"
echo "=================================="

# Run the minimal JavaScript test
echo "Running minimal.js..."
OUTPUT=$(../bin/jsondb_server -js js/minimal.js 2>&1)
RESULT=$?

# Display the output
echo
echo "=== Output ==="
echo "$OUTPUT"
echo "=============="
echo

# Check the return code
if [ $RESULT -eq 0 ]; then
  echo "✅ Command exited successfully"
else
  echo "❌ Command failed with exit code $RESULT"
fi