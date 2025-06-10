# JSONdb Evolution Implementation Plan

## Summary of Decisions

1. **Metadata Access**: Both document and API access (flexible)
2. **Function Storage**: Mixed - inline for simple, reference for complex
3. **Schema Integration**: Functions embedded within schema properties
4. **Execution Context**: User RBAC permissions + new "execute" permission
5. **Versioning**: Automatic with diffs, opt-out via `"versioning": false`
6. **Diff Format**: JSON Patch (RFC 6902)
7. **Library Access**: Explicit RBAC permission grants
8. **Unique Constraints**: Index level + function validation

## Phase 1: Core Infrastructure (Week 1-2)

### 1.1 Extend RBAC Permissions
```c
// In rbac.h
#define RBAC_EXECUTE 0x10  // New permission

// In rbac_permissions.c
// Add execute permission checks
```

### 1.2 Collection Metadata System
```json
{
  "_id": "_meta",
  "_collection": "users",
  "_versioning": {
    "exclude_fields": ["last_login", "session_count"]
  },
  "schema": {
    "type": "object",
    "properties": {
      "username": {
        "type": "string",
        "unique": true,
        "functions": {
          "transform": "function(v) { return v.toLowerCase().trim(); }",
          "validate": {
            "ref": "_functions/no_reserved_words"
          }
        }
      }
    }
  }
}
```

### 1.3 Unique Constraint Engine
- Extend hash/btree indexes with unique flag
- Add atomic check during index insert
- Return specific error for constraint violations

## Phase 2: Function Execution Engine (Week 3-4)

### 2.1 Function Resolution
```c
typedef struct function_ref {
    enum { INLINE, REFERENCE, BUILTIN } type;
    union {
        char* code;           // Inline JS
        char* ref_path;       // Reference path
        native_func* func;    // Builtin C function
    } impl;
} function_ref_t;
```

### 2.2 Execution Pipeline
1. Parse collection metadata
2. Extract functions for operation type
3. Check RBAC execute permissions
4. Resolve references
5. Execute in order
6. Handle errors/rollback

### 2.3 QuickJS Integration Enhancement
- Function compilation cache
- Shared context per collection
- Resource limits

## Phase 3: Versioning System (Week 5-6)

### 3.1 JSON Patch Implementation
```c
typedef struct version_entry {
    char* version_id;
    char* parent_version;
    json_value_t* patch;  // RFC 6902 format
    time_t created_at;
    char* created_by;
} version_entry_t;
```

### 3.2 Diff Generation
- Before update: snapshot current state
- After update: generate JSON Patch
- Store patch if changes detected
- Skip excluded fields

### 3.3 Version Storage
```
Collection: users_versions
Document: {
  "_id": "usr123_v2",
  "doc_id": "usr123",
  "version": 2,
  "parent_version": 1,
  "patch": [
    {"op": "replace", "path": "/email", "value": "new@example.com"}
  ],
  "created_at": "2025-06-09T10:00:00Z",
  "created_by": "admin"
}
```

## Phase 4: Library System (Week 7-8)

### 4.1 Library Metadata
```json
{
  "_id": "_library_meta",
  "name": "user_management",
  "collections": ["users", "roles", "sessions"],
  "shared_functions": {
    "validate_email": {
      "code": "function(email) { return /^[^@]+@[^@]+$/.test(email); }",
      "version": "1.0.0"
    }
  },
  "permissions": {
    "execute": ["user_management:*", "admin:*"],
    "read": ["*"]
  }
}
```

### 4.2 Cross-Library References
- Permission check at resolution time
- Cache resolved references
- Handle missing references gracefully

## Implementation Order

### Week 1: Foundation
1. Extend RBAC with execute permission
2. Create collection metadata structure
3. Add unique flag to indexes

### Week 2: Constraints
1. Implement atomic unique checking
2. Add constraint error types
3. Create admin API for metadata

### Week 3: Function Engine
1. Function parser/resolver
2. QuickJS execution pipeline
3. Error handling/rollback

### Week 4: Testing & Optimization
1. Performance benchmarks
2. Cache optimization
3. Security audit

### Week 5: Versioning Core
1. JSON Patch library
2. Diff generation
3. Version storage

### Week 6: Version Integration
1. Hook into update operations
2. Version retrieval API
3. Cleanup/retention

### Week 7: Library System
1. Library metadata structure
2. Collection grouping
3. Permission system

### Week 8: Polish & Deploy
1. Migration tools
2. Documentation
3. Gradual rollout

## Success Metrics

1. **Duplicate Prevention**: 100% unique constraint enforcement
2. **Performance**: <5ms function execution overhead
3. **Storage**: <20% overhead from versioning (with diffs)
4. **Compatibility**: Zero breaking changes
5. **Adoption**: Clear migration path

## Risk Mitigation

1. **Performance**: Aggressive caching, native functions for hot paths
2. **Complexity**: Excellent docs, sensible defaults
3. **Migration**: Compatibility mode, gradual opt-in
4. **Security**: Sandbox all JS execution, resource limits

## Next Steps

1. Create detailed API specifications
2. Set up performance testing framework
3. Begin Phase 1 implementation
4. Weekly progress reviews