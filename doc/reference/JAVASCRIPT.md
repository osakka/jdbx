# JavaScript Integration for JSON Database Server

This document describes the JavaScript integration features of the JSON Database Server, which allows you to extend database functionality using JavaScript (powered by QuickJS).

**Note**: For information on JavaScript file path resolution and caching, see [JS_PATH_RESOLUTION.md](JS_PATH_RESOLUTION.md).

## Overview

The JSON Database Server integrates the QuickJS JavaScript engine to provide the following capabilities:

1. **Custom Query Language**: Write complex queries using JavaScript expressions
2. **User-Defined Functions**: Create and call custom JavaScript functions
3. **Document Validation**: Validate documents using JavaScript validators
4. **Document Transformation**: Transform documents during operations

## JavaScript API Endpoints

### JavaScript Query

Execute a JavaScript expression to filter documents in a collection.

- **URL**: `/api/js/query`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "collection": "string",
    "query": "string"
  }
  ```
- **Success Response**: `200 OK`
  ```json
  {
    "documents": [
      {
        "id": "string",
        ...
      }
    ],
    "count": "number"
  }
  ```
- **Example**:
  ```json
  {
    "collection": "users",
    "query": "doc.age > 30 && doc.status === 'active'"
  }
  ```

### JavaScript Evaluation

Evaluate arbitrary JavaScript code.

- **URL**: `/api/js/eval`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "code": "string"
  }
  ```
- **Success Response**: `200 OK`
  ```json
  {
    "success": true,
    "result": "any"
  }
  ```
- **Example**:
  ```json
  {
    "code": "const sum = (a, b) => a + b; sum(5, 3);"
  }
  ```

### Register User Function

Register a JavaScript function that can be called via API.

- **URL**: `/api/js/functions`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "name": "string",
    "code": "string"
  }
  ```
- **Success Response**: `200 OK`
  ```json
  {
    "success": true,
    "message": "Function registered successfully",
    "name": "string"
  }
  ```
- **Example**:
  ```json
  {
    "name": "calculateTotal",
    "code": "function userFunction(args) {\n  const { items, tax } = args;\n  const subtotal = items.reduce((sum, item) => sum + (item.price * item.quantity), 0);\n  return {\n    subtotal,\n    tax: subtotal * (tax || 0.1),\n    total: subtotal * (1 + (tax || 0.1))\n  };\n}"
  }
  ```

### Execute User Function

Call a registered JavaScript function.

- **URL**: `/api/js/functions/:name`
- **Method**: `POST`
- **Auth Required**: Yes
- **URL Parameters**: `name=[string]` - Function name
- **Request Body**: Function arguments (any JSON)
- **Success Response**: `200 OK`
  ```json
  {
    "success": true,
    "result": "any"
  }
  ```
- **Example Request**:
  ```json
  {
    "items": [
      { "price": 10, "quantity": 2 },
      { "price": 5, "quantity": 4 }
    ],
    "tax": 0.08
  }
  ```
- **Example Response**:
  ```json
  {
    "success": true,
    "result": {
      "subtotal": 40,
      "tax": 3.2,
      "total": 43.2
    }
  }
  ```

### Register Document Validator

Register a JavaScript validator for a collection.

- **URL**: `/api/js/validators`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "collection": "string",
    "code": "string"
  }
  ```
- **Success Response**: `200 OK`
  ```json
  {
    "success": true,
    "message": "Validator registered successfully",
    "collection": "string"
  }
  ```
- **Example**:
  ```json
  {
    "collection": "users",
    "code": "function validateDocument(doc) {\n  if (!doc.email || !doc.email.includes('@')) {\n    addError('email', 'Invalid email format');\n  }\n  if (doc.age !== undefined && (doc.age < 18 || doc.age > 120)) {\n    addError('age', 'Age must be between 18 and 120');\n  }\n  return isValid;\n}"
  }
  ```

### Register Document Transformer

Register a JavaScript transformer for a collection.

