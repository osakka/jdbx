# CRITICAL ARCHITECTURE CLARIFICATION

**Date**: June 17, 2025  
**Status**: URGENT - We've been fixing things incorrectly!

## The Correct Architecture

### Three Layers (Top to Bottom)

1. **Application Layer (Business Logic)**
   - RBAC, Authentication, Session Management
   - **MUST USE**: `virtual_*` functions
   - **NEVER USE**: `storage_*` functions directly

2. **Virtual Layer (Logical Operations)**
   - User management, Role management, Collection management
   - **Functions**: `virtual_create_user()`, `virtual_query_users()`, etc.
   - Handles business logic, validation, field requirements

3. **Storage Layer (Physical Storage)**
   - Direct unified documents collection access
   - **Functions**: `storage_insert_document()`, `storage_query_documents()`, etc.
   - ONLY used by virtual layer implementation

## What We've Been Doing Wrong

We've been converting RBAC code to use `storage_*` functions directly. This is WRONG!

### ❌ INCORRECT Fix (What we just did)
```c
// In rbac_db.c
json_value_t* role_doc = storage_get_document(db, role_id);
```

### ✅ CORRECT Fix (What we should do)
```c
// In rbac_db.c
json_value_t* role_doc = virtual_get_role(db, role_id);
```

## The Right Conversions

### For RBAC Operations
```c
// ❌ OLD (Using hierarchical)
db_get_document(db, "system", "roles", role_id)

// ❌ WRONG FIX (Direct storage)
storage_get_document(db, role_id)

// ✅ CORRECT FIX (Virtual layer)
virtual_get_role(db, role_id)
```

### For User Operations
```c
// ❌ OLD
db_query_documents(db, "system", "users", query)

// ❌ WRONG FIX
storage_query_documents(db, query)

// ✅ CORRECT FIX
virtual_query_users(db, "system", filters)
```

## What Files Need Virtual Methods

1. **RBAC Files** - Should use virtual_* for users/roles
   - rbac_db.c
   - rbac_database.c
   - rbac_sessions.c

2. **API Files** - Should use virtual_* for business entities
   - auth_session_api.c
   - rbac_api.c
   - Most API endpoints

3. **Database Internal Files** - CAN use storage_*
   - database.c (implements storage layer)
   - document_storage.c
   - Internal database operations

## Action Items

1. **STOP** converting to storage_* in RBAC/API files
2. **CHECK** if virtual_* functions exist for the operations
3. **CREATE** virtual_* functions if missing
4. **CONVERT** to use virtual_* instead of storage_*

## The Rule

**If it's business logic (users, roles, sessions, permissions) → Use virtual_***  
**If it's database internals → Use storage_***

Never let application code directly touch physical storage!