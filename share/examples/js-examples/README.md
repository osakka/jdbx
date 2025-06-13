# JDBX JavaScript Examples

This directory contains comprehensive examples of JavaScript validators, transformers, and functions for the JDBX system.

## Quick Start

### 1. Install Example Scripts

Use the examples from `comprehensive_examples.js` to create your own scripts. Simply copy the JSON structure and paste it into the appropriate collection in the JDBX browser interface:

- **Validators** → `_validators` collection
- **Transformers** → `_transformers` collection  
- **Functions** → `_functions` collection

### 2. Example Installation Script

Here's a quick way to install the email validator example:

1. Navigate to the `_validators` collection in JDBX browser
2. Click "Create Document"
3. Paste this JSON:

```json
{
    "name": "Email Validator",
    "description": "Validates email format using regex",
    "type": "validator",
    "tags": ["users"],
    "enabled": true,
    "code": "function validate(document, context) {\n    const errors = [];\n    const warnings = [];\n    \n    if (!document.email) {\n        errors.push('Email is required');\n    } else {\n        const emailRegex = /^[^\\s@]+@[^\\s@]+\\.[^\\s@]+$/;\n        if (!emailRegex.test(document.email)) {\n            errors.push('Invalid email format');\n        }\n    }\n    \n    return {\n        valid: errors.length === 0,\n        errors,\n        warnings\n    };\n}",
    "version": "1.0.0",
    "author": "JDBX Examples"
}
```

## Available Examples

### Validators

| Name | Description | Tags | Use Case |
|------|-------------|------|----------|
| Email Validator | Validates email format and presence | `["users", "contacts", "global"]` | User registration, contact forms |
| Age Range Validator | Validates age is within 13-120 range | `["users", "profiles"]` | Age verification, compliance |
| Required Fields Validator | Validates presence of required fields | `["*"]` | Data integrity, form validation |

### Transformers

| Name | Description | Tags | Use Case |
|------|-------------|------|----------|
| User Data Normalizer | Normalizes email/names to standard format | `["users", "contacts"]` | Data cleanup, standardization |
| Price Calculator | Calculates total price with tax/discounts | `["products", "orders"]` | E-commerce, billing |
| Data Enrichment | Adds metadata and computed fields | `["*"]` | Audit trails, data enhancement |

### Functions

| Name | Description | Tags | Use Case |
|------|-------------|------|----------|
| Statistics Calculator | Calculates mean, median, mode | `["analytics", "math"]` | Data analysis, reporting |
| Data Export Helper | Exports data to CSV/JSON/XML | `["export", "utility"]` | Data export, integration |
| Notification Generator | Generates notifications for changes | `["notifications", "events"]` | Event handling, alerts |

## Testing Your Scripts

### 1. Real-time Validation

After installing a validator:
1. Navigate to the target collection (e.g., `users`)
2. Create or edit a document
3. See real-time validation feedback as you type

### 2. Transformation Preview

After installing a transformer:
1. Navigate to the target collection
2. Edit a document
3. Click the "Preview Transform" button to see the transformed result

### 3. Function Execution

Functions can be executed through the JavaScript native API:

```bash
curl -X POST http://localhost:5000/api/js/native/execute \
  -H "Content-Type: application/json" \
  -d '{
    "script_id": "your-function-document-id",
    "input_data": {
      "numbers": [1, 2, 3, 4, 5]
    }
  }'
```

## Customization Guide

### Modifying Examples

1. Copy an existing example from `comprehensive_examples.js`
2. Modify the `code` field with your logic
3. Update `name`, `description`, and `tags` appropriately
4. Change `version` and `author` fields
5. Install as a new document

### Creating New Scripts

Follow this template structure:

```json
{
    "name": "Your Script Name",
    "description": "What your script does",
    "type": "validator|transformer|function",
    "tags": ["collection1", "collection2"],
    "enabled": true,
    "code": "function yourFunction(input, context) { /* your code */ }",
    "version": "1.0.0",
    "author": "Your Name",
    "created_at": "2025-06-01T12:00:00Z",
    "updated_at": "2025-06-01T12:00:00Z"
}
```

## Best Practices from Examples

### 1. Error Handling

