#!/bin/bash

# Test script for transaction isolation levels
# This script tests the different isolation levels in the JSON database.

# Set the server URL
SERVER_URL="http://localhost:8080"

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Function to print success or failure
print_result() {
    local test_name="$1"
    local status="$2"
    
    if [ "$status" -eq 0 ]; then
        echo -e "${GREEN}✓ $test_name passed${NC}"
    else
        echo -e "${RED}✗ $test_name failed${NC}"
    fi
}

# Function to print section header
print_section() {
    echo -e "\n${YELLOW}========== $1 ==========${NC}"
}

# Create a test collection
create_test_collection() {
    echo "Creating test collection..."
    curl -s -X POST "$SERVER_URL/api/collections" \
         -H "Content-Type: application/json" \
         -d '{"name": "test_transactions", "schema": {}}' > /dev/null
}

# Drop the test collection
drop_test_collection() {
    echo "Dropping test collection..."
    curl -s -X DELETE "$SERVER_URL/api/collections/test_transactions" > /dev/null
}

# Clean up before running tests
cleanup() {
    drop_test_collection
    create_test_collection
}

# Begin a transaction with the specified isolation level
begin_transaction() {
    local isolation_level="$1"
    local response
    
    response=$(curl -s -X POST "$SERVER_URL/api/transactions" \
                    -H "Content-Type: application/json" \
                    -d "{\"isolation_level\": \"$isolation_level\"}")
    
    echo "$response" | grep -o '"transaction_id":"[^"]*"' | awk -F'"' '{print $4}'
}

# Commit a transaction
commit_transaction() {
    local tx_id="$1"
    
    curl -s -X POST "$SERVER_URL/api/transactions/$tx_id/commit" \
         -H "Content-Type: application/json" \
         -d '{}' > /dev/null
}

# Rollback a transaction
rollback_transaction() {
    local tx_id="$1"
    
    curl -s -X DELETE "$SERVER_URL/api/transactions/$tx_id/rollback" \
         -H "Content-Type: application/json" \
         -d '{}' > /dev/null
}

# Insert a document within a transaction
insert_document() {
    local tx_id="$1"
    local doc="$2"
    
    curl -s -X POST "$SERVER_URL/api/transactions/$tx_id/collections/test_transactions/documents" \
         -H "Content-Type: application/json" \
         -d "$doc" > /dev/null
}

# Update a document within a transaction
update_document() {
    local tx_id="$1"
    local doc_id="$2"
    local doc="$3"
    
    curl -s -X PUT "$SERVER_URL/api/transactions/$tx_id/collections/test_transactions/documents/$doc_id" \
         -H "Content-Type: application/json" \
         -d "$doc" > /dev/null
}

# Delete a document within a transaction
delete_document() {
    local tx_id="$1"
    local doc_id="$2"
    
    curl -s -X DELETE "$SERVER_URL/api/transactions/$tx_id/collections/test_transactions/documents/$doc_id" \
         -H "Content-Type: application/json" \
         -d '{}' > /dev/null
}

# Query documents within a transaction
query_documents() {
    local tx_id="$1"
    local query="$2"
    
    curl -s -X GET "$SERVER_URL/api/transactions/$tx_id/collections/test_transactions/documents?query=$query" \
         -H "Content-Type: application/json"
}

# Set transaction isolation level
set_isolation_level() {
    local tx_id="$1"
    local isolation_level="$2"
    
    curl -s -X PATCH "$SERVER_URL/api/transactions/$tx_id/isolation" \
         -H "Content-Type: application/json" \
         -d "{\"isolation_level\": \"$isolation_level\"}" > /dev/null
}

# Set transaction timeout
set_timeout() {
    local tx_id="$1"
    local timeout_sec="$2"
    
    curl -s -X PATCH "$SERVER_URL/api/transactions/$tx_id/timeout" \
         -H "Content-Type: application/json" \
         -d "{\"timeout_sec\": $timeout_sec}" > /dev/null
}

# Test read uncommitted isolation level
test_read_uncommitted() {
    print_section "Testing READ_UNCOMMITTED Isolation Level"
    
    # Begin two transactions with READ_UNCOMMITTED isolation level
    local tx1=$(begin_transaction "read_uncommitted")
    local tx2=$(begin_transaction "read_uncommitted")
    
    echo "Transaction 1 ID: $tx1"
    echo "Transaction 2 ID: $tx2"
    
    # Insert a document in transaction 1 (but don't commit yet)
    insert_document "$tx1" '{"name": "test_doc", "value": 42}'
    
    # Query documents in transaction 2 (should see uncommitted changes)
    local result=$(query_documents "$tx2" '{"name": "test_doc"}')
    local count=$(echo "$result" | grep -o '"name":"test_doc"' | wc -l)
    
    if [ "$count" -eq 1 ]; then
        print_result "READ_UNCOMMITTED can see uncommitted changes" 0
    else
        print_result "READ_UNCOMMITTED can see uncommitted changes" 1
    fi
    
    # Rollback transaction 1
    rollback_transaction "$tx1"
    
    # Query documents in transaction 2 again (should no longer see the rolled back changes)
    result=$(query_documents "$tx2" '{"name": "test_doc"}')
    count=$(echo "$result" | grep -o '"name":"test_doc"' | wc -l)
    
    if [ "$count" -eq 0 ]; then
        print_result "READ_UNCOMMITTED doesn't see rolled back changes" 0
    else
        print_result "READ_UNCOMMITTED doesn't see rolled back changes" 1
    fi
    
    # Cleanup
    commit_transaction "$tx2"
}

