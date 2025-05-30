# Using JavaScript Functions in JSONdb

This guide explains how to use JavaScript functions, validators, and transformers with JSONdb.

## Overview

JSONdb provides JavaScript integration using QuickJS that allows you to:

1. **Validate Documents**: Ensure documents meet specific requirements before saving
2. **Transform Documents**: Modify documents during various operations
3. **Create Custom Functions**: Build reusable functions for data processing

## Directory Structure

JavaScript files are stored in these directories:

- **Validators**: `/validators/{collection_name}.js`
- **Transformers**: `/transforms/{collection_name}.js`
- **Functions**: `/functions/{function_name}.js`

## Document Validators

Validators ensure data integrity by checking documents against your business rules before they're inserted or updated.

### Creating a Validator

Create a file named `{collection_name}.js` in the `/validators` directory that exports a `validateDocument` function:

```javascript
// /validators/users.js
function validateDocument(doc) {
  // Check required fields
  if (!doc.username) {
    addError('username', 'Username is required');
  } else if (doc.username.length < 3) {
    addError('username', 'Username must be at least 3 characters long');
  }
  
  if (!doc.email) {
    addError('email', 'Email is required');
  } else {
    // Simple email validation
    const emailPattern = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
    if (!emailPattern.test(doc.email)) {
      addError('email', 'Invalid email format');
    }
  }
  
  // Return the validation result
  // isValid is a global variable maintained by the system
  // that is automatically set to false when addError() is called
  return isValid;
}
```

### Validator API

Inside your validator function, you have access to:

- **doc**: The document being validated
- **addError(field, message)**: Function to report validation errors
- **isValid**: Global boolean indicating if the document is valid

### Registering a Validator via HTTP API

You can also register a validator using the HTTP API:

```bash
curl -X POST http://localhost:5000/api/js/validators/register \
  -H "Content-Type: application/json" \
  -d '{
    "collection": "users",
    "code": "function validateDocument(doc) { /* validator code */ }"
  }'
```

## Document Transformers

Transformers modify documents during operations (insert, update, query) to ensure consistency, add derived fields, or apply business logic.

### Creating a Transformer

Create a file named `{collection_name}.js` in the `/transforms` directory that exports a `transformDocument` function:

```javascript
// /transforms/users.js
function transformDocument(doc, operation) {
  // Add timestamps
  const now = new Date().toISOString();
  
  if (operation === 'insert') {
    doc.created_at = now;
    doc.updated_at = now;
    
    // Set default status for new users
    if (!doc.status) {
      doc.status = 'pending';
    }
  } 
  
  if (operation === 'update') {
    // Update timestamp on changes
    doc.updated_at = now;
  }
  
  // Normalize email (for all operations)
  if (doc.email) {
    doc.email = doc.email.toLowerCase().trim();
  }
  
  // Return the modified document
  return doc;
}
```

### Transformer API

Inside your transformer function, you have access to:

- **doc**: The document being transformed
- **operation**: The current operation ('insert', 'update', or 'query')

### Registering a Transformer via HTTP API

You can also register a transformer using the HTTP API:

```bash
curl -X POST http://localhost:5000/api/js/transformers/register \
  -H "Content-Type: application/json" \
  -d '{
    "collection": "users",
    "code": "function transformDocument(doc, operation) { /* transformer code */ }"
  }'
```

## Custom JavaScript Functions

Custom functions allow you to create reusable data processing logic that can be called from other parts of your application.

### Creating a Custom Function

Create a file named `{function_name}.js` in the `/functions` directory:

```javascript
// /functions/calculateStats.js
function userFunction(args) {
  const { collection, field } = args;
  
  if (!collection || !field) {
    return { error: "Collection and field parameters are required" };
  }
  
  // Get documents from the collection
  const documents = db.getCollection(collection);
  
  // Extract values from the specified field
  const values = documents
    .map(doc => {
      const value = doc[field];
      return typeof value === 'number' ? value : null;
    })
    .filter(val => val !== null);
  
  if (values.length === 0) {
    return { error: "No numeric values found" };
  }
  
  // Calculate statistics
  const sum = values.reduce((total, val) => total + val, 0);
  const avg = sum / values.length;
  const min = Math.min(...values);
  const max = Math.max(...values);
  
  return {
    count: values.length,
    sum: sum,
    average: avg,
    min: min,
    max: max
  };
}
```

### Function API

