#!/bin/bash
#
# JavaScript Integration Test for JSON Database Server
#
# This script runs the JSON Database Server and executes a series of
# JavaScript tests to verify integration.

set -e

# Get current directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Build the server if needed
if [ ! -f "bin/jsondb_server" ]; then
  echo "Building JSON Database Server..."
  make clean
  make
fi

# Create test directories if they don't exist
mkdir -p var/log/jsondb
mkdir -p var/data/jsondb
mkdir -p var/run

# Check if server is running and stop it
echo "Checking for existing server..."
bin/jsondb_server -stop 2>/dev/null || true
sleep 1

# Start server
echo "Starting JSON Database Server..."
bin/jsondb_server -daemon

# Wait for server to start
sleep 2

# Execute JavaScript tests
echo "Running JavaScript tests..."

# Test 1: Minimal test
echo "Test 1: Minimal test"
bin/jsondb_server -js_eval "42;"
echo

# Test 2: Basic database operations
echo "Test 2: Basic database operations"
bin/jsondb_server -js_eval "
const testCollection = 'test_collection';
const testDoc = { id: 'test1', name: 'Test Document', value: 42 };

// Insert document
const insertResult = db.insertDocument(testCollection, testDoc);

// Query document
const queryResult = db.getDocument(testCollection, 'test1');

// Check if query succeeded
if (queryResult && queryResult.name === 'Test Document' && queryResult.value === 42) {
  return { success: true, message: 'Database operations succeeded' };
} else {
  return { success: false, message: 'Database operations failed' };
}
"
echo

# Test 3: Run integration test file
echo "Test 3: Run integration test file"
bin/jsondb_server -js_file tests/js/integration_test.js
echo

# Test 4: Run test_quickjs.js
echo "Test 4: Run test_quickjs.js"
bin/jsondb_server -js_file tests/js/test_quickjs.js
echo

# Stop server
echo "Stopping JSON Database Server..."
bin/jsondb_server -stop

echo "JavaScript integration tests completed successfully!"