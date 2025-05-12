/**
 * Integration test for QuickJS in JSON Database Server
 * This file tests the JavaScript API integration with the database
 */

// Test basic integration with the database
function testDatabaseIntegration() {
  // Test if the db object exists
  if (typeof db === 'undefined') {
    return {
      status: "error", 
      message: "Database integration test failed",
      details: "db object is not defined"
    };
  }
  
  // Test if required methods exist on the db object
  const requiredMethods = [
    'getCollection',
    'queryDocuments',
    'getDocument',
    'insertDocument',
    'updateDocument',
    'deleteDocument'
  ];
  
  const missingMethods = [];
  for (const method of requiredMethods) {
    if (typeof db[method] !== 'function') {
      missingMethods.push(method);
    }
  }
  
  if (missingMethods.length > 0) {
    return {
      status: "error", 
      message: "Database integration test failed",
      details: `Missing required methods: ${missingMethods.join(', ')}`
    };
  }
  
  return {
    status: "success",
    message: "Database integration test passed",
    details: "All required methods are available on the db object"
  };
}

// Test inserting and querying documents
function testDocumentOperations() {
  // Create a test collection name with a timestamp to ensure uniqueness
  const testCollection = "js_test_" + new Date().getTime();
  
  // Test document to insert
  const testDoc = {
    id: "test-1",
    name: "Test Document",
    values: [1, 2, 3, 4, 5],
    nested: {
      property: "nested value",
      timestamp: new Date().toISOString()
    }
  };
  
  try {
    // Insert the test document
    const insertResult = db.insertDocument(testCollection, testDoc);
    if (!insertResult) {
      return {
        status: "error",
        message: "Document operations test failed",
        details: "Failed to insert test document"
      };
    }
    
    // Query the document back
    const queryResult = db.getDocument(testCollection, "test-1");
    if (!queryResult) {
      return {
        status: "error",
        message: "Document operations test failed",
        details: "Failed to query inserted document"
      };
    }
    
    // Verify document properties
    if (queryResult.name !== testDoc.name || 
        queryResult.values.length !== testDoc.values.length ||
        queryResult.nested.property !== testDoc.nested.property) {
      return {
        status: "error",
        message: "Document operations test failed",
        details: "Retrieved document doesn't match inserted document"
      };
    }
    
    // Update the document
    testDoc.name = "Updated Test Document";
    testDoc.updateTime = new Date().toISOString();
    
    const updateResult = db.updateDocument(testCollection, "test-1", testDoc);
    if (!updateResult) {
      return {
        status: "error",
        message: "Document operations test failed",
        details: "Failed to update test document"
      };
    }
    
    // Query the updated document
    const updatedDoc = db.getDocument(testCollection, "test-1");
    if (!updatedDoc || updatedDoc.name !== "Updated Test Document") {
      return {
        status: "error",
        message: "Document operations test failed",
        details: "Updated document doesn't have expected changes"
      };
    }
    
    // Delete the document
    const deleteResult = db.deleteDocument(testCollection, "test-1");
    if (!deleteResult) {
      return {
        status: "error",
        message: "Document operations test failed",
        details: "Failed to delete test document"
      };
    }
    
    // Verify document is deleted
    const shouldBeNull = db.getDocument(testCollection, "test-1");
    if (shouldBeNull) {
      return {
        status: "error",
        message: "Document operations test failed",
        details: "Document still exists after deletion"
      };
    }
    
    return {
      status: "success",
      message: "Document operations test passed",
      details: "Successfully performed CRUD operations on documents"
    };
  } catch (e) {
    return {
      status: "error",
      message: "Document operations test failed with exception",
      details: e.toString()
    };
  }
}

// Test querying multiple documents
function testQueryDocuments() {
  // Create a test collection name with a timestamp to ensure uniqueness
  const testCollection = "js_query_test_" + new Date().getTime();
  
  try {
    // Insert multiple test documents
    for (let i = 1; i <= 10; i++) {
      const doc = {
        id: `query-${i}`,
        name: `Document ${i}`,
        value: i,
        isEven: i % 2 === 0,
        tags: [`tag-${i}`, i % 2 === 0 ? "even" : "odd"]
      };
      
      const result = db.insertDocument(testCollection, doc);
      if (!result) {
        return {
          status: "error",
          message: "Query documents test failed",
          details: `Failed to insert test document ${i}`
        };
      }
    }
    
    // Test queryDocuments with a filter for even documents
    const queryFilter = { isEven: true };
    const evenDocs = db.queryDocuments(testCollection, queryFilter);
    
    if (!evenDocs || !Array.isArray(evenDocs) || evenDocs.length !== 5) {
      return {
        status: "error",
        message: "Query documents test failed",
        details: `Expected 5 even documents, got ${evenDocs ? evenDocs.length : 0}`
      };
    }
    
    // Verify all returned documents have isEven = true
    for (const doc of evenDocs) {
      if (!doc.isEven) {
        return {
          status: "error",
          message: "Query documents test failed",
          details: "Query returned documents that don't match the filter"
        };
      }
    }
    
    // Clean up - delete all test documents
    for (let i = 1; i <= 10; i++) {
      db.deleteDocument(testCollection, `query-${i}`);
    }
    
    return {
      status: "success",
      message: "Query documents test passed",
      details: "Successfully queried documents with filters"
    };
  } catch (e) {
    return {
      status: "error",
      message: "Query documents test failed with exception",
      details: e.toString()
    };
  }
}

// Run all tests and aggregate results
function runAllTests() {
  const results = {
    databaseIntegration: testDatabaseIntegration(),
    documentOperations: testDocumentOperations(),
    queryDocuments: testQueryDocuments()
  };
  
  // Check if all tests passed
  const allPassed = Object.values(results).every(result => result.status === "success");
  
  return {
    status: allPassed ? "success" : "failure",
    message: allPassed ? "All integration tests passed" : "Some integration tests failed",
    details: results
  };
}

// Execute tests
const testResults = runAllTests();

// Return results
testResults;