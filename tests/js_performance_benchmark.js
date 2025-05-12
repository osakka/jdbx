/**
 * JavaScript Database Performance Benchmark
 * 
 * This script benchmarks the performance of various database operations
 * using the JavaScript API. It measures:
 * - Document insertion speed
 * - Query performance
 * - Update operations
 * - Delete operations
 * - Complex operations (combined queries and updates)
 * 
 * Run with:
 * bin/jsondb_server -js_eval_file tests/js_performance_benchmark.js
 */

// Load database helper functions
load("functions/db_helpers.js");

// Utilities for benchmarking
function runBenchmark(name, count, fn) {
  console.log(`\nRunning benchmark: ${name} (${count} operations)`);
  
  // Warm-up
  for (let i = 0; i < Math.min(count * 0.1, 100); i++) {
    fn(i);
  }
  
  // Run the actual benchmark
  const start = Date.now();
  for (let i = 0; i < count; i++) {
    fn(i);
  }
  const end = Date.now();
  
  // Calculate results
  const totalTime = end - start;
  const opsPerSecond = count / (totalTime / 1000);
  const msPerOp = totalTime / count;
  
  console.log(`✓ Completed in ${totalTime}ms`);
  console.log(`✓ ${opsPerSecond.toFixed(2)} operations/second`);
  console.log(`✓ ${msPerOp.toFixed(4)}ms per operation`);
  
  return {
    name,
    count,
    totalTime,
    opsPerSecond,
    msPerOp
  };
}

// Helper to create random data
function generateRandomData(index) {
  return {
    id: `doc_${index}`,
    name: `Test Document ${index}`,
    value: Math.floor(Math.random() * 1000),
    tags: [
      `tag_${Math.floor(Math.random() * 10)}`,
      `tag_${Math.floor(Math.random() * 10)}`
    ],
    isActive: Math.random() > 0.3,
    created: new Date().toISOString(),
    nested: {
      field1: Math.random() * 100,
      field2: `Nested value ${index}`
    }
  };
}

// Set up test collection
const COLLECTION_NAME = "performance_test";
const DOCUMENT_COUNTS = {
  small: 100,
  medium: 1000,
  large: 10000
};

// Clean up any existing test data
try {
  const testData = db.getCollection(COLLECTION_NAME);
  if (testData && testData.length > 0) {
    console.log(`Cleaning up ${testData.length} existing test documents...`);
    testData.forEach(doc => {
      db.deleteDocument(COLLECTION_NAME, doc.id);
    });
  }
} catch (e) {
  // Collection probably doesn't exist yet
}

// Create collection object
const collection = new Collection(COLLECTION_NAME);

// Results storage
const benchmarkResults = [];

// Benchmark 1: Document Insertion - Small Batch
const insertSmallResult = runBenchmark("Insert Documents (Small Batch)", DOCUMENT_COUNTS.small, (i) => {
  const data = generateRandomData(i);
  collection.insert(data);
});
benchmarkResults.push(insertSmallResult);

// Benchmark 2: Document Queries - Exact Match
const exactQueryResult = runBenchmark("Exact Match Queries", DOCUMENT_COUNTS.small, (i) => {
  const id = `doc_${Math.floor(Math.random() * DOCUMENT_COUNTS.small)}`;
  collection.get(id);
});
benchmarkResults.push(exactQueryResult);

// Benchmark 3: Document Queries - Field Match
const fieldQueryResult = runBenchmark("Field Match Queries", DOCUMENT_COUNTS.small, (i) => {
  const isActive = i % 2 === 0;
  collection.query({ isActive });
});
benchmarkResults.push(fieldQueryResult);

// Benchmark 4: Document Updates
const updateResult = runBenchmark("Document Updates", DOCUMENT_COUNTS.small, (i) => {
  const id = `doc_${Math.floor(Math.random() * DOCUMENT_COUNTS.small)}`;
  const doc = collection.get(id);
  if (doc) {
    doc.updated = new Date().toISOString();
    doc.value = Math.floor(Math.random() * 1000);
    collection.update(id, doc);
  }
});
benchmarkResults.push(updateResult);

