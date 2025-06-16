# Unified JSON-Embedded Functions Architecture

## Core Concept: Everything is JSON

Instead of separate validators/transformers, we have **functions embedded in JSON** that naturally attach to collections and fields.

## Collection Metadata Example

```json
{
  "_id": "_meta",
  "_collection": "users",
  "schema": {
    "type": "object",
    "required": ["username", "email"],
    "properties": {
      "username": {
        "type": "string",
        "functions": {
          "pre_save": [
            {
              "type": "inline",
              "code": "function(value) { return value.toLowerCase().trim(); }"
            },
            {
              "type": "reference",
              "ref": "libraries/common/functions/ensure_unique",
              "params": {"field": "username"}
            }
          ]
        }
      },
      "email": {
        "type": "string",
        "format": "email",
        "functions": {
          "validate": {
            "type": "inline",
            "code": "function(value) { return /^[^@]+@[^@]+\\.[^@]+$/.test(value); }"
          },
          "transform": {
            "type": "reference",
            "ref": "_functions/normalize_email"
          }
        }
      }
    }
  },
  "collection_functions": {
    "pre_insert": [
      {
        "name": "ensure_unique_user",
        "type": "inline",
        "code": "function(doc, db) { \n  const existing = db.find('users', {$or: [{username: doc.username}, {email: doc.email}]});\n  if (existing.length > 0) {\n    throw new Error('Username or email already exists');\n  }\n  return true;\n}"
      }
    ],
    "post_update": {
      "type": "reference",
      "ref": "libraries/audit/functions/log_change"
    }
  }
}
```

## Function Storage as JSON Documents

```json
{
  "_id": "ensure_unique",
  "_collection": "_functions",
  "_library": "common",
  "name": "ensure_unique",
  "description": "Ensures a field value is unique in the collection",
  "params": ["value", "field", "collection", "db"],
  "code": "function(value, field, collection, db) {\n  const query = {};\n  query[field] = value;\n  const existing = db.find(collection, query);\n  if (existing.length > 0) {\n    throw new Error(`${field} must be unique: ${value} already exists`);\n  }\n  return true;\n}",
  "version": "1.0.0",
  "rbac": {
    "execute": ["*"],
    "modify": ["admin"]
  }
}
```

## Natural Integration Points

### 1. Schema Validation + Functions
```json
{
  "username": {
    "type": "string",
    "minLength": 3,
    "maxLength": 20,
    "pattern": "^[a-zA-Z0-9_]+$",
    "functions": {
      "validate": "function(v) { return !reservedUsernames.includes(v); }",
      "transform": "function(v) { return v.toLowerCase(); }"
    }
  }
}
```

### 2. Function Attachment Points
- **Field Level**: Validate/transform specific fields
- **Document Level**: Pre/post save, update, delete
- **Collection Level**: Batch operations, queries
- **Cross-Collection**: Relationships, integrity

### 3. Function Types
- **inline**: JS code directly in JSON
- **reference**: Path to function document
- **builtin**: Native C functions for performance

## Cross-Library References

```json
{
  "functions": {
    "audit": {
      "type": "reference",
      "ref": "libraries/audit/functions/track_change",
      "auth": {
        "require_permission": "audit:read"
      }
    }
  }
}
```

## Natural Flow

1. **Collection Creation** → Metadata doc created with schema
2. **Schema Definition** → Functions attach to fields naturally
3. **Operation Execution** → Functions run in defined order
4. **Cross-References** → RBAC controls access
5. **Version Control** → Functions versioned like any JSON doc

## Benefits

1. **Unified Model**: Everything is JSON
2. **Natural Embedding**: Functions live where they're used
3. **Flexible References**: Share functions across collections/libraries
4. **RBAC Integration**: Permission control on function execution
5. **Version Everything**: Functions are just versioned documents