- **URL**: `/api/js/transformers`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "collection": "string",
    "code": "string"
  }
  ```
- **Success Response**: `200 OK`
  ```json
  {
    "success": true,
    "message": "Transformer registered successfully",
    "collection": "string"
  }
  ```
- **Example**:
  ```json
  {
    "collection": "users",
    "code": "function transformDocument(doc, operation) {\n  if (operation === 'insert' || operation === 'update') {\n    doc.updated_at = new Date().toISOString();\n    doc.email = doc.email ? doc.email.toLowerCase() : null;\n  }\n  return doc;\n}"
  }
  ```

## JavaScript Function Structure

### Document Validators

Validators should define a `validateDocument` function that returns a boolean indicating validity:

```javascript
function validateDocument(doc) {
  // Validate the document
  if (!doc.name) {
    addError('name', 'Name is required');
  }
  
  if (doc.age < 0) {
    addError('age', 'Age cannot be negative');
  }
  
  return isValid; // Built-in variable that tracks if any errors were added
}
```

The following helper functions and variables are available:
- `addError(field, message)`: Add a validation error
- `isValid`: Boolean tracking if the document is valid

### Document Transformers

Transformers should define a `transformDocument` function that takes a document and operation:

```javascript
function transformDocument(doc, operation) {
  // Modify the document based on the operation
  // operation can be: 'insert', 'update', 'delete', 'query'
  
  if (operation === 'insert') {
    doc.created_at = new Date().toISOString();
  }
  
  if (operation === 'insert' || operation === 'update') {
    doc.updated_at = new Date().toISOString();
  }
  
  return doc;
}
```

### User Functions

User functions should define a `userFunction` that takes arguments and returns a result:

```javascript
function userFunction(args) {
  // Process args and return result
  const { x, y } = args;
  return x + y;
}
```

## Database Access in JavaScript

JavaScript code has access to the database through the global `db` object:

### db.getCollection(collection)

Get all documents in a collection.

```javascript
const users = db.getCollection('users');
```

### db.queryDocuments(collection, query)

Query documents in a collection with a JSON query.

```javascript
const activeUsers = db.queryDocuments('users', { status: 'active' });
```

### db.getDocument(collection, id)

Get a specific document by ID.

```javascript
const user = db.getDocument('users', '123');
```

### db.insertDocument(collection, document)

Insert a new document.

```javascript
const result = db.insertDocument('users', { name: 'John', age: 30 });
```

### db.updateDocument(collection, id, document)

Update an existing document.

```javascript
const result = db.updateDocument('users', '123', { name: 'John', age: 31 });
```

### db.deleteDocument(collection, id)

Delete a document.

```javascript
const result = db.deleteDocument('users', '123');
```

## Running and Debugging JavaScript

### Command-Line Execution

To run a JavaScript file directly:

```bash
bin/jsondb_server -js my_script.js
```

For debugging JavaScript file execution, use the provided utility script:

```bash
# Basic debugging with detailed logs
scripts/js_debug.sh my_script.js

# Debugging with GDB for segmentation faults
scripts/js_debug.sh -g my_script.js

# Memory checking with Valgrind
scripts/js_debug.sh -v my_script.js
```

### JavaScript Path Cache Management

The server maintains a cache of resolved JavaScript file paths. You can manage this cache with:

```bash
# Show cache status
scripts/manage_js_cache.sh status

# List all cached paths
scripts/manage_js_cache.sh list

# Clear the cache
scripts/manage_js_cache.sh clear
```

## Examples

### Complex Query

```javascript
// Query for active admin users who joined in the last 30 days
const thirtyDaysAgo = new Date();
thirtyDaysAgo.setDate(thirtyDaysAgo.getDate() - 30);
const isoDate = thirtyDaysAgo.toISOString();

const query = `
  doc.status === 'active' && 
  doc.role === 'admin' && 
  doc.created_at > '${isoDate}'
`;

// POST to /api/js/query with body: {"collection": "users", "query": query}
```

### Custom Aggregation Function

```javascript
function userFunction(args) {
  const { collection, groupBy, metric } = args;
  
  // Get all documents
  const documents = db.getCollection(collection);
  
  // Group by specified field
  const groups = {};
  
  documents.forEach(doc => {
    const key = doc[groupBy] || 'undefined';
    if (!groups[key]) {
      groups[key] = [];
    }
    groups[key].push(doc);
  });
  
  // Calculate metrics for each group
  const result = {};
  
  for (const key in groups) {
    if (metric === 'count') {
      result[key] = groups[key].length;
    } else if (metric === 'sum') {
      result[key] = groups[key].reduce((sum, doc) => sum + (doc.amount || 0), 0);
    } else if (metric === 'avg') {
      const sum = groups[key].reduce((sum, doc) => sum + (doc.amount || 0), 0);
      result[key] = sum / groups[key].length;
    }
  }
  
  return result;
}
```

### Document Validator

```javascript
function validateDocument(doc) {
  // Email validation
  if (doc.email) {
    const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
    if (!emailRegex.test(doc.email)) {
      addError('email', 'Invalid email format');
    }
  } else {
    addError('email', 'Email is required');
  }
  
  // Age validation
  if (doc.age !== undefined) {
    if (typeof doc.age !== 'number') {
      addError('age', 'Age must be a number');
    } else if (doc.age < 0) {
      addError('age', 'Age cannot be negative');
    }
  }
  
  // Role validation
  if (doc.role) {
    const validRoles = ['user', 'admin', 'guest'];
    if (!validRoles.includes(doc.role)) {
      addError('role', `Role must be one of: ${validRoles.join(', ')}`);
    }
  }
  
  return isValid;
}
```

### Document Transformer

```javascript
function transformDocument(doc, operation) {
  // Add timestamps
  if (operation === 'insert') {
    const now = new Date().toISOString();
    doc.created_at = now;
    doc.updated_at = now;
  } else if (operation === 'update') {
    doc.updated_at = new Date().toISOString();
  }
  
  // Normalize strings
  if (doc.email) {
    doc.email = doc.email.toLowerCase().trim();
  }
  
  if (doc.name) {
    doc.name = doc.name.trim();
  }
  
  // Set defaults
  if (doc.status === undefined) {
    doc.status = 'active';
  }
  
  return doc;
}
```