# Field-Level RBAC Design for JSONdb

## Overview

A hybrid approach combining Unix-style permissions with regex-based field patterns provides both simplicity and flexibility.

## Design Principles

1. **Collection Level**: Unix-style (owner/group/world) for simplicity
2. **Field Level**: Regex patterns with permissions for flexibility
3. **Natural Flow**: Permissions cascade from collection → document → field

## Permission Structure

```json
{
  "_id": "_meta",
  "_collection": "users",
  "permissions": {
    "collection": {
      "owner": "rwxda",
      "group": "rx",
      "world": "r",
      "owner_id": "admin",
      "group_id": "user_managers"
    },
    "field_rules": [
      {
        "pattern": "^_id$",
        "permissions": {
          "owner": "r",
          "group": "r",
          "world": "r"
        },
        "description": "ID field is read-only for everyone"
      },
      {
        "pattern": "^password.*",
        "permissions": {
          "owner": "rw",
          "group": "",
          "world": ""
        },
        "description": "Password fields only accessible by owner"
      },
      {
        "pattern": "^(created_at|updated_at)$",
        "permissions": {
          "owner": "r",
          "group": "r",
          "world": "r"
        },
        "description": "Timestamps are system-managed, read-only"
      },
      {
        "pattern": "^internal_.*",
        "permissions": {
          "owner": "rw",
          "group": "r",
          "world": ""
        },
        "description": "Internal fields hidden from world"
      }
    ]
  }
}
```

## Permission Resolution Flow

1. **Check Collection Permission**: Can user access collection at all?
2. **Check Operation Permission**: Can user perform this operation (read/write/delete)?
3. **Check Field Rules**: For each field in document:
   - Find matching regex patterns
   - Apply most restrictive permission
   - System fields (_id, _created, etc.) have built-in rules

## Built-in Field Protection

```javascript
const SYSTEM_FIELD_RULES = [
  {
    pattern: /^_id$/,
    create: false,  // Cannot set on create
    update: false,  // Cannot modify
    delete: false   // Cannot remove field
  },
  {
    pattern: /^_meta$/,
    permissions: "admin_only"
  },
  {
    pattern: /^_(created|updated)_(at|by)$/,
    update: "system_only"
  }
];
```

## Examples

### 1. Protecting ID Fields
```json
{
  "pattern": "^(_id|id|uuid)$",
  "permissions": {
    "owner": "r",
    "group": "r",
    "world": "r"
  }
}
```

### 2. Sensitive Data
```json
{
  "pattern": "^(ssn|credit_card|bank_account)$",
  "permissions": {
    "owner": "rw",
    "group": "",
    "world": ""
  },
  "encrypt": true
}
```

### 3. Computed Fields
```json
{
  "pattern": "^computed_.*",
  "permissions": {
    "owner": "r",
    "group": "r",
    "world": "r"
  },
  "compute_function": "calculate_field"
}
```

## API Integration

```javascript
// Before insert
function checkFieldPermissions(doc, user, metadata) {
  for (const field in doc) {
    const rule = findMatchingRule(field, metadata.field_rules);
    if (!hasPermission(user, rule, 'write')) {
      throw new Error(`No permission to write field: ${field}`);
    }
  }
}

// During read
function filterFields(doc, user, metadata) {
  const filtered = {};
  for (const field in doc) {
    const rule = findMatchingRule(field, metadata.field_rules);
    if (hasPermission(user, rule, 'read')) {
      filtered[field] = doc[field];
    }
  }
  return filtered;
}
```

## Benefits

1. **Natural Extension**: Builds on Unix permissions
2. **Flexible**: Regex patterns handle any field naming
3. **Performant**: Rules cached per collection
4. **Secure**: System fields protected by default
5. **Auditable**: Clear permission trail

## Implementation Priority

1. System field protection (immediate)
2. Basic regex matching (phase 1)
3. Field filtering on read (phase 1)
4. Field validation on write (phase 1)
5. Advanced patterns (phase 2)