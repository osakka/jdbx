# ADR-017: TRUE Unified Documents Architecture

**Date**: June 14, 2025  
**Status**: Active  
**Deciders**: Architecture Team  
**Technical Story**: Eliminate all hierarchical storage patterns

## Context and Problem Statement

JDBX v3.x-v5.x implemented partial unified documents with fallback logic to hierarchical storage. This created confusion, mixed routing patterns, and maintenance burden.

## Decision Drivers

- Architectural clarity and simplicity
- Maintenance burden of dual systems
- Performance overhead of routing logic
- Developer confusion with mixed patterns
- True single source of truth requirement

## Considered Options

1. **Keep Mixed Routing** - Maintain backward compatibility
2. **Gradual Migration** - Slowly phase out hierarchical
3. **Complete Cutover** - Remove all hierarchical patterns
4. **Abstraction Layer** - Hide complexity behind API

## Decision Outcome

Chosen option: **Complete Cutover** to TRUE unified architecture:
- ALL documents in single `default/documents` collection
- Zero fallback logic anywhere
- Clear Storage/Virtual function separation
- Mandatory field enforcement
- 25+ components converted

### Positive Consequences

- True single source of truth achieved
- Eliminated routing complexity
- Simplified maintenance
- Clear architectural boundaries
- Better performance (no routing overhead)

### Negative Consequences

- Breaking change for existing deployments
- Migration required for old data
- Complete rewrite of many components
- Testing effort for conversion

## Implementation Details

### Storage Layer (Physical)
```c
// Direct unified collection access
json_value_t* storage_insert_document(database_t* db, json_value_t* doc);
json_value_t* storage_query_documents(database_t* db, json_value_t* query);
json_value_t* storage_get_document(database_t* db, const char* uuid);
```

### Virtual Layer (Logical)
```c
// Entity-specific operations with business logic
json_value_t* virtual_create_user(database_t* db, const char* username, ...);
json_value_t* virtual_query_roles(database_t* db, const char* library, ...);
```

### Mandatory Document Fields
```json
{
  "uuid": "doc-123-456",        // Auto-generated
  "type": "user",               // Document type
  "library": "default",         // Namespace
  "collection": "users",        // Virtual collection
  "owner": "admin",             // Security
  "created_at": 1234567890,     // Timestamp
  "modified_at": 1234567890     // Timestamp
}
```

## Migration Guide

1. Export data from hierarchical collections
2. Transform to unified format with mandatory fields
3. Import into single documents collection
4. Update application code to use new APIs
5. Remove hierarchical collection references

## Links

- [Unified Documents Guide](../core-concepts/unified-documents.md)
- [Migration Tools](../../how-to/operations/unified-migration.md)