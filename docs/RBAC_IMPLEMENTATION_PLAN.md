# JSONdb RBAC Implementation Plan

## Overview
This document outlines the comprehensive plan for completing the RBAC (Role-Based Access Control) implementation in JSONdb, including all necessary fixes, testing, documentation, and deployment steps.

## Current Status

### ✅ Completed
1. Database-backed user storage in `_users` collection
2. User registration and authentication with JWT tokens
3. Password hashing and verification
4. RBAC collections created (`_users`, `_roles`, `_permissions`, `_collections`, `_sessions`, `_permission_cache`)
5. Binary persistence for all RBAC data
6. Fixed collection creation (arrays vs objects)
7. Fixed database query result handling

### ❌ Issues to Fix
1. RBAC API routes not accessible in daemon process
2. Role management functions not fully implemented
3. Permission checking system incomplete
4. Collection ownership not implemented
5. Permission caching not implemented
6. JWT token expiration not configurable
7. Missing trace logging in critical paths

## Implementation Tasks

### Phase 1: Core Fixes (Priority: HIGH)

#### Task 1.1: Fix RBAC API Routes in Daemon Process
**Problem**: RBAC routes are registered after API context creation but lost during daemonization
**Solution**: 
- Move RBAC route registration to before daemonization
- OR: Create a unified route registration system
- OR: Pass extended routes to daemon process

**Files to modify**:
- `src/components/core/api.c`
- `src/initialize/api.c`
- `src/components/main.c`

**Testing**:
- Verify `/api/rbac/users` endpoint works
- Verify all RBAC endpoints are accessible

#### Task 1.2: Add Comprehensive Trace Logging
**Locations needing traces**:
- Role creation/deletion
- Permission granting/revoking
- Permission checking logic
- Collection ownership operations
- Cache operations
- JWT token validation

**Files to modify**:
- `src/components/rbac/rbac_database.c`
- `src/components/rbac/rbac.c`
- `src/components/rbac/jwt.c`

### Phase 2: Role Management (Priority: HIGH)

#### Task 2.1: Implement Role CRUD Operations
**Functions to implement**:
- `rbac_database_create_role()`
- `rbac_database_delete_role()`
- `rbac_database_get_role()`
- `rbac_database_list_roles()`

**Database operations**:
- Insert role documents into `_roles` collection
- Query and update role documents
- Handle role-user relationships

#### Task 2.2: Implement User-Role Assignment
**Functions to implement**:
- `rbac_database_add_user_to_role()`
- `rbac_database_remove_user_from_role()`
- `rbac_database_get_user_roles()`

**Database operations**:
- Update user documents with role arrays
- Update role documents with user arrays
- Maintain consistency between users and roles

### Phase 3: Permission System (Priority: HIGH)

#### Task 3.1: Implement Permission Management
**Functions to implement**:
- `rbac_database_grant_permission()`
- `rbac_database_revoke_permission()`
- `rbac_database_check_permission()` (enhance existing)

**Permission structure**:
```json
{
  "_id": "perm_id",
  "role_id": "role_id",
  "resource_type": "collection|document",
  "resource_id": "resource_name_or_path",
  "permissions": ["CREATE", "READ", "UPDATE", "DELETE", "ADMIN"]
}
```

#### Task 3.2: Implement Permission Inheritance
**Logic to implement**:
- Collection permissions cascade to documents
- Document permissions can override collection permissions
- Multiple roles with additive permissions
- Owner gets automatic ADMIN permission

#### Task 3.3: Implement Collection Ownership
**Functions to implement**:
- `rbac_database_create_collection_with_owner()`
- `rbac_database_get_collection_owner()`
- `rbac_database_transfer_ownership()`

**Database structure**:
```json
{
  "_id": "collection_name",
  "owner_id": "user_id",
  "created_at": "timestamp",
  "permissions": {}
}
```

### Phase 4: Advanced Features (Priority: MEDIUM)

#### Task 4.1: Implement Permission Caching
**Cache structure**:
```json
{
  "_id": "user:resource:permission",
  "result": true/false,
  "expires_at": "timestamp",
  "computed_at": "timestamp"
}
```

**Functions to implement**:
- `rbac_database_cache_permission()`
- `rbac_database_get_cached_permission()`
- `rbac_database_invalidate_cache()`

#### Task 4.2: Implement Configurable JWT Expiration
**Implementation**:
- Read from environment variable `JSONDB_JWT_EXPIRATION`
- Default to 2 hours (7200 seconds)
- Support refresh token rotation

### Phase 5: Testing (Priority: HIGH)

#### Task 5.1: Unit Tests
- Test user registration/authentication
- Test role CRUD operations
- Test permission granting/checking
- Test inheritance logic
- Test cache operations

#### Task 5.2: Integration Tests
- End-to-end user journey tests
- Permission inheritance scenarios
- Multi-role permission aggregation
- Collection ownership transfers
- Cache invalidation scenarios

#### Task 5.3: Performance Tests
- Permission check performance with cache
- Large number of roles/permissions
- Concurrent access patterns

### Phase 6: Documentation (Priority: MEDIUM)

#### Task 6.1: API Documentation
- Document all RBAC endpoints
- Provide curl examples
- Document permission model

#### Task 6.2: Implementation Documentation
- Update architecture docs
- Document RBAC database schema
- Document permission inheritance rules

#### Task 6.3: User Guide
- How to set up RBAC
- Best practices for roles/permissions
- Migration guide from file-based RBAC

### Phase 7: Code Cleanup & Git (Priority: MEDIUM)

#### Task 7.1: Code Cleanup
- Remove deprecated file-based RBAC code
- Clean up temporary test code
- Ensure consistent code style
- Fix all compiler warnings

#### Task 7.2: Git Preparation
- Review all changes
- Organize commits logically
- Write comprehensive commit messages
- Update CHANGELOG.md

#### Task 7.3: Final Testing & Push
- Full system test
- Verify binary compatibility
- Push to repository

## Success Criteria

1. All RBAC API endpoints accessible and functional
2. Users can be assigned to multiple roles
3. Permissions correctly cascade from collections to documents
4. Document-level permissions can override collection permissions
5. Permission checks are fast with caching
6. All operations have trace logging
7. Zero compiler warnings
8. Comprehensive documentation
9. All tests passing

## Time Estimate

- Phase 1 (Core Fixes): 2-3 hours
- Phase 2 (Role Management): 3-4 hours
- Phase 3 (Permission System): 4-5 hours
- Phase 4 (Advanced Features): 2-3 hours
- Phase 5 (Testing): 2-3 hours
- Phase 6 (Documentation): 2 hours
- Phase 7 (Cleanup & Git): 1-2 hours

**Total Estimate**: 16-22 hours

## Risk Mitigation

1. **Daemonization Issues**: If route registration fix is complex, implement as post-daemon initialization
2. **Performance**: Implement caching early to avoid permission check bottlenecks
3. **Data Consistency**: Use transactions for multi-document updates
4. **Backward Compatibility**: Maintain API compatibility where possible