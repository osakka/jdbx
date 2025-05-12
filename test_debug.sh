#!/bin/bash
# Debug script to check logging implementation

# Create test directory and ensure log file exists
mkdir -p test_logs
TEST_LOG="./test_logs/debug.log"
touch $TEST_LOG
chmod 666 $TEST_LOG

echo "Testing logging with DEBUG level..."
./bin/jsondb_server -log $TEST_LOG -log-level debug -js ./tests/js/logging_test.js

echo "Checking if log file contains entries..."
if [ -s "$TEST_LOG" ]; then
    echo "SUCCESS: Log file contains data!"
    echo "Log file size: $(du -h $TEST_LOG | cut -f1)"
    echo "Number of log lines: $(wc -l < $TEST_LOG)"
    
    echo "Sample log entries:"
    head -n 5 $TEST_LOG
    echo "..."
    tail -n 5 $TEST_LOG
    
    echo "Log statistics by level:"
    echo "ERROR: $(grep -c '\[ERROR\]' $TEST_LOG)"
    echo "WARNING: $(grep -c '\[WARNING\]' $TEST_LOG)"
    echo "INFO: $(grep -c '\[INFO\]' $TEST_LOG)"
    echo "DEBUG: $(grep -c '\[DEBUG\]' $TEST_LOG)"
    echo "TRACE: $(grep -c '\[TRACE\]' $TEST_LOG)"
    
    echo "Log entries by component:"
    grep -o '\[[^:]*' $TEST_LOG | sort | uniq -c | sort -nr | head -10
else
    echo "FAILURE: Log file is empty!"
    
    # Debug information to help identify the issue
    echo "Server binary details:"
    ls -la ./bin/jsondb_server
    echo "Checking server binary for symbols:"
    nm -D ./bin/jsondb_server | grep -i logger
    
    echo "Checking for running processes:"
    ps aux | grep jsondb
fi