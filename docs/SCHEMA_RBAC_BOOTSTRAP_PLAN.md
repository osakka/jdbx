# Schema and RBAC Bootstrap Plan

## The Problem

When implementing schema validation alongside RBAC, we face a circular dependency:
1. To insert a user, we need to pass schema validation for the _users collection
2. To have a schema for _users, we need to be authenticated (schemas API requires auth)
3. To authenticate, we need a user in the database
4. But we can't insert a user without passing schema validation

## The Solution

### 1. Bootstrap Mode Detection
- During initialization, detect if the database is empty (no collections or just system collections)
- Enable a special "bootstrap mode" that bypasses certain validations

### 2. System Collection Schema Definitions
- Define built-in schemas for system collections (_users, _roles, _permissions, etc.)
- These schemas are hardcoded and loaded during initialization
- They cannot be modified via API (read-only)

### 3. Initialization Sequence
```
1. Database Init
   - Create database
   - Create system collections
   
2. Schema Bootstrap
   - Load hardcoded system schemas
   - Attach schemas to system collections (bypass auth)
   
3. RBAC Bootstrap  
   - Create default admin role
   - Create default admin user (will now pass schema validation)
   - Mark bootstrap complete
   
4. Normal Operation
   - All operations now require auth and schema validation
```

### 4. System Schemas

#### _users Schema
```json
{
  "type": "object",
  "properties": {
    "_id": { "type": "string" },
    "username": { 
      "type": "string", 
      "minLength": 3,
      "maxLength": 50,
      "pattern": "^[a-zA-Z0-9_-]+$"
    },
    "password": { "type": "string", "minLength": 8 },
    "email": { 
      "type": "string",
      "pattern": "^[^@]+@[^@]+\\.[^@]+$"
    },
    "roles": {
      "type": "array",
      "items": { "type": "string" }
    },
    "created_at": { "type": "number" },
    "last_login": { "type": "number" }
  },
  "required": ["username", "password"],
  "additionalProperties": false
}
```

#### _roles Schema
```json
{
  "type": "object", 
  "properties": {
    "_id": { "type": "string" },
    "name": { 
      "type": "string",
      "minLength": 3,
      "maxLength": 50
    },
    "description": { "type": "string" },
    "permissions": {
      "type": "object",
      "patternProperties": {
        ".*": {
          "type": "array",
          "items": {
            "type": "string",
            "enum": ["CREATE", "READ", "UPDATE", "DELETE", "ADMIN"]
          }
        }
      }
    },
    "system_permissions": {
      "type": "array",
      "items": { "type": "string" }
    }
  },
  "required": ["name", "permissions"],
  "additionalProperties": false
}
```

### 5. Implementation Changes

1. **Add Bootstrap Mode Flag**
   - Add `is_bootstrap_mode` to database struct
   - Set during initialization if database is empty
   - Clear after default admin created

2. **Schema Validation Bypass**
   - In `db_insert` and `db_update`, check bootstrap mode
   - If bootstrap mode AND system collection, skip schema validation

3. **System Schema Loader**
   - Create `load_system_schemas()` function
   - Hardcode system collection schemas
   - Attach during database initialization

4. **API Protection**
   - Prevent modification of system collection schemas via API
   - Return 403 Forbidden for PUT/DELETE on system schemas

### 6. Migration Path
For existing databases:
1. Check if system collections have schemas
2. If not, attach system schemas
3. Validate existing data matches schemas
4. Log any validation errors (don't fail)

### 7. Benefits
- Clean bootstrap process
- System collections always have proper validation
- No manual intervention needed for new installations
- Existing databases can be migrated safely