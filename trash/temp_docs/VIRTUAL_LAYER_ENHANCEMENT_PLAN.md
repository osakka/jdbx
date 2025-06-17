# Virtual Layer Enhancement Plan

**Date**: June 17, 2025  
**Goal**: Implement a proper virtual layer that handles all business logic

## Design Principles

1. **Complete Abstraction**: Application code NEVER touches storage layer
2. **UUID Support**: Virtual layer handles both name and UUID lookups
3. **Single Source of Truth**: One consistent way to do everything
4. **No Shortcuts**: Proper implementations only

## Virtual Layer Functions Needed

### User Management
```c
// Primary operations
json_value_t* virtual_create_user(database_t* db, const char* username, 
                                 const char* password, const char* library);
json_value_t* virtual_get_user_by_uuid(database_t* db, const char* uuid);
json_value_t* virtual_get_user_by_name(database_t* db, const char* username, 
                                      const char* library);
json_value_t* virtual_update_user(database_t* db, const char* uuid, 
                                 json_value_t* updates);
int virtual_delete_user(database_t* db, const char* uuid);
json_value_t* virtual_query_users(database_t* db, json_value_t* query);

// Convenience operations
json_value_t* virtual_authenticate_user(database_t* db, const char* username, 
                                       const char* password, const char* library);
int virtual_add_role_to_user(database_t* db, const char* user_uuid, 
                            const char* role_uuid);
int virtual_remove_role_from_user(database_t* db, const char* user_uuid, 
                                 const char* role_uuid);
```

### Role Management
```c
// Primary operations
json_value_t* virtual_create_role(database_t* db, const char* name, 
                                 const char* library);
json_value_t* virtual_get_role_by_uuid(database_t* db, const char* uuid);
json_value_t* virtual_get_role_by_name(database_t* db, const char* name, 
                                      const char* library);
json_value_t* virtual_update_role(database_t* db, const char* uuid, 
                                 json_value_t* updates);
int virtual_delete_role(database_t* db, const char* uuid);
json_value_t* virtual_query_roles(database_t* db, json_value_t* query);

// Permission operations
int virtual_grant_permission(database_t* db, const char* role_uuid, 
                           const char* resource, const char* permission);
int virtual_revoke_permission(database_t* db, const char* role_uuid, 
                            const char* resource, const char* permission);
```

### Session Management
```c
json_value_t* virtual_create_session(database_t* db, const char* user_uuid, 
                                    const char* token, time_t expires_at);
json_value_t* virtual_get_session_by_token(database_t* db, const char* token);
json_value_t* virtual_get_session_by_uuid(database_t* db, const char* uuid);
int virtual_update_session(database_t* db, const char* uuid, 
                          json_value_t* updates);
int virtual_delete_session(database_t* db, const char* uuid);
int virtual_invalidate_user_sessions(database_t* db, const char* user_uuid);
```

## Implementation Strategy

### Step 1: Create virtual_layer.c/h
- New files dedicated to virtual layer implementation
- Clear separation from storage layer
- Comprehensive error handling

### Step 2: Implement Core Functions
1. Start with user management (most critical)
2. Add role management
3. Add session management
4. Add helper/convenience functions

### Step 3: Add UUID Standardization
- All documents get "uuid" field on creation
- Support lookup by uuid for all types
- Maintain backward compatibility with "id", "_id"

### Step 4: Convert RBAC
- Update all RBAC functions to use virtual layer
- Remove ALL direct storage/database calls
- Test each function thoroughly

### Step 5: Convert API Endpoints
- Update authentication to use virtual layer
- Update RBAC API to use virtual layer
- Update session management to use virtual layer

## UUID Generation Standard

```c
// Standard UUID generation function
char* virtual_generate_uuid(void) {
    static uint32_t counter = 0;
    char* uuid = BUFFER_ALLOC(64);
    if (!uuid) return NULL;
    
    time_t now = time(NULL);
    uint32_t unique = __sync_fetch_and_add(&counter, 1);
    
    snprintf(uuid, 64, "doc-%ld-%u", (long)now, unique);
    return uuid;
}
```

## Testing Requirements

1. Unit tests for each virtual function
2. Integration tests for RBAC operations
3. Performance tests for UUID lookups
4. Regression tests for existing functionality

## Success Criteria

1. Zero direct storage calls in application code
2. Consistent UUID handling throughout
3. All tests passing
4. No performance degradation
5. Clean, maintainable code

## Timeline

1. **Day 1**: Implement virtual_layer.c with user management
2. **Day 2**: Add role and session management
3. **Day 3**: Convert RBAC to use virtual layer
4. **Day 4**: Convert API endpoints
5. **Day 5**: Testing and refinement