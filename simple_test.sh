#!/bin/bash
# Simple test to check if logging is working

# Create test directory
mkdir -p test_logs

# Create a test log file
TEST_LOG="./test_logs/test.log"
touch $TEST_LOG
chmod 666 $TEST_LOG

echo "Running simple database server with logging enabled..."
./bin/jsondb_server -log $TEST_LOG -log-level info -js ./tests/js/minimal.js

echo "Checking log file..."
if [ -s "$TEST_LOG" ]; then
    echo "Log file created successfully!"
    echo "Log file content (last 10 lines):"
    tail -n 10 $TEST_LOG
    
    # Count log entries by level
    echo "Log statistics:"
    echo "ERROR entries: $(grep -c "\[ERROR\]" $TEST_LOG)"
    echo "WARNING entries: $(grep -c "\[WARNING\]" $TEST_LOG)"
    echo "INFO entries: $(grep -c "\[INFO\]" $TEST_LOG)"
    echo "DEBUG entries: $(grep -c "\[DEBUG\]" $TEST_LOG)"
    echo "TRACE entries: $(grep -c "\[TRACE\]" $TEST_LOG)"
else
    echo "Log file is empty or not created!"
fi