# Test read committed isolation level
test_read_committed() {
    print_section "Testing READ_COMMITTED Isolation Level"
    
    # Begin two transactions with READ_COMMITTED isolation level
    local tx1=$(begin_transaction "read_committed")
    local tx2=$(begin_transaction "read_committed")
    
    echo "Transaction 1 ID: $tx1"
    echo "Transaction 2 ID: $tx2"
    
    # Insert a document in transaction 1 (but don't commit yet)
    insert_document "$tx1" '{"name": "test_doc_committed", "value": 42}'
    
    # Query documents in transaction 2 (should NOT see uncommitted changes)
    local result=$(query_documents "$tx2" '{"name": "test_doc_committed"}')
    local count=$(echo "$result" | grep -o '"name":"test_doc_committed"' | wc -l)
    
    if [ "$count" -eq 0 ]; then
        print_result "READ_COMMITTED doesn't see uncommitted changes" 0
    else
        print_result "READ_COMMITTED doesn't see uncommitted changes" 1
    fi
    
    # Commit transaction 1
    commit_transaction "$tx1"
    
    # Query documents in transaction 2 again (should now see the committed changes)
    result=$(query_documents "$tx2" '{"name": "test_doc_committed"}')
    count=$(echo "$result" | grep -o '"name":"test_doc_committed"' | wc -l)
    
    if [ "$count" -eq 1 ]; then
        print_result "READ_COMMITTED can see committed changes" 0
    else
        print_result "READ_COMMITTED can see committed changes" 1
    fi
    
    # Cleanup
    commit_transaction "$tx2"
}

# Test serializable isolation level
test_serializable() {
    print_section "Testing SERIALIZABLE Isolation Level"
    
    # Begin two transactions with SERIALIZABLE isolation level
    local tx1=$(begin_transaction "serializable")
    local tx2=$(begin_transaction "serializable")
    
    echo "Transaction 1 ID: $tx1"
    echo "Transaction 2 ID: $tx2"
    
    # Insert a document in transaction 1
    insert_document "$tx1" '{"name": "test_doc_serializable", "value": 42}'
    
    # Try to update the same document in transaction 2 (should fail due to locking)
    update_document "$tx2" "test_doc_serializable" '{"name": "test_doc_serializable", "value": 43}'
    
    # Query documents in transaction 2 (should not see uncommitted changes)
    local result=$(query_documents "$tx2" '{"name": "test_doc_serializable"}')
    local count=$(echo "$result" | grep -o '"name":"test_doc_serializable"' | wc -l)
    
    if [ "$count" -eq 0 ]; then
        print_result "SERIALIZABLE provides proper isolation" 0
    else
        print_result "SERIALIZABLE provides proper isolation" 1
    fi
    
    # Commit transaction 1
    commit_transaction "$tx1"
    
    # Now transaction 2 should see the committed changes
    result=$(query_documents "$tx2" '{"name": "test_doc_serializable"}')
    count=$(echo "$result" | grep -o '"name":"test_doc_serializable"' | wc -l)
    
    if [ "$count" -eq 1 ]; then
        print_result "SERIALIZABLE can see committed changes" 0
    else
        print_result "SERIALIZABLE can see committed changes" 1
    fi
    
    # Cleanup
    commit_transaction "$tx2"
}

# Test transaction timeout
test_transaction_timeout() {
    print_section "Testing Transaction Timeout"
    
    # Begin a transaction
    local tx=$(begin_transaction "read_committed")
    
    echo "Transaction ID: $tx"
    
    # Set a short timeout (5 seconds)
    set_timeout "$tx" 5
    
    # Insert a document
    insert_document "$tx" '{"name": "test_doc_timeout", "value": 42}'
    
    # Wait for the timeout
    echo "Waiting for transaction timeout (5 seconds)..."
    sleep 6
    
    # Try to commit the transaction (should fail due to timeout)
    local result=$(commit_transaction "$tx")
    
    # Check if the transaction was auto-aborted
    local status=$(curl -s -X GET "$SERVER_URL/api/transactions/$tx" \
                        -H "Content-Type: application/json" | grep -o '"status":"[^"]*"' | awk -F'"' '{print $4}')
    
    if [ "$status" = "aborted" ]; then
        print_result "Transaction timeout works correctly" 0
    else
        print_result "Transaction timeout works correctly" 1
    fi
}

# Test changing isolation level
test_changing_isolation_level() {
    print_section "Testing Changing Isolation Level"
    
    # Begin a transaction with READ_UNCOMMITTED
    local tx=$(begin_transaction "read_uncommitted")
    
    echo "Transaction ID: $tx"
    
    # Change isolation level to SERIALIZABLE
    set_isolation_level "$tx" "serializable"
    
    # Check the isolation level
    local level=$(curl -s -X GET "$SERVER_URL/api/transactions/$tx" \
                      -H "Content-Type: application/json" | grep -o '"isolation_level":"[^"]*"' | awk -F'"' '{print $4}')
    
    if [ "$level" = "serializable" ]; then
        print_result "Changing isolation level works correctly" 0
    else
        print_result "Changing isolation level works correctly" 1
    fi
    
    # Cleanup
    commit_transaction "$tx"
}

# Run all tests
run_all_tests() {
    cleanup
    test_read_uncommitted
    cleanup
    test_read_committed
    cleanup
    test_serializable
    cleanup
    test_transaction_timeout
    cleanup
    test_changing_isolation_level
    cleanup
}

# Main function
main() {
    echo "Starting transaction isolation tests..."
    run_all_tests
    echo -e "\nAll tests completed."
}

# Run the main function
main