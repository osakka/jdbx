# RBAC Unified Documents Architecture Audit Report

**Date**: June 17, 2025  
**Status**: CRITICAL - Multiple violations of unified documents architecture found

## Executive Summary

The RBAC implementation contains numerous violations of the unified documents architecture. While some components have been partially converted to use `storage_*` functions, many still use direct collection access patterns and hierarchical collection paths that violate the single source of truth principle.

## Critical Violations Found

### 1. Direct Collection Access (FORBIDDEN)

#### File: `src/components/rbac/rbac_db.c`
- **Lines 43-96**: Using `db_get_collection()` and `db_create_collection()` directly
- **Lines 415, 435, 670, 716, 749-787**: Using `db_update_document()` with library/collection paths
- **Lines 670, 807, 852, 861**: Using `db_get_document()` with library/collection paths
- **Lines 435, 736**: Using `db_delete_document()` with library/collection paths

```c
// ❌ FORBIDDEN - Direct collection operations
if (!db_get_collection(db, RBAC_CONFIG_COLLECTION_NAME)) {
    if (!db_create_collection(db, RBAC_CONFIG_COLLECTION_NAME)) {
        // ...
    }
}

// ❌ FORBIDDEN - Hierarchical paths
json_value_t* role_doc = db_get_document(db, STORAGE_LIBRARY, STORAGE_COLLECTION, role_id);
```

### 2. Hierarchical Collection Paths (VIOLATIONS)

#### File: `src/components/rbac/rbac_database.c`
- **Lines 32-35**: Defines hierarchical paths like `"system/permissions"`, `"system/sessions"`
- **Lines 91, 209, 295, 333, 448, 588, 687, 772**: Using `db_query_documents()` with library/collection paths
- **Line 12**: Defines `SESSIONS_COLLECTION` as `"system/sessions"`

```c
// ❌ FORBIDDEN - Hierarchical collection paths
#define RBAC_PERMISSIONS_COLLECTION "system/permissions"
#define RBAC_SESSIONS_COLLECTION "system/sessions"

// ❌ FORBIDDEN - Query with hierarchical paths
json_value_t* results = db_query_documents(db, RBAC_SYSTEM_LIBRARY, RBAC_ROLES_COLLECTION_NAME, query);
```

### 3. Mixed Architecture Patterns

#### File: `src/components/rbac/rbac_database.c`
- Some functions use `storage_*` functions (GOOD)
- Others use `db_query_documents()` with library/collection paths (BAD)
- Inconsistent approach violates single source of truth

```c
// ✅ CORRECT - Using storage functions
json_value_t* result = storage_insert_document(db, admin_role);

// ❌ WRONG - Same file using hierarchical queries
json_value_t* results = db_query_documents(db, RBAC_SYSTEM_LIBRARY, RBAC_ROLES_COLLECTION_NAME, query);
```

## Functions That Need Conversion

### 1. `rbac_database.c` - Partial Conversion Needed
- `create_default_admin_role()` - Lines 91, 295, 333: Replace `db_query_documents()` with `storage_query_documents()`
- `rbac_database_get_user_by_username()` - Line 448: Replace `db_query_documents()` with `storage_query_documents()`
- `rbac_database_create_role()` - Line 588: Replace `db_query_documents()` with `storage_query_documents()`
- `rbac_database_check_permission()` - Lines 687, 736, 772: Replace `db_query_documents()` and `db_get_document()` with storage functions

### 2. `rbac_db.c` - Major Conversion Needed
- `rbac_db_init_collections()` - Lines 43-96: Remove all `db_get_collection()` and `db_create_collection()` calls
- `rbac_db_delete_user()` - Lines 415, 435: Replace `db_update_document()` and `db_delete_document()`
- `rbac_db_delete_role()` - Lines 670, 716, 736: Replace `db_get_document()`, `db_update_document()`, `db_delete_document()`
- `rbac_db_update_role()` - Lines 749-787: Replace `db_get_document()` and `db_update_document()`
- `rbac_db_get_role()` - Line 807: Replace `db_get_document()`
- `rbac_db_add_user_to_role()` - Lines 852-861, 913-930: Replace `db_get_document()` and `db_update_document()`
- `rbac_db_remove_user_from_role()` - Lines 978-987, 1031-1059: Replace `db_get_document()` and `db_update_document()`
- `rbac_db_grant_permission()` - Lines 1106, 1140: Replace `db_get_document()` and `db_update_document()`
- `rbac_db_revoke_permission()` - Lines 1163, 1206: Replace `db_get_document()` and `db_update_document()`
- `rbac_db_check_permission()` - Line 1279: Replace `db_get_document()`

### 3. `rbac_sessions.c` - Properly Converted ✅
- This file correctly uses `storage_*` functions throughout
- Good example of proper unified documents implementation

## Required Changes

### 1. Remove Collection Management
The `rbac_db_init_collections()` function should be removed entirely. In the unified documents architecture:
- Collections don't need to be "created" - they're virtual
- Indexes are managed by the adaptive indexing system
- All documents go into the single physical `documents` collection

### 2. Convert All Queries to Unified Format
Replace all `db_query_documents(db, library, collection, query)` calls with:
```c
// Add type field to query
json_object_set(query, "type", json_create_string(doc_type));
json_object_set(query, "library", json_create_string(library));
json_object_set(query, "collection", json_create_string(collection));

// Use storage function
json_value_t* results = storage_query_documents(db, query);
```

### 3. Convert Direct Document Operations
Replace all direct document operations:
```c
// ❌ WRONG
db_get_document(db, library, collection, id);
db_update_document(db, library, collection, id, doc);
db_delete_document(db, library, collection, id);

// ✅ CORRECT
storage_get_document(db, uuid);
storage_update_document(db, uuid, doc);
storage_delete_document(db, uuid);
```

### 4. Use Document Type Constants
All RBAC documents should use the proper type constants from `document_storage.h`:
- `DOC_TYPE_NAME_USER` for users
- `DOC_TYPE_NAME_ROLE` for roles
- `DOC_TYPE_NAME_SESSION` for sessions
- `DOC_TYPE_NAME_CONFIG` for configuration

## Priority Actions

1. **IMMEDIATE**: Remove `rbac_db_init_collections()` function and all collection creation logic
2. **HIGH**: Convert all `db_query_documents()` calls to use `storage_query_documents()` with proper type fields
3. **HIGH**: Convert all direct document operations to use `storage_*` functions
4. **MEDIUM**: Ensure all new documents include mandatory fields (type, library, collection, owner)
5. **MEDIUM**: Update tests to verify unified documents compliance

## Validation Checklist

- [ ] No direct collection operations (`db_create_collection`, `db_get_collection`)
- [ ] No hierarchical collection paths (`"system/users"`, `"system/roles"`)
- [ ] All queries include `type`, `library`, and `collection` fields
- [ ] All document operations use `storage_*` functions
- [ ] No mixed architecture patterns in the same file
- [ ] All documents stored in the single physical `documents` collection

## Conclusion

The RBAC system requires significant refactoring to comply with the unified documents architecture. While some progress has been made (particularly in `rbac_sessions.c`), the majority of the RBAC codebase still uses legacy patterns that violate the single source of truth principle. These violations must be addressed to maintain architectural consistency and prevent future maintenance issues.