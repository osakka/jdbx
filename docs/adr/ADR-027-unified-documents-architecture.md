# ADR-027: TRUE Unified Documents Architecture

**Date**: June 15, 2025  
**Status**: Accepted  
**Version**: 6.0.0  
**Impact**: Fundamental  

## Context

The JDBX database had evolved with a traditional hierarchical storage model where each library and collection represented a physical namespace. This led to:
- Complex routing logic for different entity types
- Duplicate storage mechanisms for system entities (users, roles, libraries)
- Inconsistent access patterns across different parts of the system
- Scalability limitations from hierarchical directory structures

The system needed architectural simplification while maintaining performance and functionality.

## Decision

Implement TRUE Unified Documents Architecture where:
1. ALL entities stored in single physical `default/documents` collection
2. Virtual collections based on document fields (`type`, `library`, `collection`)
3. No hierarchical storage - everything unified
4. Clear separation between storage operations and virtual business logic

## Rationale

### Architectural Simplification
- Single storage path eliminates routing complexity
- One query mechanism for all document types
- Unified indexing and optimization strategies
- Simplified backup and replication

### Performance Benefits
- O(log n) skiplist operations for all queries
- Automatic adaptive indexing on discriminator fields
- Reduced I/O from consolidated storage
- Better cache utilization

### Scalability
- No filesystem limitations from deep hierarchies
- Horizontal scaling simplified
- Better distribution characteristics

## Implementation

### Mandatory Document Fields
```json
{
  "uuid": "doc-1234567890-abcdef",      // Auto-generated
  "type": "user",                       // Document type
  "library": "system",                  // Virtual library
  "collection": "users",                // Virtual collection  
  "owner": "system",                    // Security/audit
  "created_at": "2025-06-15T10:00:00Z", // Timestamp
  "modified_at": "2025-06-15T10:00:00Z" // Timestamp
}
```

### Storage Functions
```c
// Direct unified collection operations
json_value_t* storage_insert_document(database_t* db, json_value_t* document);
json_value_t* storage_query_documents(database_t* db, json_value_t* query);
json_value_t* storage_get_document(database_t* db, const char* uuid);
json_value_t* storage_update_document(database_t* db, const char* uuid, json_value_t* document);
int storage_delete_document(database_t* db, const char* uuid);
```

### Virtual Functions
```c
// Business logic operations with field handling
json_value_t* virtual_create_user(database_t* db, const char* username, const char* password, const char* library);
json_value_t* virtual_query_users(database_t* db, const char* library, json_value_t* filters);
json_value_t* virtual_create_role(database_t* db, const char* name, json_value_t* permissions, const char* library);
json_value_t* virtual_query_roles(database_t* db, const char* library, json_value_t* filters);
```

## Consequences

### Positive
- **Architectural Clarity**: Single source of truth for all data
- **Performance**: Unified optimization strategies
- **Maintainability**: Simpler codebase with clear patterns
- **Scalability**: Better suited for distributed systems

### Negative
- **Migration Complexity**: Existing systems need data migration
- **Learning Curve**: Developers must understand virtual collections
- **Query Complexity**: Some queries more complex without hierarchies

### Neutral
- **Different Mental Model**: Shift from hierarchical to flat + fields
- **Tool Compatibility**: Some tools expect traditional collections

## Migration

### System Components Converted (25+)
- Core database layer
- API endpoints
- Authentication system
- RBAC subsystem
- Metrics collection
- JavaScript integration
- All utility systems

### Key Files
- `src/components/database/database.c` - Storage layer
- `src/components/core/api.c` - API conversion
- `src/include/database/document_storage.h` - Constants
- `src/components/database/virtual_layer.c` - Virtual operations

## Validation

- ✅ Single collection verification
- ✅ Field-based discrimination working
- ✅ Performance maintained
- ✅ All components converted
- ✅ Zero mixed routing

## References

- Git commit: `18a1606` - TRUE Unified Documents Architecture Complete
- Related: ADR-029 (RBAC Single Source)
- CLAUDE.md: v6.0.0 section