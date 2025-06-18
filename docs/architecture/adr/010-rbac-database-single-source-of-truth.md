# ADR-010: RBAC Database as Single Source of Truth

**Date**: June 16, 2025  
**Status**: Implemented  
**Version**: v6.4.0

## Context

The JDBX RBAC (Role-Based Access Control) system previously maintained duplicate data structures:
- In-memory JSON objects (`rbac->users` and `rbac->roles`) for runtime access
- Database storage for persistence

This dual-storage approach violated our core architectural principle of "single source of truth" and created several issues:
- Data synchronization complexity
- Memory overhead from loading all users/roles into memory
- Potential inconsistencies between memory and database
- Unnecessary code complexity maintaining two data stores

## Decision

We have removed all in-memory RBAC storage and made the database the single source of truth for all RBAC data.

### Changes Made

1. **Removed fields from `rbac_system_t` structure**:
   ```c
   // BEFORE
   typedef struct {
       json_value_t* users; /* JSON object of users (deprecated) */
       json_value_t* roles; /* JSON object of roles (deprecated) */
       struct database* db;
       char* jwt_secret;
   } rbac_system_t;
   
   // AFTER
   typedef struct {
       struct database* db; /* Database for RBAC storage - SINGLE SOURCE OF TRUTH */
       char* jwt_secret;    /* JWT secret for token generation */
   } rbac_system_t;
   ```

2. **Updated all RBAC functions** to query the database directly instead of accessing in-memory structures

3. **Removed persistence functions** that saved/loaded between memory and database

4. **Updated API endpoints** to use database queries for all user/role operations

## Implementation Details

### Core Changes

1. **`rbac_check_permission()`** - Now queries user and role data directly from database
2. **User/Role CRUD operations** - All operations go directly to database via `rbac_db_*` functions
3. **API endpoints** - `/api/users` and `/api/roles` now use `storage_query_documents()`
4. **Session management** - Fixed to query sessions from correct library ("system" not "default")

### Benefits

1. **True Single Source of Truth**: All RBAC data exists only in the database
2. **Scalability**: No memory limitations from loading all users/roles
3. **Consistency**: No synchronization issues between memory and storage
4. **Performance**: Adaptive indexing optimizes frequently queried RBAC data
5. **Simplicity**: Removed complex synchronization and persistence code

### Security Improvements

During this refactoring, we also discovered and fixed a critical security vulnerability:
- Removed hardcoded "admin" password backdoor in `verify_password()` function
- Now all passwords must be properly verified against stored hashes

## Consequences

### Positive
- Cleaner architecture aligned with JDBX principles
- Better scalability for large user bases
- Reduced memory footprint
- Eliminated data synchronization bugs
- Improved security with no hardcoded credentials

### Neutral
- RBAC queries now always hit the database (mitigated by adaptive indexing)
- Slightly different performance characteristics (database queries vs memory access)

### Migration
- No migration needed - the database already contained all RBAC data
- Simply removed the redundant in-memory copies

## Technical Debt Removed

1. Eliminated ~500 lines of synchronization code
2. Removed persistence layer between memory and database
3. Simplified RBAC initialization and cleanup
4. Removed potential race conditions in multi-threaded access

## Future Considerations

1. **Caching**: If performance becomes an issue, implement a proper cache layer with TTL
2. **Prepared Queries**: Could optimize frequently used RBAC queries
3. **Audit Trail**: Database approach makes it easier to add RBAC audit logging

## References

- Original RBAC implementation: `src/components/rbac/`
- Database query functions: `src/components/database/document_storage.h`
- Adaptive indexing: ADR-006
- Unified documents architecture: ADR-008