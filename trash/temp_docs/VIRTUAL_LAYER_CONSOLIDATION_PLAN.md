# Virtual Layer Consolidation Plan

**Date**: June 17, 2025  
**Status**: CRITICAL - Multiple virtual function implementations found

## Current State Analysis

### Functions in database.c
1. `virtual_insert()` - Generic insert with type/library/collection parameters
2. `virtual_query()` - Generic query with type/library/collection parameters  
3. `virtual_update()` - Update by UUID
4. `virtual_get()` - Get by UUID
5. `virtual_delete()` - Delete by UUID
6. `virtual_query_users()` - Compatibility wrapper that calls virtual_query

### Functions in virtual_layer.c (NEW)
1. `virtual_create_user()` - User-specific creation
2. `virtual_get_user_by_uuid()` - User-specific UUID lookup
3. `virtual_get_user_by_name()` - User-specific name lookup
4. `virtual_update_user()` - User-specific update
5. `virtual_delete_user()` - User-specific delete
6. `virtual_query_users()` - User-specific query (CONFLICTS!)
7. Similar functions for roles and sessions

## Consolidation Strategy

### Option 1: Two-Layer Architecture (RECOMMENDED)
- **Generic Layer**: Keep virtual_insert, virtual_query, virtual_update, virtual_get, virtual_delete in database.c
- **Specific Layer**: Create user/role/session specific functions in virtual_layer.c that call generic functions

### Option 2: Single Layer Architecture
- Move ALL virtual functions to virtual_layer.c
- Remove from database.c entirely

## Implementation Plan (Option 1)

### Step 1: Fix Conflicts
1. Remove duplicate `virtual_query_users` from virtual_layer.c
2. Update virtual_layer.c functions to call generic virtual functions from database.c
3. Remove conflicting function signatures

### Step 2: Standardize Signatures
```c
// In database.c (generic layer)
json_value_t* virtual_insert(database_t* db, const char* type, const char* library, 
                            const char* collection, json_value_t* document, const char* owner);
json_value_t* virtual_query(database_t* db, const char* type, const char* library, 
                           const char* collection, json_value_t* filters);

// In virtual_layer.c (specific layer)  
json_value_t* virtual_create_user(database_t* db, const char* username, 
                                 const char* password_hash, const char* library);
json_value_t* virtual_get_user_by_name(database_t* db, const char* username, 
                                       const char* library);
```

### Step 3: Update Implementation
- Make virtual_layer.c functions call the generic functions in database.c
- Remove direct storage calls from virtual_layer.c
- Use single source of truth approach

## Example Implementation

```c
// In virtual_layer.c
json_value_t* virtual_create_user(database_t* db, const char* username, 
                                 const char* password_hash, const char* library) {
    // Create user document
    json_value_t* user_doc = json_create_object();
    json_object_set(user_doc, "username", json_create_string(username));
    json_object_set(user_doc, "password_hash", json_create_string(password_hash));
    
    // Call generic virtual_insert
    json_value_t* result = virtual_insert(db, DOC_TYPE_NAME_USER, library, 
                                         "users", user_doc, username);
    json_free(user_doc);
    return result;
}
```

## Benefits
1. Single source of truth for core virtual operations
2. Business logic encapsulation in specific functions
3. No duplicate implementations
4. Clean separation of concerns

## Next Steps
1. Fix compilation errors by removing conflicts
2. Update virtual_layer.c to use generic functions
3. Test thoroughly
4. Update RBAC to use consolidated virtual layer