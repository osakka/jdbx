# Database JavaScript Functions

This directory contains JavaScript functions for use with the JSONdb JavaScript integration. These functions provide database utilities and examples that can be imported and used in your JavaScript code.

## Available Functions

- **db_example.js**: Example database functions demonstrating common operations
- **db_helpers.js**: Helper utilities for database operations
- **func_test.js**: Test functions for testing JavaScript functionality
- **simple_test.js**: Simple test functions for basic functionality testing

## Usage

These functions can be imported into your JavaScript code using the JSONdb JavaScript integration API:

```javascript
// Load a function from the functions directory
const dbHelpers = require('db_helpers');

// Use the functions
const result = dbHelpers.validateDocument(myDoc);
```

The functions are automatically available through the JavaScript path resolution system in JSONdb.

## Adding New Functions

When adding new functions to this directory:

1. Follow the existing naming convention: `feature_purpose.js`
2. Document the purpose of the function at the top of the file
3. Provide examples of how to use the function
4. Update this README if adding a new category of functions