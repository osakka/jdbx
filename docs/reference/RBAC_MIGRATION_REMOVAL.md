# RBAC Migration Path Removal

This document describes the changes made to completely remove the file-based RBAC system and transition fully to database-based RBAC with no fallback or migration mechanism.

## Changes Made

1. **rbac_enhanced.c**: 
   - Updated to ignore the `path` parameter completely
   - Removed all code that checked for or tried to load from a file-based RBAC source
   - Added parameter suppression to avoid compiler warnings
   - Updated function documentation to note that the parameter is ignored

2. **rbac_db.c**:
   - Marked `rbac_db_migrate_from_file` as deprecated with a warning message
   - Modified it to always return failure (0) to prevent its use
   - Added parameter suppression to avoid compiler warnings

3. **main.c**:
   - Removed the `rbac_file_path` variable
   - Updated the call to `rbac_enhanced_init` to pass NULL directly
   - Updated the `rbac_api_register_routes` call to remove the file path parameter

4. **Documentation**:
   - Updated DATABASE_RBAC.md to indicate that file-based RBAC is completely removed
   - Changed "Seamless transition" to "Complete cutover" in design principles
   - Added a warning about the need for manual setup when upgrading from previous versions

## Rationale

The decision to completely remove file-based RBAC with no migration path was made to:

1. Simplify the codebase by having a single source of truth
2. Remove potential security issues from mixing storage mechanisms
3. Enforce a clean transition to the more robust database-based approach
4. Follow the project guidelines to have "one source of truth" and "one clear goal"

## Impact on Existing Systems

Existing systems that used file-based RBAC configurations will need to manually set up their RBAC configuration through the API. This includes:

1. Creating users with the same usernames and new passwords
2. Creating roles with the same permissions
3. Assigning users to the appropriate roles

While this requires manual intervention, it ensures a clean transition and forces a review of existing permissions, which is beneficial for security.

## Future Considerations

In the future, we might consider:

1. Providing a standalone migration utility (separate from the server) to help existing users transition
2. Adding a bulk import endpoint to facilitate moving multiple users/roles at once
3. Further enhancing the RBAC API with additional management features

However, the core server code will maintain its commitment to database-only RBAC without fallbacks.