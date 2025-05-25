# RBAC Duplicate Admin Users/Roles Fix

## Problem
The database had multiple admin users and roles with different structures and password hash formats:
- User with _id "user_admin" using PBKDF2 hash (correct)
- User with auto-generated _id using bcrypt ($2a$) hash (incorrect)
- Potentially duplicate admin roles

## Root Cause
1. Multiple RBAC initialization paths creating admin users without checking for existing ones
2. `rbac_database.c` only checked for existence by specific _id, not by username
3. Different parts of the code used different password hashing algorithms

## Solution Implemented

### 1. Enhanced Duplicate Checking
Modified `create_default_admin_user()` and `create_default_admin_role()` to check for existence by both:
- Specific _id (user_admin, role_admin)
- Username/name field to prevent any duplicates

### 2. Cleanup Function
Added `cleanup_rbac_duplicates()` that:
- Finds all users with username "admin"
- Keeps only the one with _id "user_admin"
- Deletes all others
- Same for roles with name "admin", keeping only "role_admin"

### 3. Standardization
- All admin users use _id "user_admin"
- All admin roles use _id "role_admin"
- Password hashing uses PBKDF2 format from rbac.c

## Code Changes

### rbac_database.c
```c
/* Clean up duplicate admin users and roles */
static void cleanup_rbac_duplicates(struct database* db) {
    // Implementation to find and remove duplicates
}

/* Enhanced checks in create functions */
int create_default_admin_user(struct database* db) {
    // Check by _id
    // Also check by username to prevent duplicates
}
```

## Result
- Single admin user with _id "user_admin"
- Single admin role with _id "role_admin"
- Consistent PBKDF2 password hashing
- No more duplicate entries on server restart

## Prevention
The enhanced checks ensure that even if multiple initialization paths are triggered, only one admin user and role will exist in the database.