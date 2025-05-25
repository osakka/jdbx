# RBAC Universal Standardization and Duplicate Prevention

## Problem
The database had inconsistent user and role management:
- Some users/roles used auto-generated IDs (doc-xxx-xxx)
- Some used hardcoded IDs (user_admin, role_admin)
- Multiple users could have the same username
- Multiple roles could have the same name
- No consistent ID generation scheme

## Root Cause
1. No standardized ID generation for users and roles
2. Duplicate checking only by _id, not by username/name
3. Different initialization paths creating inconsistent data
4. Special handling for admin instead of universal rules

## Solution Implemented - Universal Standardization

### 1. Standardized ID Generation
Implemented universal ID generation functions:
- `generate_user_id(username)` → Returns "user_<username>"
- `generate_role_id(rolename)` → Returns "role_<rolename>"

This ensures:
- All users have predictable IDs based on username
- All roles have predictable IDs based on role name
- No more auto-generated doc-xxx-xxx IDs
- Consistent naming across the entire system

### 2. Universal Duplicate Prevention
- ALL user creation checks for duplicates by both ID and username
- ALL role creation checks for duplicates by both ID and name
- Not just admin - applies to every user and role

### 3. Comprehensive Cleanup
Added `cleanup_all_rbac_duplicates()` that:
- Scans ALL users and roles (not just admin)
- Groups by username/name
- Keeps only entries with standardized IDs
- Deletes any duplicates or non-standard IDs

### 4. Consistent User/Role Creation
- `rbac_database_create_user()` uses standardized IDs
- `rbac_database_create_role()` uses standardized IDs
- Both prevent duplicates at creation time
- Same rules for admin and regular users/roles

## Code Changes

### rbac_database.c
```c
/* Generate standardized IDs for ALL users and roles */
static char* generate_user_id(const char* username) {
    return "user_<username>";
}

static char* generate_role_id(const char* rolename) {
    return "role_<rolename>";
}

/* Universal cleanup for ALL duplicates */
static void cleanup_all_rbac_duplicates(struct database* db) {
    // Removes ALL duplicates, not just admin
}

/* Consistent creation for ALL users */
rbac_user_t* rbac_database_create_user(...) {
    char* user_id = generate_user_id(username);
    // Check for duplicates by ID AND username
}

/* Consistent creation for ALL roles */
rbac_role_t* rbac_database_create_role(...) {
    char* role_id = generate_role_id(rolename);
    // Check for duplicates by ID AND name
}
```

## Result
- ALL users follow pattern: _id = "user_<username>"
- ALL roles follow pattern: _id = "role_<rolename>"
- No special cases or exceptions
- Usernames and role names are unique
- Consistent PBKDF2 password hashing for all users
- True single source of truth

## Benefits
1. **Predictable IDs**: Can derive ID from username/role name
2. **No duplicates**: Username/role name uniqueness enforced
3. **Consistency**: Same rules for all users, no exceptions
4. **Maintainability**: Clear, simple ID generation logic
5. **Data integrity**: Prevents inconsistent states