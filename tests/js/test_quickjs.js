/**
 * Simple test script for QuickJS integration
 * This file can be run with the JSON Database Server to verify JavaScript functionality
 */

// Simple function to test basic JavaScript execution
function testBasicExecution() {
  console.log("Basic JavaScript execution is working");
  return { status: "success", message: "Basic execution test passed" };
}

// Test JSON handling
function testJsonHandling() {
  const testObject = {
    name: "Test Object",
    properties: {
      numeric: 42,
      boolean: true,
      string: "Hello, QuickJS!",
      array: [1, 2, 3, 4, 5]
    },
    nestedArray: [
      { id: 1, value: "first" },
      { id: 2, value: "second" },
      { id: 3, value: "third" }
    ]
  };
  
  // Test JSON stringification
  const jsonString = JSON.stringify(testObject);
  const parsedBack = JSON.parse(jsonString);
  
  // Verify properties survive the round trip
  if (parsedBack.properties.numeric !== 42 || 
      parsedBack.properties.boolean !== true ||
      parsedBack.properties.string !== "Hello, QuickJS!" ||
      parsedBack.properties.array.length !== 5 ||
      parsedBack.nestedArray.length !== 3) {
    return { 
      status: "error", 
      message: "JSON handling test failed", 
      details: "JSON properties did not survive stringification and parsing"
    };
  }
  
  return { status: "success", message: "JSON handling test passed" };
}

// Test error handling
function testErrorHandling() {
  try {
    // Deliberately cause an error
    const obj = null;
    const result = obj.nonExistentProperty;
    
    // We should never reach here
    return { status: "error", message: "Error handling test failed - exception was not thrown" };
  } catch (e) {
    // This is expected - the error was correctly caught
    return { status: "success", message: "Error handling test passed", error: e.toString() };
  }
}

// Test memory management
function testMemoryManagement() {
  // Create a large array to use some memory
  const largeArray = new Array(10000).fill(0).map((_, i) => ({ 
    id: i, 
    data: "This is a test string to consume some memory".repeat(10) 
  }));
  
  // Filter and map the array
  const filtered = largeArray.filter(item => item.id % 2 === 0);
  const mapped = filtered.map(item => ({ 
    newId: item.id * 2, 
    originalData: item.data,
    computed: item.data.length * item.id
  }));
  
  // Clear references to allow garbage collection
  const result = { 
    originalSize: largeArray.length,
    filteredSize: filtered.length,
    mappedSize: mapped.length,
    sample: mapped[0]
  };
  
  return { status: "success", message: "Memory test completed", result };
}

// Run all tests and return combined results
function runAllTests() {
  const results = {
    basicExecution: testBasicExecution(),
    jsonHandling: testJsonHandling(),
    errorHandling: testErrorHandling(),
    memoryManagement: testMemoryManagement()
  };
  
  // Check if all tests passed
  const allPassed = Object.values(results).every(result => result.status === "success");
  
  return {
    status: allPassed ? "success" : "failure",
    message: allPassed ? "All tests passed" : "Some tests failed",
    details: results
  };
}

// Execute tests
const testResults = runAllTests();
console.log(JSON.stringify(testResults, null, 2));

// Return test results
return testResults;