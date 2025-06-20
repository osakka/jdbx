# ADR-029: RBAC Database Single Source of Truth

**Date**: June 16, 2025  
**Status**: Accepted  
**Version**: 6.4.0  
**Impact**: Architectural  

## Context

The RBAC system maintained dual storage:
- In-memory structures (`rbac->users`, `rbac->roles`) for fast access
- Database persistence for durability

This dual storage created:
- Memory limitations from loading all users/roles at startup
- Synchronization complexity between memory and database
- Race conditions during concurrent updates
- Security vulnerability with hardcoded "admin" password fallback
- ~500 lines of synchronization code

## Decision

Eliminate all in-memory RBAC storage, making the database the single source of truth:
1. Remove `rbac->users` and `rbac->roles` structures
2. All RBAC queries go directly to database
3. Leverage adaptive indexing for performance
4. Remove synchronization and persistence code
5. Eliminate security backdoors

## Rationale

### Scalability
- No memory limitations from pre-loading all users
- Can handle millions of users without memory pressure
- Better suited for enterprise deployments

### Consistency
- No synchronization issues
- No stale data in memory
- ACID guarantees from database layer

### Security
- Removed hardcoded "admin" password backdoor
- All authentication through proper channels
- Complete audit trail in database

### Simplicity
- Eliminated ~500 lines of complex synchronization code
- Single query path for all RBAC operations
- Cleaner architecture with fewer failure modes

## Implementation

### Before - In-Memory Access
```c
// Direct memory access
json_value_t* user = json_object_get(rbac->users, user_id);
if (user) {
    // Use user data
}
```

### After - Database Query
```c
// Database query with unified documents
json_value_t* query = json_create_object();
json_object_set(query, "uuid", json_create_string(user_id));
json_object_set(query, "type", json_create_string("user"));
json_object_set(query, "library", json_create_string("system"));

json_value_t* result = storage_query_documents(rbac->db, query);
if (result && json_array_size(result) > 0) {
    json_value_t* user = json_array_get(result, 0);
    // Use user data
}
```

### Simplified RBAC Structure
```c
typedef struct rbac_system {
    database_t* db;           // Database connection only
    char jwt_secret[65];      // JWT signing secret
    // Removed: users, roles, permissions maps
} rbac_system_t;
```

### Performance Optimization
- Adaptive indexing automatically creates indexes for frequent queries
- System collections get lower thresholds for faster index creation
- Database caching layer handles hot data

## Consequences

### Positive
- **Unlimited Scale**: No memory constraints on user/role count
- **True Consistency**: Database ACID guarantees
- **Better Security**: No backdoors or in-memory secrets
- **Simpler Code**: ~500 lines removed

### Negative
- **Network Latency**: Database queries have network overhead
- **Query Complexity**: More complex than simple map lookups

### Mitigations
- **Adaptive Indexing**: Automatic performance optimization
- **Connection Pooling**: Reuse database connections
- **Query Optimization**: Prepared statements and caching

## Technical Details

### Files Modified
- `src/components/rbac/rbac_database.c` - Direct database queries
- `src/components/rbac/rbac.c` - Removed in-memory structures  
- `src/components/rbac/rbac_persistence.c` - DELETED (no longer needed)
- `src/components/rbac/rbac_db.c` - Updated query patterns

### Security Fix
```c
// REMOVED dangerous backdoor:
if (strcmp(password, "admin") == 0) {
    return 1; // Hardcoded backdoor
}
```

### Session Management
- Sessions correctly queried from "system" library
- No more in-memory session cache
- Proper expiration handling in database

## Validation

- ✅ All RBAC operations working
- ✅ Performance acceptable with indexing
- ✅ No memory growth with user count
- ✅ Security backdoor eliminated
- ✅ Concurrent operations safe

## References

- Git commit: `7b77830` - RBAC DATABASE SINGLE SOURCE OF TRUTH
- Depends on: ADR-027 (Unified Documents)
- CLAUDE.md: v6.4.0 section