// Benchmark 5: Document Deletion and Reinsertion
const deleteInsertResult = runBenchmark("Delete and Reinsert", Math.floor(DOCUMENT_COUNTS.small * 0.5), (i) => {
  const id = `doc_${Math.floor(Math.random() * DOCUMENT_COUNTS.small)}`;
  collection.delete(id);
  collection.insert(generateRandomData(i + DOCUMENT_COUNTS.small));
});
benchmarkResults.push(deleteInsertResult);

// Benchmark 6: Complex Operation (Get-Modify-Update)
const complexOpResult = runBenchmark("Complex Operations", Math.floor(DOCUMENT_COUNTS.small * 0.2), (i) => {
  // Get a random document
  const id = `doc_${Math.floor(Math.random() * DOCUMENT_COUNTS.small)}`;
  const doc = collection.get(id);
  
  if (doc) {
    // Modify multiple fields
    doc.name = `Modified ${doc.name}`;
    doc.value += 10;
    doc.nested.field1 *= 1.1;
    doc.tags.push(`new_tag_${i}`);
    doc.modified = true;
    doc.modifiedDate = new Date().toISOString();
    
    // Update the document
    collection.update(id, doc);
    
    // Query documents with similar properties
    const similarDocs = collection.query({
      value: { $gt: doc.value - 50, $lt: doc.value + 50 }
    });
  }
});
benchmarkResults.push(complexOpResult);

// Now let's do some medium-scale testing
console.log("\n\n=== MEDIUM SCALE TESTING ===");

// Insert more documents to reach medium size
const remainingToInsert = DOCUMENT_COUNTS.medium - DOCUMENT_COUNTS.small;
console.log(`\nInserting ${remainingToInsert} additional documents for medium-scale tests...`);
for (let i = 0; i < remainingToInsert; i++) {
  const data = generateRandomData(i + DOCUMENT_COUNTS.small);
  collection.insert(data);
}

// Benchmark 7: Medium-scale Queries
const mediumQueryResult = runBenchmark("Medium-scale Queries", 100, (i) => {
  const query = {};
  
  // Randomize query type for varied testing
  const queryType = i % 4;
  switch (queryType) {
    case 0:
      // Exact ID match
      const id = `doc_${Math.floor(Math.random() * DOCUMENT_COUNTS.medium)}`;
      collection.get(id);
      break;
    case 1:
      // Boolean field match
      query.isActive = i % 2 === 0;
      collection.query(query);
      break;
    case 2:
      // Range query
      const min = Math.floor(Math.random() * 500);
      query.value = { $gt: min, $lt: min + 200 };
      collection.query(query);
      break;
    case 3:
      // Tag match
      const tagNum = Math.floor(Math.random() * 10);
      query.tags = `tag_${tagNum}`;
      collection.query(query);
      break;
  }
});
benchmarkResults.push(mediumQueryResult);

// Clean up after ourselves
console.log("\nCleaning up test data...");
const allDocs = collection.findAll();
console.log(`Deleting ${allDocs.length} test documents...`);
allDocs.forEach(doc => {
  collection.delete(doc.id);
});

// Print summary
console.log("\n=== BENCHMARK SUMMARY ===");
console.log("Operation                  | Count | Ops/Sec   | Ms/Op");
console.log("---------------------------|-------|-----------|-------");
benchmarkResults.forEach(result => {
  const name = result.name.padEnd(27);
  const count = String(result.count).padEnd(7);
  const opsPerSec = result.opsPerSecond.toFixed(2).padEnd(11);
  const msPerOp = result.msPerOp.toFixed(4);
  
  console.log(`${name}| ${count}| ${opsPerSec}| ${msPerOp}`);
});

// Return final results object
({
  totalOperations: benchmarkResults.reduce((sum, r) => sum + r.count, 0),
  totalExecutionTime: benchmarkResults.reduce((sum, r) => sum + r.totalTime, 0),
  averageOpsPerSecond: benchmarkResults.reduce((sum, r) => sum + r.opsPerSecond, 0) / benchmarkResults.length,
  results: benchmarkResults
});