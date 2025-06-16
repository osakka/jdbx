# Reference Counting System Tests

This directory contains a set of tests for the reference counting system implemented to resolve memory management issues in the JSON Database Server.

## Test Files

- **test_refcount_simple.c** - Basic test for the core reference counting functionality
- **test_refcount_debug.c** - Debug test with detailed logging for reference counting
- **test_refcount_complex.c** - Test for complex objects with reference counting
- **test_refcount_nested.c** - Test for nested objects with reference counting
- **test_refcount_shared.c** - Test for shared values between objects
- **test_refcount_simple_js.c** - Test for JavaScript integration with reference counting

## Running the Tests

Each test can be compiled and run individually:

```bash
# Compile and run the simple test
gcc -Wall -I./include -I/opt/qjs/include tests/test_refcount_simple.c src/utils/memory/ref_counter.c src/utils/memory/ref_json.c src/json.c -o tests/ref_counter_simple && ./tests/ref_counter_simple

# Compile and run the debug test
gcc -Wall -I./include -I/opt/qjs/include tests/test_refcount_debug.c src/utils/memory/ref_counter.c src/utils/memory/ref_json.c src/json.c -o tests/ref_counter_debug && ./tests/ref_counter_debug

# Compile and run the complex test
gcc -Wall -I./include -I/opt/qjs/include tests/test_refcount_complex.c src/utils/memory/ref_counter.c src/utils/memory/ref_json.c src/json.c -o tests/ref_counter_complex && ./tests/ref_counter_complex

# Compile and run the nested objects test
gcc -Wall -I./include -I/opt/qjs/include tests/test_refcount_nested.c src/utils/memory/ref_counter.c src/utils/memory/ref_json.c src/json.c -o tests/ref_counter_nested && ./tests/ref_counter_nested

# Compile and run the shared values test
gcc -Wall -I./include -I/opt/qjs/include tests/test_refcount_shared.c src/utils/memory/ref_counter.c src/utils/memory/ref_json.c src/json.c -o tests/ref_counter_shared && ./tests/ref_counter_shared

# Compile and run the JavaScript integration test
gcc -Wall -I./include -I/opt/qjs/include tests/test_refcount_simple_js.c src/utils/memory/ref_counter.c src/utils/memory/ref_json.c src/json.c -o tests/test_refcount_simple_js && ./tests/test_refcount_simple_js
```

## Test Descriptions

### Simple Reference Counting Test
Tests the basic functionality of the reference counting system, including creation, acquisition, and release of references.

### Debug Reference Counting Test
Provides detailed logging of reference counting operations to help debug issues.

### Complex Reference Counting Test
Tests the reference counting system with more complex objects, including multiple references to the same object.

### Nested Objects Test
Tests the reference counting system with nested objects, where freeing a parent object should properly handle nested child objects.

### Shared Values Test
Tests the reference counting system with values shared between multiple objects, ensuring no double-free errors occur.

### JavaScript Integration Test
Tests the integration of the reference counting system with JavaScript, ensuring proper memory management when passing objects between C and JavaScript.

## Expected Output

Each test should complete without any errors, particularly no "double free detected" errors. The debug tests will show detailed information about reference counting operations.

## Troubleshooting

If you encounter a "double free detected" error, it indicates that an object is being freed twice. This usually happens when the reference counting system is not properly tracking references or when objects are being freed directly instead of through the reference counting system.

Use the debug test to trace the reference counting operations and identify where the issue occurs.

## Memory Management Improvements

The reference counting system has been enhanced with:

1. **Registry for JSON Values** - Ensures each JSON value is wrapped by only one reference counter
2. **Safe Freeing Process** - Checks if an object has already been freed before attempting to free it again
3. **Address Tracking** - Maintains a list of freed addresses to prevent double-free errors
4. **Debug Logging** - Provides detailed information about reference counting operations

For more details about the memory management system, see the `MEMORY_MANAGEMENT.md` document in the `docs` directory.