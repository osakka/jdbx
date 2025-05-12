// Test script to exercise logging functionality
console.log("Starting logging test");

// Test database operations
console.log("Testing database operations...");
db.create_collection("test_collection");

// Insert a document
const testDoc = {
  id: "test1",
  name: "Test Document",
  value: 42,
  active: true
};
db.insert_document("test_collection", testDoc);

// Retrieve document
const doc = db.get_document("test_collection", "test1");
console.log("Retrieved document:", JSON.stringify(doc));

// Update document
testDoc.value = 100;
db.update_document("test_collection", "test1", testDoc);

// Test index operations
console.log("Testing index operations...");
db.create_index("test_collection", "value_index", "value", "non_unique");

// Test transaction operations
console.log("Testing transaction operations...");
const tx = db.transaction_begin();
db.transaction_insert(tx, "test_collection", {
  id: "tx_doc",
  name: "Transaction Test",
  value: 200
});
db.transaction_commit(tx);

// Query operations
console.log("Testing query operations...");
const results = db.query_documents("test_collection", {
  value: { "$gt": 50 }
});
console.log(`Found ${results.length} documents with value > 50`);

console.log("Logging test completed");