# JDBX JavaScript Integration Reference

**Version**: 2.0.7  
**Last Updated**: January 2025

This comprehensive guide documents the JavaScript integration in JDBX, powered by QuickJS engine.

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Core API](#core-api)
4. [HTTP Endpoints](#http-endpoints)
5. [Functions and Scripts](#functions-and-scripts)
6. [Path Resolution](#path-resolution)
7. [Security](#security)
8. [Examples](#examples)

## Overview

JDBX integrates the QuickJS JavaScript engine to provide powerful scripting capabilities for data manipulation, validation, and transformation.

### Key Features

- **Query Language**: JavaScript-based query expressions
- **User-Defined Functions**: Create reusable JavaScript functions
- **Document Validation**: Custom validation logic
- **Document Transformation**: Transform data during operations
- **File-based Scripts**: Load and execute JavaScript files
- **Secure Execution**: Sandboxed JavaScript environment

## Architecture

### QuickJS Integration

JDBX uses QuickJS, a small and embeddable JavaScript engine that supports:
- ES2020 specification
- Modules (ES6 modules)
- Async/await
- TypedArrays
- BigInt
- Proxies

### Compilation Options

JavaScript support can be enabled/disabled at compile time:

```bash
# Enable JavaScript support (default)
make

# Disable JavaScript support
make ENABLE_JS=0
```

### JavaScript Context

Each JavaScript execution runs in an isolated context with:
- Limited execution time (configurable timeout)
- Restricted memory usage
- No access to system calls
- Database API exposed through global `db` object

## Core API

### Global `db` Object

The database is accessible through a global `db` object with the following methods:

#### Collection Operations

```javascript
// Get all documents in a collection
const users = db.getCollection('users');
console.log(`Found ${users.length} users`);

// List all collections
const collections = db.listCollections();
console.log('Collections:', collections);
```

#### Document Operations

```javascript
// Get a document by ID
const user = db.getDocument('users', 'user123');

// Insert a new document
const newUser = {
  name: 'John Doe',
  email: 'john@example.com',
  age: 30
};
const result = db.insertDocument('users', newUser);
console.log(`Created user with ID: ${result.id}`);

// Update a document
const updated = db.updateDocument('users', 'user123', {
  age: 31,
  updated_at: new Date().toISOString()
});

// Delete a document
const deleted = db.deleteDocument('users', 'user123');
```

#### Query Operations

```javascript
// Query documents with JavaScript expressions
const adults = db.queryDocuments('users', {
  filter: 'doc.age >= 18'
});

// Complex queries
const results = db.queryDocuments('users', {
  filter: 'doc.age > 25 && doc.status === "active"',
  sort: { age: -1 },
  limit: 10,
  offset: 0
});
```

### Helper Functions

```javascript
// Logging
console.log('Info message');
console.error('Error message');

// JSON operations
const obj = JSON.parse('{"key": "value"}');
const str = JSON.stringify(obj);

// Array operations
const numbers = [1, 2, 3, 4, 5];
const doubled = numbers.map(n => n * 2);
const sum = numbers.reduce((a, b) => a + b, 0);

// Date operations
const now = new Date();
const timestamp = now.toISOString();
```

## HTTP Endpoints

### JavaScript Query

Execute JavaScript-based queries:

```http
POST /api/js/query
Authorization: Bearer <token>
Content-Type: application/json

{
  "collection": "users",
  "query": "doc.age > 30 && doc.status === 'active'"
}
```

**Response**:
```json
{
  "documents": [
    {"id": "user123", "name": "John", "age": 35, "status": "active"}
  ],
  "count": 1
}
```

### JavaScript Evaluation

Execute arbitrary JavaScript code:

```http
POST /api/js/eval
Authorization: Bearer <token>
Content-Type: application/json

{
  "code": "const users = db.getCollection('users'); users.length;"
}
```

**Response**:
```json
{
  "success": true,
  "result": 42
}
```

### Register Function

Register a reusable JavaScript function:

```http
POST /api/js/functions
Authorization: Bearer <token>
Content-Type: application/json

{
  "name": "calculateAge",
  "code": "function calculateAge(birthYear) { return new Date().getFullYear() - birthYear; }"
}
```

### Call Function

Execute a registered function:

```http
POST /api/js/functions/{name}/call
Authorization: Bearer <token>
Content-Type: application/json

{
  "args": [1990]
}
```

### List Functions

Get all registered functions:

```http
GET /api/js/functions
Authorization: Bearer <token>
```

## Functions and Scripts

### User-Defined Functions

Functions can be stored and reused:

```javascript
// Register a validator function
db.registerFunction('validateEmail', `
  function validateEmail(doc) {
    const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
    if (!emailRegex.test(doc.email)) {
      throw new Error('Invalid email format');
    }
    return true;
  }
`);

// Register a transformer function
db.registerFunction('normalizeUser', `
  function normalizeUser(doc) {
    return {
      ...doc,
      email: doc.email.toLowerCase(),
      name: doc.name.trim(),
      createdAt: doc.createdAt || new Date().toISOString()
    };
  }
`);
```

### Script Files

Load and execute JavaScript files:

```javascript
// validators/user.js
function validateUser(doc) {
  if (!doc.name || doc.name.length < 2) {
    throw new Error('Name must be at least 2 characters');
  }
  if (!doc.email) {
    throw new Error('Email is required');
  }
  return true;
}

// Register the validator
db.registerValidator('users', validateUser);
```

Execute script file:
```bash
./bin/jdbxd -js_eval_file validators/user.js
```

## Path Resolution

### Search Order

JavaScript files are searched in the following order:

1. **Absolute paths**: Used as-is if they start with `/`
2. **Binary-relative paths**: Relative to the server binary location
3. **Share directory**: `<binary_dir>/../share/js/`
4. **Current directory**: As a fallback

### Default Directories

```
/opt/jdbx/
├── bin/
│   └── jdbxd
├── share/
│   └── js/
│       ├── functions/
│       ├── validators/
│       └── transforms/
└── var/
    └── js/
        └── user/
```

### File Caching

JavaScript files are cached in memory:
- Cache is enabled by default
- Files are reloaded if modified
- Clear cache with: `db.clearJSCache()`

## Security

### Sandbox Environment

JavaScript runs in a sandboxed environment with:
- No filesystem access
- No network access
- No process spawning
- Limited memory usage
- Execution timeout

### Permissions

JavaScript execution requires appropriate RBAC permissions:
- `js:eval` - Execute arbitrary JavaScript
- `js:function:create` - Create functions
- `js:function:call` - Call functions
- `js:query` - Execute JavaScript queries

### Best Practices

1. **Validate Input**: Always validate user-provided JavaScript
2. **Use Functions**: Prefer registered functions over eval
3. **Set Timeouts**: Configure appropriate execution timeouts
4. **Limit Memory**: Set memory limits for JavaScript execution
5. **Audit Usage**: Log all JavaScript executions

## Examples

### Data Validation

```javascript
// Register a comprehensive validator
db.registerFunction('validateProduct', `
  function validateProduct(doc) {
    const errors = [];
    
    if (!doc.name || doc.name.trim().length < 3) {
      errors.push('Product name must be at least 3 characters');
    }
    
    if (typeof doc.price !== 'number' || doc.price < 0) {
      errors.push('Price must be a positive number');
    }
    
    if (!doc.category || !['electronics', 'clothing', 'food'].includes(doc.category)) {
      errors.push('Invalid category');
    }
    
    if (errors.length > 0) {
      throw new Error(errors.join(', '));
    }
    
    return true;
  }
`);
```

### Data Transformation

```javascript
// Transform documents during query
const transformedUsers = db.queryDocuments('users', {
  filter: 'doc.age >= 18',
  transform: `
    ({
      id: doc.id,
      displayName: doc.firstName + ' ' + doc.lastName,
      isAdult: doc.age >= 18,
      ageGroup: doc.age < 30 ? 'young' : doc.age < 60 ? 'middle' : 'senior'
    })
  `
});
```

### Aggregation

```javascript
// Calculate statistics
const stats = db.eval(`
  const users = db.getCollection('users');
  const ages = users.map(u => u.age).filter(age => age != null);
  
  ({
    count: users.length,
    avgAge: ages.reduce((a, b) => a + b, 0) / ages.length,
    minAge: Math.min(...ages),
    maxAge: Math.max(...ages)
  })
`);
```

### Batch Operations

```javascript
// Bulk update with conditions
db.eval(`
  const users = db.getCollection('users');
  let updated = 0;
  
  users.forEach(user => {
    if (user.lastLogin < '2024-01-01') {
      db.updateDocument('users', user.id, {
        ...user,
        status: 'inactive',
        notified: false
      });
      updated++;
    }
  });
  
  console.log(\`Updated \${updated} inactive users\`);
`);
```

### Custom Query Language

```javascript
// Register a custom query function
db.registerFunction('search', `
  function search(collection, criteria) {
    const docs = db.getCollection(collection);
    
    return docs.filter(doc => {
      // Text search
      if (criteria.text) {
        const searchText = criteria.text.toLowerCase();
        const docText = JSON.stringify(doc).toLowerCase();
        if (!docText.includes(searchText)) return false;
      }
      
      // Date range
      if (criteria.dateFrom || criteria.dateTo) {
        const docDate = new Date(doc.createdAt);
        if (criteria.dateFrom && docDate < new Date(criteria.dateFrom)) return false;
        if (criteria.dateTo && docDate > new Date(criteria.dateTo)) return false;
      }
      
      // Custom fields
      for (const [key, value] of Object.entries(criteria.fields || {})) {
        if (doc[key] !== value) return false;
      }
      
      return true;
    });
  }
`);
```

## Configuration

### JavaScript Settings

```json
{
  "javascript": {
    "enabled": true,
    "timeout_ms": 5000,
    "memory_limit_mb": 128,
    "cache_enabled": true,
    "functions_dir": "share/js/functions",
    "validators_dir": "share/js/validators",
    "transforms_dir": "share/js/transforms"
  }
}
```

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `JDBX_JS_ENABLED` | Enable JavaScript | `true` |
| `JDBX_JS_TIMEOUT` | Execution timeout (ms) | `5000` |
| `JDBX_JS_MEMORY_LIMIT` | Memory limit (MB) | `128` |
| `JDBX_JS_CACHE` | Enable file caching | `true` |

## Troubleshooting

### Common Issues

1. **Script Not Found**
   - Check file path and search directories
   - Ensure proper permissions on script files
   - Verify JDBX_SHARE_DIR is set correctly

2. **Execution Timeout**
   - Optimize JavaScript code
   - Increase timeout setting
   - Break large operations into chunks

3. **Memory Limit Exceeded**
   - Process data in batches
   - Increase memory limit
   - Optimize data structures

4. **Permission Denied**
   - Check user has js:eval permission
   - Verify RBAC configuration
   - Check function permissions

## References

- [QuickJS Documentation](https://bellard.org/quickjs/)
- [JavaScript API Implementation](src/components/js/js_api.c)
- [JavaScript Engine](src/components/js/js_engine.c)
- [API Documentation](docs/api/api-rest.md#javascript)