Inside your custom function, you have access to:

- **args**: Arguments passed to the function
- **db**: Database API object for data access
  - `db.getCollection(collectionName)`
  - `db.getDocument(collectionName, id)`
  - `db.queryDocuments(collectionName, query)`

### Registering a Function via HTTP API

You can register a custom function using the HTTP API:

```bash
curl -X POST http://localhost:5000/api/js/functions/register \
  -H "Content-Type: application/json" \
  -d '{
    "name": "calculateStats",
    "code": "function userFunction(args) { /* function code */ }"
  }'
```

### Calling a Function via HTTP API

Call your custom function:

```bash
curl -X POST http://localhost:5000/api/js/functions/calculateStats \
  -H "Content-Type: application/json" \
  -d '{
    "collection": "products",
    "field": "price"
  }'
```

## JavaScript Query API

JSONdb allows you to execute JavaScript queries for advanced data manipulation.

### Executing a Query via HTTP API

```bash
curl -X POST http://localhost:5000/api/js/query \
  -H "Content-Type: application/json" \
  -d '{
    "collection": "users",
    "query": "return db.getCollection(\"users\").filter(user => user.age > 30 && user.status === \"active\");"
  }'
```

## JavaScript Database API

Inside JavaScript functions, you have access to the global `db` object:

```javascript
// Get all documents in a collection
const users = db.getCollection('users');

// Get a specific document by ID
const user = db.getDocument('users', 'user123');

// Query documents
const activeUsers = db.queryDocuments('users', { status: 'active' });

// Insert a document
const newUser = {
  username: 'john_doe',
  email: 'john@example.com'
};
const result = db.insertDocument('users', newUser);

// Update a document
const updated = db.updateDocument('users', 'user123', { 
  status: 'inactive' 
});

// Delete a document
const deleted = db.deleteDocument('users', 'user123');
```

## Example: Advanced Analytics Function

Here's an example of a more advanced function that performs analytics:

```javascript
function userFunction(args) {
  const { collection, analysis, groupBy, metric, filter } = args;
  
  if (!collection) {
    return { error: "Collection name is required" };
  }
  
  // Get all documents from the collection
  let documents = db.getCollection(collection);
  
  // Apply filter if provided
  if (filter && typeof filter === 'object') {
    documents = db.queryDocuments(collection, filter);
  }
  
  // Handle different analysis types
  switch (analysis) {
    case 'count':
      return { count: documents.length };
      
    case 'group':
      return groupDocuments(documents, groupBy, metric);
      
    case 'timeseries':
      return timeSeriesAnalysis(documents, args.timeField, args.interval, metric);
      
    default:
      return { error: "Unknown analysis type" };
  }
}

// Helper function to group documents by a field
function groupDocuments(documents, groupBy, metric) {
  if (!groupBy) {
    return { error: "groupBy field is required" };
  }
  
  // Group documents
  const groups = {};
  
  documents.forEach(doc => {
    const key = doc[groupBy] || 'null';
    
    if (!groups[key]) {
      groups[key] = [];
    }
    groups[key].push(doc);
  });
  
  // Calculate metrics
  const result = {};
  
  for (const key in groups) {
    const groupDocs = groups[key];
    result[key] = groupDocs.length;
  }
  
  return { results: result };
}
```

## Best Practices

1. **Keep Functions Simple**: Focus on specific tasks to make them easier to test and maintain.
2. **Handle Errors**: Always include error handling in your functions.
3. **Validate Inputs**: Check that required parameters are provided.
4. **Use Helper Functions**: Break complex logic into smaller functions.
5. **Avoid Heavy Computation**: JavaScript runs in the database process, so performance matters.
6. **Document Your Code**: Add comments explaining what your functions do.

## Security Considerations

- JavaScript functions run with the same permissions as the server.
- Validate user input before passing it to functions.
- Be cautious with functions that modify data, especially in validators and transformers.
- Avoid using `eval()` or other potentially unsafe JavaScript features.

## Troubleshooting

1. **Syntax Errors**: Check your JavaScript code for syntax errors.
2. **Validator/Transformer Not Running**: Verify that filenames match collection names.
3. **Function Not Found**: Ensure your function is registered properly.
4. **Performance Issues**: Check for inefficient code that processes large amounts of data.

## API Reference

For a complete reference of the JavaScript API, see the [JavaScript API documentation](/docs/api/JAVASCRIPT_API.md).