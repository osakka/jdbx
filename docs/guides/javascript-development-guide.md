# JSONdb JavaScript Development Guide

## Overview

JSONdb provides a powerful JavaScript integration system that allows you to create validators, transformers, and functions that work natively with your database. Scripts are stored as regular JSON documents and execute using the QuickJS engine with full RBAC integration.

## Table of Contents

1. [Script Types](#script-types)
2. [Document Structure](#document-structure)
3. [Development Workflow](#development-workflow)
4. [API Reference](#api-reference)
5. [Best Practices](#best-practices)
6. [Performance Guidelines](#performance-guidelines)
7. [Debugging and Testing](#debugging-and-testing)
8. [Examples](#examples)

## Script Types

### Validators
Validators check document integrity and business rules before data is stored.

**Use Cases:**
- Email format validation
- Required field checking
- Business rule enforcement
- Data type validation

**Return Format:**
```javascript
{
    valid: boolean,
    errors: string[],
    warnings: string[]
}
```

### Transformers
Transformers modify documents during processing, normalizing or enriching data.

**Use Cases:**
- Data normalization (lowercase emails, title case names)
- Field calculation (totals, taxes)
- Data enrichment (adding metadata, timestamps)
- Format standardization

**Return Format:**
```javascript
// Modified document object
{
    ...originalDocument,
    // modified/added fields
}
```

### Functions
Functions perform complex operations and calculations on data.

**Use Cases:**
- Statistical analysis
- Data export/import
- Complex business logic
- Integration with external systems

**Return Format:**
```javascript
// Any JSON-serializable result
{
    result: any,
    metadata: object
}
```

## Document Structure

JavaScript scripts are stored as regular JSON documents in their respective collections:

### Collection Names
- **Validators**: `_validators`
- **Transformers**: `_transformers`
- **Functions**: `_functions`

### Required Fields

```json
{
    "_id": "doc-1234567890-123",
    "name": "Script Name",
    "description": "Brief description of what this script does",
    "type": "validator|transformer|function",
    "tags": ["collection1", "collection2", "*"],
    "enabled": true,
    "code": "function validate(document, context) { ... }",
    "version": "1.0.0",
    "author": "Developer Name",
    "created_at": "2025-06-01T12:00:00Z",
    "updated_at": "2025-06-01T12:00:00Z"
}
```

### Field Descriptions

- **name**: Human-readable script name
- **description**: Detailed description of functionality
- **type**: Script type (`validator`, `transformer`, or `function`)
- **tags**: Array of collection names or patterns (`*` for all collections)
- **enabled**: Boolean flag to activate/deactivate script
- **code**: JavaScript function code as string
- **version**: Semantic version string
- **author**: Script author/maintainer

## Development Workflow

### 1. Create Script Document

Navigate to the appropriate collection (`_validators`, `_transformers`, or `_functions`) in the JSONdb browser interface and create a new document.

### 2. Define Script Metadata

```json
{
    "name": "Email Validator",
    "description": "Validates email format and domain",
    "type": "validator",
    "tags": ["users", "contacts"],
    "enabled": true,
    "version": "1.0.0",
    "author": "Your Name"
}
```

### 3. Write JavaScript Code

Add your function code as a string in the `code` field:

```json
{
    "code": "function validate(document, context) {\n    // Your validation logic here\n    return { valid: true, errors: [], warnings: [] };\n}"
}
```

### 4. Test the Script

Use the browser's real-time validation and transformation preview features to test your script with sample data.

### 5. Deploy and Monitor

Once satisfied, enable the script and monitor its performance through the metrics dashboard.

## API Reference

### Context Object

All script functions receive a `context` object with execution metadata:

```javascript
{
    collection: "collection_name",
    operation: "create|read|update|delete|validate|transform",
    user_id: "doc-1234567890-123",
    realtime: true,
    preview: false
}
```

### Validator Function Signature

```javascript
function validate(document, context) {
    // document: The JSON document being validated
    // context: Execution context object
    
    return {
        valid: boolean,        // true if validation passes
        errors: string[],      // array of error messages
        warnings: string[]     // array of warning messages
    };
}
```

### Transformer Function Signature

```javascript
function transform(document, context) {
    // document: The JSON document being transformed
    // context: Execution context object
    
    // Return modified document
    return {
        ...document,
        // your modifications
    };
}
```

### Function Signature

```javascript
function execute(input, context) {
    // input: Input data passed to the function
    // context: Execution context object
    
    // Return any JSON-serializable result
    return {
        result: any,
        // additional metadata
    };
}
```

## Best Practices

### 1. Error Handling

Always use try-catch blocks for potentially failing operations:

```javascript
function validate(document, context) {
    try {
        // Your validation logic
        return { valid: true, errors: [], warnings: [] };
    } catch (error) {
        return { 
            valid: false, 
            errors: [`Validation error: ${error.message}`], 
            warnings: [] 
        };
    }
}
```

### 2. Input Validation

Always validate inputs before processing:

```javascript
function transform(document, context) {
    if (!document || typeof document !== 'object') {
        return document; // Return unchanged for invalid input
    }
    
    // Your transformation logic
    return document;
}
```

### 3. Idempotency

Ensure transformers are idempotent (can be run multiple times safely):

```javascript
function transform(document, context) {
    // Only add timestamp if not already present
    if (!document.processed_at) {
        document.processed_at = new Date().toISOString();
    }
    
    return document;
}
```

### 4. Performance Optimization

- Keep functions lightweight and fast
- Avoid heavy computations in real-time validation
- Use efficient algorithms and data structures
- Cache expensive calculations when possible

### 5. Security Considerations

- Never access external resources without explicit permission
- Validate all user inputs thoroughly
- Avoid storing sensitive data in script code
- Use RBAC permissions appropriately

## Performance Guidelines

### Execution Limits

- **Timeout**: 5 seconds maximum execution time
- **Memory**: 64MB memory limit per execution
- **CPU**: Limited CPU cycles to prevent blocking

### Optimization Tips

1. **Minimize Object Creation**: Reuse objects when possible
2. **Efficient Loops**: Use appropriate loop types for your use case
3. **Early Returns**: Return as early as possible in validation functions
4. **Batch Operations**: Process multiple items efficiently

### Performance Anti-Patterns

❌ **Avoid:**
```javascript
// Heavy computation in validator
function validate(document, context) {
    for (let i = 0; i < 1000000; i++) {
        // Expensive operation
    }
    return { valid: true, errors: [], warnings: [] };
}
```

✅ **Prefer:**
```javascript
// Lightweight validation
function validate(document, context) {
    if (!document.email) {
        return { valid: false, errors: ["Email required"], warnings: [] };
    }
    return { valid: true, errors: [], warnings: [] };
}
```

## Debugging and Testing

### 1. Browser Integration

Use the JSONdb browser interface to:
- Test validators in real-time as you type
- Preview transformations on sample data
- Execute functions with custom input

### 2. Console Logging

Use console.log for debugging (output appears in server logs):

```javascript
function validate(document, context) {
    console.log('Validating document:', document);
    // Your validation logic
}
```

### 3. Error Messages

Provide detailed error messages for troubleshooting:

```javascript
function validate(document, context) {
    if (!document.email) {
        return { 
            valid: false, 
            errors: ["Email field is required for user registration"], 
            warnings: [] 
        };
    }
}
```

### 4. Testing Strategy

1. **Unit Testing**: Test individual functions with various inputs
2. **Integration Testing**: Test with real document data
3. **Edge Cases**: Test with empty, null, and malformed data
4. **Performance Testing**: Verify execution time and resource usage

## Examples

### Simple Email Validator

```json
{
    "name": "Email Validator",
    "description": "Validates email format using regex",
    "type": "validator",
    "tags": ["users"],
    "enabled": true,
    "code": "function validate(document, context) {\n    const errors = [];\n    const warnings = [];\n    \n    if (!document.email) {\n        errors.push('Email is required');\n    } else {\n        const emailRegex = /^[^\\s@]+@[^\\s@]+\\.[^\\s@]+$/;\n        if (!emailRegex.test(document.email)) {\n            errors.push('Invalid email format');\n        }\n    }\n    \n    return {\n        valid: errors.length === 0,\n        errors,\n        warnings\n    };\n}",
    "version": "1.0.0",
    "author": "Admin"
}
```

### Data Normalizer Transformer

```json
{
    "name": "Name Normalizer",
    "description": "Converts names to title case",
    "type": "transformer",
    "tags": ["users", "contacts"],
    "enabled": true,
    "code": "function transform(document, context) {\n    const result = { ...document };\n    \n    if (result.firstName) {\n        result.firstName = result.firstName\n            .toLowerCase()\n            .split(' ')\n            .map(word => word.charAt(0).toUpperCase() + word.slice(1))\n            .join(' ');\n    }\n    \n    if (result.lastName) {\n        result.lastName = result.lastName\n            .toLowerCase()\n            .split(' ')\n            .map(word => word.charAt(0).toUpperCase() + word.slice(1))\n            .join(' ');\n    }\n    \n    return result;\n}",
    "version": "1.0.0",
    "author": "Admin"
}
```

### Statistics Function

```json
{
    "name": "Calculate Average",
    "description": "Calculates average of numeric array",
    "type": "function",
    "tags": ["analytics"],
    "enabled": true,
    "code": "function execute(input, context) {\n    const { numbers } = input;\n    \n    if (!Array.isArray(numbers)) {\n        throw new Error('Input must contain a numbers array');\n    }\n    \n    const validNumbers = numbers.filter(n => typeof n === 'number' && !isNaN(n));\n    \n    if (validNumbers.length === 0) {\n        return { average: 0, count: 0 };\n    }\n    \n    const sum = validNumbers.reduce((acc, num) => acc + num, 0);\n    const average = sum / validNumbers.length;\n    \n    return {\n        average: Math.round(average * 100) / 100,\n        count: validNumbers.length,\n        sum: sum\n    };\n}",
    "version": "1.0.0",
    "author": "Admin"
}
```

## Advanced Topics

### Tag-Based Execution

Scripts can be triggered based on document tags or collection patterns:

- **Specific Collections**: `["users", "contacts"]`
- **Global Execution**: `["*"]`
- **Pattern Matching**: `["user_*", "temp_*"]`

### Conditional Logic

Use context information for conditional execution:

```javascript
function transform(document, context) {
    // Only run on create operations
    if (context.operation === 'create') {
        document.created_at = new Date().toISOString();
    }
    
    // Skip processing in preview mode
    if (context.preview) {
        return document;
    }
    
    // Apply transformation
    return document;
}
```

### Chaining Operations

Transformers can be chained by having multiple scripts with the same tags. They execute in order of creation.

## Troubleshooting

### Common Issues

1. **Script Not Executing**
   - Check if `enabled: true`
   - Verify tags match collection name
   - Check RBAC permissions

2. **Syntax Errors**
   - Validate JavaScript syntax
   - Check for proper escaping in JSON strings
   - Use browser validation feature

3. **Performance Issues**
   - Review execution metrics
   - Optimize algorithm complexity
   - Consider caching strategies

4. **Unexpected Results**
   - Add console.log statements
   - Test with simple inputs first
   - Verify context object usage

### Getting Help

- Check the JSONdb logs for detailed error messages
- Use the browser's real-time validation features
- Review the comprehensive examples provided
- Monitor execution metrics for performance insights

---

This guide provides the foundation for developing powerful JavaScript integrations with JSONdb. For additional examples and advanced use cases, see the `/opt/jsondb/share/examples/js-examples/` directory.