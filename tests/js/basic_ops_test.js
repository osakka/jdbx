/**
 * Basic operations test for QuickJS integration
 * This tests basic JavaScript operations without using database APIs
 */

// Test basic arithmetic
function testArithmetic() {
  const a = 42;
  const b = 8;
  
  const sum = a + b;
  const diff = a - b;
  const product = a * b;
  const quotient = a / b;
  
  return {
    a: a,
    b: b,
    sum: sum,
    difference: diff,
    product: product,
    quotient: quotient
  };
}

// Test string operations
function testStrings() {
  const str1 = "Hello";
  const str2 = "World";
  
  const combined = str1 + ", " + str2 + "!";
  const upper = combined.toUpperCase();
  const lower = combined.toLowerCase();
  const len = combined.length;
  
  return {
    string1: str1,
    string2: str2,
    combined: combined,
    uppercase: upper,
    lowercase: lower,
    length: len
  };
}

// Test object operations
function testObjects() {
  const obj = {
    name: "Test Object",
    values: [1, 2, 3, 4, 5],
    nested: {
      prop1: "value1",
      prop2: 42
    }
  };
  
  // Add a property
  obj.newProp = "added later";
  
  // Modify a property
  obj.nested.prop2 *= 2;
  
  return obj;
}

// Run all tests
function runAllTests() {
  return {
    arithmetic: testArithmetic(),
    strings: testStrings(),
    objects: testObjects()
  };
}

// Execute tests
const result = runAllTests();

// Final result
result;