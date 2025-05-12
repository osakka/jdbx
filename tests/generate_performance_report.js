/**
 * Performance Report Generator
 * 
 * Generates a comprehensive performance report for the JSON database
 * with JavaScript integration. This tool performs multiple benchmark runs
 * and produces a detailed analysis of the results.
 * 
 * Run with:
 * bin/jsondb_server -js_eval_file tests/generate_performance_report.js > performance_report.json
 */

// Load database helper functions
load("functions/db_helpers.js");

// Configuration
const CONFIG = {
  // Number of test iterations
  iterations: 5,
  
  // Test document counts
  documentCounts: {
    small: 100,
    medium: 1000,
    large: 5000  // Be careful with larger values as they might affect server stability
  },
  
  // Collection name for testing
  collectionName: "perf_report_test",
  
  // Operations to benchmark
  operations: [
    "insert",
    "get",
    "query",
    "update",
    "delete",
    "complex"
  ]
};

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

// Benchmark runner
function runBenchmark(name, count, fn) {
  // Warm-up
  for (let i = 0; i < Math.min(count * 0.1, 50); i++) {
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
  
  return {
    name,
    count,
    totalTime,
    opsPerSecond,
    msPerOp
  };
}

// Setup and cleanup
function setupCollection(collection, count) {
  // Clear any existing data
  try {
    const existingDocs = collection.findAll();
    existingDocs.forEach(doc => collection.delete(doc.id));
  } catch (e) {
    // Collection probably doesn't exist yet
  }
  
  // Insert test documents
  for (let i = 0; i < count; i++) {
    collection.insert(generateRandomData(i));
  }
  
  return collection;
}

function cleanupCollection(collection) {
  try {
    const existingDocs = collection.findAll();
    existingDocs.forEach(doc => collection.delete(doc.id));
  } catch (e) {
    // Collection probably doesn't exist or is empty
  }
}

// Run benchmark for each operation
function runOperationBenchmark(collection, operation, docCount, sampleSize) {
  switch (operation) {
    case "insert":
      return runBenchmark(`Insert (${docCount} docs)`, sampleSize, (i) => {
        collection.insert(generateRandomData(i + docCount));
      });
    
    case "get":
      return runBenchmark(`Get (${docCount} docs)`, sampleSize, (i) => {
        const id = `doc_${Math.floor(Math.random() * docCount)}`;
        collection.get(id);
      });
    
    case "query":
      return runBenchmark(`Query (${docCount} docs)`, sampleSize, (i) => {
        // Alternate different query types
        const queryType = i % 3;
        
        switch (queryType) {
          case 0: // Boolean field
            collection.query({ isActive: i % 2 === 0 });
            break;
          case 1: // Numeric range
            const min = Math.floor(Math.random() * 500);
            collection.query({ value: { $gt: min, $lt: min + 200 } });
            break;
          case 2: // Array contains
            const tagNum = Math.floor(Math.random() * 10);
            collection.query({ tags: `tag_${tagNum}` });
            break;
        }
      });
    
    case "update":
      return runBenchmark(`Update (${docCount} docs)`, sampleSize, (i) => {
        const id = `doc_${Math.floor(Math.random() * docCount)}`;
        const doc = collection.get(id);
        if (doc) {
          doc.updated = new Date().toISOString();
          doc.value = Math.floor(Math.random() * 1000);
          collection.update(id, doc);
        }
      });
    
    case "delete":
      // We need to insert additional documents that we can delete
      for (let i = 0; i < sampleSize; i++) {
        collection.insert(generateRandomData(`todelete_${i}`));
      }
      
      return runBenchmark(`Delete (${docCount} docs)`, sampleSize, (i) => {
        collection.delete(`doc_todelete_${i}`);
      });
    
    case "complex":
      return runBenchmark(`Complex (${docCount} docs)`, sampleSize, (i) => {
        // Get a random document
        const id = `doc_${Math.floor(Math.random() * docCount)}`;
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
  }
}

// Run the performance report generation
function generatePerformanceReport() {
  const startTime = new Date();
  const collection = new Collection(CONFIG.collectionName);
  const results = {};
  
  // Initialize results structure
  CONFIG.operations.forEach(op => {
    results[op] = {
      small: [],
      medium: [],
      large: []
    };
  });
  
  // Run benchmarks for each document count
  Object.entries(CONFIG.documentCounts).forEach(([sizeLabel, docCount]) => {
    console.log(`\n=== Running ${sizeLabel} dataset benchmarks (${docCount} documents) ===`);
    
    for (let iteration = 1; iteration <= CONFIG.iterations; iteration++) {
      console.log(`\nIteration ${iteration}/${CONFIG.iterations}`);
      
      // Setup collection with test data
      setupCollection(collection, docCount);
      
      // Run each operation benchmark
      CONFIG.operations.forEach(operation => {
        console.log(`- Benchmarking ${operation} operation...`);
        // Use a reasonable sample size for the operation
        const sampleSize = Math.min(docCount, 
          operation === "insert" || operation === "delete" ? 500 : 1000);
        
        const result = runOperationBenchmark(
          collection, 
          operation, 
          docCount, 
          sampleSize
        );
        
        results[operation][sizeLabel].push(result);
        console.log(`  ${result.opsPerSecond.toFixed(2)} ops/sec, ${result.msPerOp.toFixed(4)} ms/op`);
      });
      
      // Clean up after each iteration
      cleanupCollection(collection);
    }
  });
  
  // Calculate averages and statistics
  const summary = {};
  
  Object.entries(results).forEach(([operation, sizesData]) => {
    summary[operation] = {};
    
    Object.entries(sizesData).forEach(([sizeLabel, iterations]) => {
      const opsPerSecond = iterations.map(it => it.opsPerSecond);
      const msPerOp = iterations.map(it => it.msPerOp);
      
      // Calculate average, min, max, and standard deviation
      const avgOpsPerSec = opsPerSecond.reduce((sum, val) => sum + val, 0) / iterations.length;
      const minOpsPerSec = Math.min(...opsPerSecond);
      const maxOpsPerSec = Math.max(...opsPerSecond);
      
      const avgMsPerOp = msPerOp.reduce((sum, val) => sum + val, 0) / iterations.length;
      const minMsPerOp = Math.min(...msPerOp);
      const maxMsPerOp = Math.max(...msPerOp);
      
      // Standard deviation calculation
      const stdDevOpsPerSec = Math.sqrt(
        opsPerSecond.reduce((sum, val) => sum + Math.pow(val - avgOpsPerSec, 2), 0) / iterations.length
      );
      
      const stdDevMsPerOp = Math.sqrt(
        msPerOp.reduce((sum, val) => sum + Math.pow(val - avgMsPerOp, 2), 0) / iterations.length
      );
      
      summary[operation][sizeLabel] = {
        iterations: iterations.length,
        opsPerSecond: {
          avg: avgOpsPerSec,
          min: minOpsPerSec,
          max: maxOpsPerSec,
          stdDev: stdDevOpsPerSec
        },
        msPerOp: {
          avg: avgMsPerOp,
          min: minMsPerOp,
          max: maxMsPerOp,
          stdDev: stdDevMsPerOp
        },
        raw: iterations
      };
    });
  });
  
  // Generate final report
  const report = {
    generatedAt: new Date().toISOString(),
    duration: (new Date() - startTime) / 1000,
    config: CONFIG,
    summary,
    results
  };
  
  return report;
}

// Run the performance report generation
const report = generatePerformanceReport();

// Clean up any remaining test data
cleanupCollection(new Collection(CONFIG.collectionName));

// Return the complete report
report;