All examples include proper error handling:

```javascript
function validate(document, context) {
    try {
        // Validation logic
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
        return document;
    }
    // Transformation logic
}
```

### 3. Context Awareness

Use context information for conditional logic:

```javascript
function transform(document, context) {
    // Skip in preview mode
    if (context.preview) {
        return document;
    }
    
    // Apply different logic based on operation
    if (context.operation === 'create') {
        document.created_at = new Date().toISOString();
    }
    
    return document;
}
```

## Performance Examples

### Efficient Validation

```javascript
// ✅ Good: Early return for performance
function validate(document, context) {
    if (!document.email) {
        return { valid: false, errors: ["Email required"], warnings: [] };
    }
    // Continue validation...
}
```

### Efficient Transformation

```javascript
// ✅ Good: Minimal object creation
function transform(document, context) {
    const result = { ...document };
    
    if (result.email) {
        result.email = result.email.toLowerCase();
    }
    
    return result;
}
```

## Common Patterns

### 1. Field Validation Pattern

```javascript
function validate(document, context) {
    const errors = [];
    const warnings = [];
    
    // Check required fields
    ['field1', 'field2'].forEach(field => {
        if (!document[field]) {
            errors.push(`${field} is required`);
        }
    });
    
    return { valid: errors.length === 0, errors, warnings };
}
```

### 2. Data Normalization Pattern

```javascript
function transform(document, context) {
    const result = { ...document };
    
    // Normalize string fields
    ['field1', 'field2'].forEach(field => {
        if (result[field] && typeof result[field] === 'string') {
            result[field] = result[field].trim().toLowerCase();
        }
    });
    
    return result;
}
```

### 3. Statistical Calculation Pattern

```javascript
function execute(input, context) {
    const { data, field } = input;
    
    if (!Array.isArray(data)) {
        throw new Error("Data must be an array");
    }
    
    const numbers = data
        .map(item => parseFloat(item[field]))
        .filter(n => !isNaN(n));
    
    return {
        count: numbers.length,
        sum: numbers.reduce((a, b) => a + b, 0),
        average: numbers.length > 0 ? numbers.reduce((a, b) => a + b, 0) / numbers.length : 0
    };
}
```

## Troubleshooting

### Common Issues

1. **Script Not Executing**
   - Verify `enabled: true`
   - Check tags match collection name
   - Ensure proper RBAC permissions

2. **Syntax Errors**
   - Use JSON string escaping for newlines (`\n`)
   - Validate JavaScript syntax before saving
   - Check browser console for errors

3. **Performance Issues**
   - Review execution time in metrics
   - Avoid heavy computations in validators
   - Use efficient algorithms

### Debug Logging

Add console.log statements for debugging:

```javascript
function validate(document, context) {
    console.log('Validating:', document);
    console.log('Context:', context);
    
    // Your validation logic
    const result = { valid: true, errors: [], warnings: [] };
    
    console.log('Validation result:', result);
    return result;
}
```

Check server logs to see debug output:
```bash
tail -f /opt/jdbx/var/jdbxd.log
```

## Advanced Usage

### Collection-Specific Logic

```javascript
function validate(document, context) {
    const collection = context.collection;
    
    switch (collection) {
        case 'users':
            return validateUser(document);
        case 'products':
            return validateProduct(document);
        default:
            return { valid: true, errors: [], warnings: [] };
    }
}
```

### Operation-Specific Logic

```javascript
function transform(document, context) {
    const result = { ...document };
    
    switch (context.operation) {
        case 'create':
            result.created_at = new Date().toISOString();
            break;
        case 'update':
            result.updated_at = new Date().toISOString();
            break;
    }
    
    return result;
}
```

## Documentation

For complete documentation, see:
- [JavaScript Development Guide](/opt/jdbx/docs/guides/javascript-development-guide.md)
- [API Documentation](/opt/jdbx/docs/api/)
- [RBAC Configuration Guide](/opt/jdbx/docs/guides/rbac-setup.md)

## Support

For issues or questions:
1. Check the JDBX logs for error details
2. Use the browser's real-time validation features
3. Review the comprehensive examples in this directory
4. Consult the development guide for advanced topics