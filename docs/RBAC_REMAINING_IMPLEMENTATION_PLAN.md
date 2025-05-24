# RBAC Remaining Features Implementation Plan

## Overview
This document outlines the detailed implementation plan for completing the RBAC system, including role management, permission management, caching, testing, documentation, and final deployment.

## Phase 2: Role CRUD Operations (2-3 hours)

### 2.1 GET /api/rbac/roles/:id
- Implement api_handle_rbac_get_role()
- Extract role ID from path
- Query role from database
- Return role with permissions

### 2.2 POST /api/rbac/roles
- Implement api_handle_rbac_create_role()
- Validate role name (unique, non-empty)
- Create role with initial permissions
- Return created role with ID

### 2.3 PUT /api/rbac/roles/:id
- Implement api_handle_rbac_update_role()
- Extract role ID from path
- Validate updated data
- Update role name/description
- Return updated role

### 2.4 DELETE /api/rbac/roles/:id
- Implement api_handle_rbac_delete_role()
- Extract role ID from path
- Check role not in use by users
- Delete role from database
- Return success/error

### 2.5 GET /api/rbac/roles
- Implement api_handle_rbac_get_roles()
- Query all roles from database
- Include permission summaries
- Return paginated results

## Phase 3: Permission Management (3-4 hours)

### 3.1 Fix Permission Structure
- Update rbac_db_check_permission() to handle both flat and nested structures
- Implement proper JSON INTEGER/NUMBER handling
- Create migration for existing permissions

### 3.2 POST /api/rbac/roles/:id/permissions
- Implement api_handle_rbac_grant_permission()
- Validate resource type and permission level
- Add permission to role
- Update permission cache

### 3.3 DELETE /api/rbac/roles/:id/permissions
- Implement api_handle_rbac_revoke_permission()
- Validate resource and permission
- Remove permission from role
- Update permission cache

### 3.4 GET /api/rbac/check-permission
- Implement api_handle_rbac_check_permission()
- Accept user_id, resource_type, resource_id, permission
- Check through role hierarchy
- Return permission status

### 3.5 User-Role Assignment
- POST /api/rbac/users/:id/roles - Add role to user
- DELETE /api/rbac/users/:id/roles/:role_id - Remove role from user
- GET /api/rbac/users/:id/roles - List user's roles

## Phase 4: Permission Caching (1-2 hours)

### 4.1 Implement Permission Cache
- Create in-memory cache for user permissions
- Add TTL-based expiration (5 minutes default)
- Implement cache invalidation on permission changes

### 4.2 Cache Integration
- Update rbac_db_check_permission() to use cache
- Add cache warming on server start
- Add cache statistics endpoint

## Phase 5: Testing (2-3 hours)

### 5.1 Unit Tests
- Test permission checking logic
- Test role CRUD operations
- Test user-role assignments
- Test cache functionality

### 5.2 Integration Tests
- Test complete authentication flow
- Test permission inheritance
- Test role updates affecting users
- Test concurrent access

### 5.3 Load Tests
- Test permission check performance
- Test cache effectiveness
- Test concurrent user operations

## Phase 6: Documentation (1-2 hours)

### 6.1 API Documentation
- Document all RBAC endpoints
- Provide example requests/responses
- Document permission model

### 6.2 Configuration Guide
- Document RBAC configuration options
- Explain permission hierarchy
- Provide setup examples

### 6.3 Migration Guide
- Document migration from file-based RBAC
- Explain permission structure changes
- Provide troubleshooting guide

## Phase 7: Code Cleanup & Git (1-2 hours)

### 7.1 Remove Temporary Code
- Remove admin permission bypass
- Remove JWT expiration bypass
- Clean up debug logging

### 7.2 Fix Warnings/Errors
- Fix all compiler warnings
- Fix implicit function declarations
- Fix unused variables
- Ensure zero-warning build

### 7.3 Code Review
- Review all changes
- Ensure consistent style
- Add missing comments
- Update function documentation

### 7.4 Git Operations
- Create feature branch
- Commit with detailed messages
- Create comprehensive PR description
- Push to mainline

## Implementation Order

1. **Fix Permission Structure First** (Phase 3.1)
   - Critical for all other operations
   - Removes need for temporary bypasses

2. **Implement Role CRUD** (Phase 2)
   - Foundation for permission management
   - Enables role-based testing

3. **Complete Permission Management** (Phase 3.2-3.5)
   - Builds on role CRUD
   - Completes core functionality

4. **Add Caching** (Phase 4)
   - Performance optimization
   - Can be tested with full system

5. **Testing** (Phase 5)
   - Validates all functionality
   - Ensures no regressions

6. **Documentation** (Phase 6)
   - Documents final implementation
   - Provides usage examples

7. **Cleanup & Deploy** (Phase 7)
   - Final polish
   - Production ready

## Success Criteria

- All RBAC endpoints functional
- Zero compiler warnings
- All tests passing
- Complete documentation
- Clean git history
- No regressions in existing functionality

## Risk Mitigation

- Test each phase before proceeding
- Keep detailed logs of changes
- Create rollback plan
- Test in isolated environment first