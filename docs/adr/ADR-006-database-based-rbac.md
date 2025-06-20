# ADR-006: Database-Based RBAC

**Date**: May 17, 2025  
**Status**: Accepted  
**Version**: 1.5.0  
**Impact**: Security  

## Context

Initial file-based RBAC implementation had critical limitations:
- File permission issues in containerized environments
- No ACID guarantees for security operations
- Synchronization complexity
- Limited scalability
- Separate storage from main data

## Decision

Implement RBAC using the database itself:
- Store users, roles, permissions as documents
- Use same transaction guarantees
- Leverage existing indexing
- Unified backup/restore

## Rationale

### Security Requirements
- ACID compliance for auth operations
- Audit trail in database
- No external dependencies
- Consistent with data model

### Operational Benefits
- Single backup includes RBAC
- No file permission issues
- Scales with database
- Unified access patterns

## Implementation

### RBAC Document Types
```json
// User document
{
  "type": "user",
  "uuid": "user-123",
  "username": "admin",
  "password_hash": "$2b$10$...",
  "roles": ["admin", "operator"],
  "library": "system"
}

// Role document
{
  "type": "role",
  "uuid": "role-456",
  "name": "admin",
  "permissions": [
    "collection:*:*",
    "system:*"
  ],
  "library": "system"
}

// Session document
{
  "type": "session",
  "uuid": "session-789",
  "user_id": "user-123",
  "token": "jwt-token",
  "expires_at": "2025-05-17T12:00:00Z",
  "library": "system"
}
```

### Storage Location
- System library: `system`
- Collections: `users`, `roles`, `sessions`
- Indexed on: `username`, `name`, `token`

## Consequences

### Positive
- **ACID Guarantees**: Security operations atomic
- **Unified Storage**: Single source of truth
- **Scalability**: Same as database
- **Backup**: Included in database backup

### Negative
- **Bootstrap**: Chicken-egg for first user
- **Performance**: Database overhead
- **Migration**: From file-based system

### Mitigations
- Bootstrap admin via environment
- Cached permission checks
- Automated migration tool
- Session cache for performance

## Technical Details

### Files Modified
- `src/components/rbac/rbac_database.c` - Database storage
- `src/components/rbac/rbac_init.c` - Bootstrap logic
- Removed: `rbac_file.c` - File-based implementation

### Bootstrap Process
1. Check for existing admin user
2. If none, create from environment:
   - `JDBX_BOOTSTRAP_ADMIN_USER`
   - `JDBX_BOOTSTRAP_ADMIN_PASS`
3. Store in database
4. Normal operations proceed

### Performance Optimizations
- In-memory session cache
- Permission result caching
- Indexed lookups
- Batch operations

## Validation

- ✅ ACID compliance verified
- ✅ Bootstrap process tested
- ✅ Migration tool working
- ✅ Performance acceptable
- ✅ Container deployment successful

## References

- Git commits: `ba3521d`, `816eee6` - Database-based RBAC
- Removed: `cfa1d53` - File-based fallback removal
- Related: ADR-007 (SSL/TLS Support)