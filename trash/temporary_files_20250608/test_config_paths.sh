#!/bin/bash
# Test script to verify path configuration changes

echo "Testing JSONdb path configuration..."
echo "===================================="

# Test 1: Default behavior (no environment set)
echo -e "\nTest 1: Default behavior (no JSONDB_BASE_PATH set)"
unset JSONDB_BASE_PATH
unset JSONDB_BASE_DIR
cd /opt/jsondb/build
./jsondb_runtime.sh status

# Test 2: Using JSONDB_BASE_PATH
echo -e "\nTest 2: Using JSONDB_BASE_PATH=/opt/jsondb"
export JSONDB_BASE_PATH="/opt/jsondb"
./jsondb_runtime.sh status

# Test 3: Using custom base path
echo -e "\nTest 3: Using custom JSONDB_BASE_PATH=/tmp/jsondb"
export JSONDB_BASE_PATH="/tmp/jsondb"
./jsondb_runtime.sh status

# Test 4: Backward compatibility with JSONDB_BASE_DIR
echo -e "\nTest 4: Backward compatibility test (JSONDB_BASE_DIR)"
unset JSONDB_BASE_PATH
export JSONDB_BASE_DIR="/opt/jsondb"
./jsondb_runtime.sh status

echo -e "\nPath configuration tests completed."