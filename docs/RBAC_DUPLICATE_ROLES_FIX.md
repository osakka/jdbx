# RBAC Duplicate Roles Fix

## Problem
The RBAC system was creating duplicate "Administrator" roles every time the server started. This happened because:

1. The code checked for a role with name "admin" 
2. But it created a role with name "Administrator"
3. So on next startup, it didn't find "admin" and created another "Administrator"

## Root Cause
There were inconsistencies in the RBAC initialization code:
- `rbac_database.c`: Checked for role name "admin", created permissions correctly
- `rbac_db_fix.c`: Created role with name "Administrator" 
- `rbac_minimal.c`: Created role with name "Administrator"

## Solution
1. **Code Fix**: Updated all RBAC initialization code to consistently use "admin" as the role name
   - Modified `rbac_db_fix.c` line 48
   - Modified `rbac_minimal.c` line 44

2. **Data Cleanup**: Removed duplicate role documents from the `_roles` collection
   - Deleted 23 duplicate "Administrator" roles
   - Kept only one "admin" role

## Prevention
To prevent future duplicates:

1. **Consistent Naming**: All code now uses "admin" as the default admin role name
2. **Check Before Create**: The initialization code checks if a role exists before creating it
3. **Unique Constraint**: Consider adding a unique index on the role name field:
   ```javascript
   db.createIndex("_roles", { name: 1 }, { unique: true })
   ```

## Current State
After the fix, the `_roles` collection contains:
- 1 admin role (the system administrator)
- Other custom roles (Editor, Viewer, Support, etc.)
- No duplicates

## Code Changes

### rbac_db_fix.c
```c
// Before:
json_object_set(admin_role, "name", json_create_string("Administrator"));

// After:
json_object_set(admin_role, "name", json_create_string("admin"));
```

### rbac_minimal.c
```c
// Before:
json_object_set(admin_role, "name", json_create_string("Administrator"));

// After:
json_object_set(admin_role, "name", json_create_string("admin"));
```

This ensures consistency across all RBAC implementations and prevents duplicate role creation.