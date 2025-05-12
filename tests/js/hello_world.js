/**
 * Simple hello world script for testing QuickJS integration
 * 
 * Note: Using a minimal approach since console.log isn't available
 */

// Store our output in a variable
let output = "Hello from QuickJS!\n";
output += "This is a simple test to verify JavaScript execution.\n";

// Test basic functionality
const num1 = 42;
const num2 = 8;
const sum = num1 + num2;
const product = num1 * num2;

output += "Basic arithmetic: " + num1 + " + " + num2 + " = " + sum + "\n";
output += "Basic arithmetic: " + num1 + " * " + num2 + " = " + product + "\n";

// Test JSON
const testObject = { 
  message: "JSON test successful", 
  timestamp: new Date().toISOString() 
};

output += "JSON test: " + JSON.stringify(testObject) + "\n";

// Define a function to get a result
function getResult() {
  return { 
    status: "success", 
    message: "Hello world test completed successfully",
    output: output
  };
}

// Return the result object as a string
getResult();