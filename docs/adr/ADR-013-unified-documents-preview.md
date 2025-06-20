# ADR-013: Unified Documents Architecture Preview

**Date**: June 12, 2025  
**Status**: Accepted  
**Version**: 5.0.0  
**Impact**: Architectural  

## Context

Multiple storage patterns created complexity:
- System entities in separate collections
- Hierarchical library/collection structure
- Inconsistent access patterns
- Duplicate code for different entities

## Decision

Preview unified documents architecture:
- All entities as documents
- Single physical collection exploration
- Virtual collections via fields
- Unified access patterns

## Rationale

### Simplification Goals
- One storage mechanism
- Consistent operations
- Reduced complexity
- Better maintainability

### Design Exploration
- Test feasibility
- Measure performance
- Identify challenges
- Plan migration

## Implementation

### Unified Document Model (Preview)
```json
// Everything is a document with type field
{
  "uuid": "doc-123",
  "type": "user",        // Discriminator
  "library": "system",   // Virtual namespace
  "collection": "users", // Virtual collection
  "owner": "system",
  "created_at": "2025-06-12T10:00:00Z",
  
  // Type-specific fields
  "username": "admin",
  "email": "admin@example.com"
}
```

### Virtual Collection Mapping
```c
// Preview implementation
const char* get_virtual_collection(const char* type) {
    if (strcmp(type, "user") == 0) return "users";
    if (strcmp(type, "role") == 0) return "roles";
    if (strcmp(type, "library") == 0) return "libraries";
    // ... more mappings
    return type; // Default to type name
}
```

### Query Translation
```c
// Before: Hierarchical
query_documents("system", "users", filter);

// After: Unified with type field
filter["type"] = "user";
filter["library"] = "system";
query_documents("documents", filter);
```

## Consequences

### Positive
- **Simplicity**: One storage pattern
- **Consistency**: Same operations everywhere
- **Flexibility**: Easy to add new types
- **Performance**: Single index optimization

### Negative
- **Migration**: Existing data conversion
- **Breaking**: API changes required
- **Learning**: New mental model

### Mitigations
- Gradual migration path
- Compatibility layer
- Comprehensive testing
- Clear documentation

## Technical Details

### Preview Implementation
- Created prototype in branch
- Tested with subset of operations
- Measured performance impact
- Identified migration challenges

### Performance Testing
```
Operation         Hierarchical    Unified    Impact
Create document   10ms           10ms       None
Query by type     5ms            6ms        +20%
Index overhead    Low            Medium     Acceptable
```

### Migration Challenges
1. Data structure changes
2. API compatibility
3. Index rebuilding
4. Client updates

## Validation

- ✅ Prototype functional
- ✅ Performance acceptable
- ✅ Migration path identified
- ✅ Benefits confirmed
- ✅ Ready for full implementation

## References

- Preview branch: `unified-docs-preview`
- Related: ADR-027 (Full Implementation)
- Design doc: Unified Documents RFC