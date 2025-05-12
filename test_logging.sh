#!/bin/bash
# Test script to exercise the system with logging
# This script tests various operations and components using different log levels

# Colors for better visualization
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[0;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${GREEN}=========================================================${NC}"
echo -e "${GREEN}      JSON Database Server Logging Test Script            ${NC}"
echo -e "${GREEN}=========================================================${NC}"

# Create test directories if they don't exist
mkdir -p ./test_logs
TEST_DB_PATH="./test_logs/test_db.json"
TEST_CONFIG_PATH="./test_logs/test_config.json"
TEST_LOG_PATH="./test_logs/server.log"

# Make sure log file exists and is writable
touch $TEST_LOG_PATH
chmod 666 $TEST_LOG_PATH

# Function to run a test with specified log level
run_test() {
    local test_name=$1
    local log_level=$2
    local test_cmd=$3

    echo -e "\n${BLUE}Test: ${test_name} (Log Level: ${log_level})${NC}"
    echo -e "${YELLOW}Command: ${test_cmd}${NC}\n"
    
    # Run command with specified log level
    eval $test_cmd
    
    # Display tail of log file
    echo -e "\n${BLUE}Log Output (last 10 lines):${NC}"
    tail -n 10 $TEST_LOG_PATH
    
    echo -e "\n${GREEN}Test Complete${NC}"
    echo -e "${GREEN}----------------------------------------${NC}"
}

# Clean start - remove old test files
rm -f $TEST_DB_PATH $TEST_CONFIG_PATH $TEST_LOG_PATH

# Test 1: Basic database operations with ERROR log level
cat > $TEST_CONFIG_PATH <<EOL
{
  "database": {
    "path": "${TEST_DB_PATH}"
  },
  "server": {
    "port": 5000,
    "host": "localhost"
  }
}
EOL

TEST_CMD_1="./bin/jsondb_server -js ./test_logs/test1.js -log ${TEST_LOG_PATH} -log-level error"

cat > ./test_logs/test1.js <<EOL
// Test script for database operations
console.log("Creating test collections");

// Create collections
db.create_collection("users");
db.create_collection("products");

// Insert documents
console.log("Inserting test documents");
db.insert_document("users", {
  "id": "user1",
  "name": "Test User",
  "email": "test@example.com",
  "age": 30
});

db.insert_document("products", {
  "id": "product1",
  "name": "Test Product",
  "price": 99.99,
  "in_stock": true
});

// Query documents
console.log("Querying documents");
var user = db.get_document("users", "user1");
console.log(JSON.stringify(user));

var product = db.get_document("products", "product1");
console.log(JSON.stringify(product));

// Update a document
console.log("Updating document");
db.update_document("users", "user1", {
  "id": "user1",
  "name": "Updated User",
  "email": "updated@example.com",
  "age": 31
});

// Query again to verify update
user = db.get_document("users", "user1");
console.log(JSON.stringify(user));

// Test complete
console.log("Basic database operations test complete");
EOL

run_test "Basic Database Operations" "ERROR" "$TEST_CMD_1"

# Test 2: Index operations with DEBUG log level
cat > ./test_logs/test2.js <<EOL
// Test script for index operations
console.log("Creating test indices");

// Create collections if they don't exist
if (!db.get_collection("users")) {
  db.create_collection("users");
}

if (!db.get_collection("products")) {
  db.create_collection("products");
}

// Insert more test documents
for (var i = 1; i <= 10; i++) {
  db.insert_document("users", {
    "id": "user" + i,
    "name": "User " + i,
    "email": "user" + i + "@example.com",
    "age": 20 + i
  });
  
  db.insert_document("products", {
    "id": "product" + i,
    "name": "Product " + i,
    "category": i % 3 == 0 ? "A" : (i % 3 == 1 ? "B" : "C"),
    "price": 10.0 * i,
    "in_stock": i % 2 == 0
  });
}

// Create indexes
console.log("Creating indexes");
db.create_index("users", "age_index", "age", "non_unique");
db.create_index("users", "email_index", "email", "unique");
db.create_index("products", "category_index", "category", "non_unique");
db.create_index("products", "price_index", "price", "non_unique");

// List indexes
console.log("Listing indexes");
var user_indexes = db.list_indexes("users");
console.log(JSON.stringify(user_indexes));

var product_indexes = db.list_indexes("products");
console.log(JSON.stringify(product_indexes));

// Query using indexes
console.log("Querying with indexes");
var users = db.query_documents("users", { "age": { "$gt": 25 } });
console.log("Users with age > 25: " + users.length);

var products = db.query_documents("products", { "category": "A" });
console.log("Products in category A: " + products.length);

// Get index statistics
console.log("Index statistics");
var age_index_stats = db.index_stats("users", "age_index");
console.log(JSON.stringify(age_index_stats));

// Drop an index
console.log("Dropping index");
db.drop_index("products", "price_index");

// Verify index was dropped
product_indexes = db.list_indexes("products");
console.log(JSON.stringify(product_indexes));

// Test complete
console.log("Index operations test complete");
EOL

TEST_CMD_2="./bin/jsondb_server -js ./test_logs/test2.js -log ${TEST_LOG_PATH} -log-level debug"

run_test "Index Operations" "DEBUG" "$TEST_CMD_2"

# Test 3: Transaction operations with INFO log level
cat > ./test_logs/test3.js <<EOL
// Test script for transaction operations
console.log("Testing transactions");

// Begin transaction
var tx = db.transaction_begin();
console.log("Transaction ID: " + tx.id);

// Perform operations within transaction
console.log("Inserting document in transaction");
db.transaction_insert(tx, "users", {
  "id": "txuser1",
  "name": "Transaction User",
  "email": "tx@example.com",
  "age": 40
});

console.log("Updating document in transaction");
db.transaction_update(tx, "products", "product1", {
  "id": "product1",
  "name": "Updated in Transaction",
  "price": 199.99,
  "in_stock": false
});

// Commit transaction
console.log("Committing transaction");
db.transaction_commit(tx);

// Verify committed changes
var txuser = db.get_document("users", "txuser1");
console.log(JSON.stringify(txuser));

var product = db.get_document("products", "product1");
console.log(JSON.stringify(product));

// Test rollback
console.log("Testing transaction rollback");
var tx2 = db.transaction_begin();
console.log("Transaction ID: " + tx2.id);

// Perform operations within transaction
db.transaction_insert(tx2, "users", {
  "id": "txuser2",
  "name": "Rollback User",
  "email": "rollback@example.com",
  "age": 50
});

// Rollback transaction
console.log("Rolling back transaction");
db.transaction_rollback(tx2);

// Verify rolled back changes (should not exist)
var txuser2 = db.get_document("users", "txuser2");
console.log("txuser2 exists: " + (txuser2 !== null));

// Test complete
console.log("Transaction operations test complete");
EOL

TEST_CMD_3="./bin/jsondb_server -js ./test_logs/test3.js -log ${TEST_LOG_PATH} -log-level info"

run_test "Transaction Operations" "INFO" "$TEST_CMD_3"

# Test 4: Query operations with TRACE log level
cat > ./test_logs/test4.js <<EOL
// Test script for complex query operations
console.log("Testing complex queries");

// Insert more test data if needed
for (var i = 11; i <= 20; i++) {
  db.insert_document("users", {
    "id": "user" + i,
    "name": "User " + i,
    "email": "user" + i + "@example.com",
    "age": 20 + i,
    "tags": ["tag" + (i % 3), "tag" + (i % 5)],
    "address": {
      "city": i % 2 == 0 ? "New York" : "San Francisco",
      "country": "USA"
    }
  });
}

// Simple query
console.log("Simple query");
var users = db.query_documents("users", { "age": { "$gt": 30 } });
console.log("Users with age > 30: " + users.length);

// Complex query with logical operators
console.log("Complex query with logical operators");
var complex_query = {
  "$and": [
    { "age": { "$gte": 25 } },
    { "age": { "$lte": 40 } },
    { "$or": [
      { "address.city": "New York" },
      { "tags": { "$in": ["tag1"] } }
    ]}
  ]
};
var complex_results = db.query_documents("users", complex_query);
console.log("Complex query results: " + complex_results.length);

// Query with projection
console.log("Query with projection");
var projection_query = {
  "query": { "age": { "$gt": 30 } },
  "projection": { "name": 1, "email": 1, "age": 1 }
};
var projection_results = db.query_documents("users", projection_query);
console.log("Projection results: " + projection_results.length);
console.log(JSON.stringify(projection_results[0]));

// Query with sort
console.log("Query with sort");
var sort_query = {
  "query": { "age": { "$gt": 25 } },
  "sort": { "age": -1 }  // Sort by age descending
};
var sort_results = db.query_documents("users", sort_query);
console.log("Sort results: " + sort_results.length);
console.log("First result age (should be highest): " + sort_results[0].age);

// Query with pagination
console.log("Query with pagination");
var page_query = {
  "query": { "age": { "$gt": 20 } },
  "sort": { "age": 1 },
  "limit": 5,
  "skip": 5
};
var page_results = db.query_documents("users", page_query);
console.log("Page results: " + page_results.length);
console.log("First result on page: " + JSON.stringify(page_results[0]));

// Test complete
console.log("Query operations test complete");
EOL

TEST_CMD_4="./bin/jsondb_server -js ./test_logs/test4.js -log ${TEST_LOG_PATH} -log-level trace"

run_test "Complex Query Operations" "TRACE" "$TEST_CMD_4"

echo -e "\n${GREEN}=========================================================${NC}"
echo -e "${GREEN}    All tests completed! Log file: ${TEST_LOG_PATH}        ${NC}"
echo -e "${GREEN}=========================================================${NC}"

# Analyze log file statistics
echo -e "\n${BLUE}Log File Analysis:${NC}"
echo -e "${YELLOW}Total log lines:${NC} $(wc -l < $TEST_LOG_PATH)"
echo -e "${YELLOW}ERROR log entries:${NC} $(grep -c "\[ERROR\]" $TEST_LOG_PATH)"
echo -e "${YELLOW}WARNING log entries:${NC} $(grep -c "\[WARNING\]" $TEST_LOG_PATH)"
echo -e "${YELLOW}INFO log entries:${NC} $(grep -c "\[INFO\]" $TEST_LOG_PATH)"
echo -e "${YELLOW}DEBUG log entries:${NC} $(grep -c "\[DEBUG\]" $TEST_LOG_PATH)"
echo -e "${YELLOW}TRACE log entries:${NC} $(grep -c "\[TRACE\]" $TEST_LOG_PATH)"

echo -e "\n${GREEN}Most frequent log sources:${NC}"
grep -o "\[[^]]*:[0-9]*:[^]]*\]" $TEST_LOG_PATH | sort | uniq -c | sort -nr | head -5

echo -e "\n${BLUE}Testing complete!${NC}"