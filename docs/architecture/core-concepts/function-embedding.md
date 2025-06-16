# Function Embedding Architecture

## Overview

The JDBX function embedding system allows JavaScript functions to be defined either inline within documents or referenced from a centralized functions collection. This provides flexibility and reusability for validators, transformers, and lifecycle hooks.

## Function Types

### 1. Inline Functions

Functions defined directly in documents as string values:

```javascript
{
  "validator": "function(doc) { return doc.price > 0 ? { valid: true } : { valid: false }; }"
}
```

**Advantages:**
- Self-contained documents
- No external dependencies
- Quick prototyping
- Version-specific logic

**Detection:**
- Strings starting with "function"
- Strings containing "=>" (arrow functions)

### 2. Referenced Functions  

Functions stored in the functions collection and referenced by ID:

```javascript
{
  "validator": "@function:system/validate-email"
}
```

**Advantages:**
- Reusable across collections
- Centralized management
- Version control
- Performance optimization

**Format:** `@function:library/function-name`

## Implementation Details

### Function Resolution

The `js_function_resolver` module handles:

1. **Detection** - Identifying inline vs referenced functions
2. **Loading** - Fetching referenced functions from database
3. **Resolution** - Converting to executable format
4. **Recursion** - Processing nested documents

### Resolved Function Format

```javascript
{
  "code": "function(doc) { ... }",      // Executable JavaScript
  "name": "validate-email",             // Function name
  "type": "inline|reference",           // Source type
  "parameters": ["doc"],                // Optional: parameter list
  "description": "Validates email"      // Optional: description
}
```

### Integration Points

1. **Collection Validators** - Document validation on insert/update
2. **Transformers** - Document transformation on retrieval
3. **Lifecycle Hooks** - onCreate, onUpdate, onDelete handlers
4. **Custom Functions** - Application-specific logic

## Usage Examples

### Collection with Inline Validator

```javascript
{
  "type": "collection",
  "library": "myapp",
  "name": "products",
  "settings": {
    "validator": "function(doc) { 
      if (!doc.name || doc.name.length < 3) {
        return { valid: false, error: 'Name too short' };
      }
      if (doc.price <= 0) {
        return { valid: false, error: 'Price must be positive' };
      }
      return { valid: true };
    }"
  }
}
```

### Collection with Referenced Validator

```javascript
{
  "type": "collection", 
  "library": "myapp",
  "name": "users",
  "settings": {
    "validator": "@function:system/validate-user-email"
  }
}
```

### Reusable Function Definition

```javascript
{
  "type": "function",
  "library": "system",
  "name": "validate-user-email",
  "description": "Validates user documents with email",
  "parameters": ["doc"],
  "code": "function(doc) {
    const emailRegex = /^[^\\s@]+@[^\\s@]+\\.[^\\s@]+$/;
    if (!doc.email || !emailRegex.test(doc.email)) {
      return { valid: false, error: 'Invalid email address' };
    }
    return { valid: true };
  }",
  "tags": ["validator", "user", "email"],
  "version": "1.0.0"
}
```

### Lifecycle Hooks Example

```javascript
{
  "type": "collection",
  "library": "audit",
  "name": "logs",
  "settings": {
    "onCreate": "function(doc) { 
      doc.created_at = new Date().toISOString();
      doc.version = 1;
      return doc;
    }",
    "onUpdate": "@function:system/increment-version",
    "onDelete": "@function:audit/archive-log"
  }
}
```

## Best Practices

### When to Use Inline Functions

- Simple, collection-specific logic
- Prototyping and development
- One-off validation rules
- Performance-critical paths

### When to Use Referenced Functions

- Complex, reusable logic
- Cross-collection validators
- Standardized transformations
- Team-shared functions

### Function Naming Conventions

- **Validators**: `validate-{entity}-{field}`
- **Transformers**: `transform-{entity}-{action}`
- **Lifecycle**: `on-{event}-{entity}`
- **Utilities**: `util-{action}-{target}`

## Performance Considerations

1. **Caching** - Referenced functions are cached after first load
2. **Compilation** - Functions compiled once by QuickJS
3. **Resolution** - Happens during document processing
4. **Overhead** - Minimal for inline, one DB query for references

## Security

1. **Sandboxing** - All functions run in QuickJS sandbox
2. **Permissions** - Function execution respects RBAC
3. **Resource Limits** - CPU and memory constraints
4. **No I/O** - Functions cannot access filesystem/network

## Future Enhancements

1. **Function Versioning** - Reference specific versions
2. **Hot Reloading** - Update functions without restart
3. **TypeScript Support** - Type-safe function definitions
4. **Function Composition** - Chain multiple functions
5. **Performance Metrics** - Track function execution stats