# UUID Field Naming Audit

**Date**: June 17, 2025  
**Status**: CRITICAL - Multiple naming inconsistencies found

## Current UUID Field Names Found

1. **"uuid"** - Used in most places as the document identifier
2. **"id"** - Sometimes used interchangeably with uuid
3. **"_id"** - MongoDB-style identifier in some places
4. **"doc_id"** - Document ID in some contexts
5. **"session_id"** - For session documents
6. **"user_id"** - References to user UUIDs
7. **"role_id"** - References to role UUIDs

## Architectural Decision Required

### Option 1: Single Field Name "uuid"
- **Pros**: Consistent, clear, no ambiguity
- **Cons**: Need to update many references

### Option 2: Context-Specific Names
- **Pros**: More semantic (user_id, role_id)
- **Cons**: Inconsistent, requires mapping logic

## Recommendation

Use **"uuid"** as the PRIMARY document identifier field name everywhere.
- This is the document's own ID
- References to other documents use semantic names (user_id, role_id)

## Virtual Layer Requirements

The virtual layer needs to support:
1. **Lookup by UUID**: `virtual_get_user_by_uuid(db, uuid)`
2. **Lookup by Name**: `virtual_get_user_by_name(db, username, library)`
3. **Lookup by Any Field**: `virtual_find_user(db, field_name, field_value, library)`

## Implementation Plan

### Phase 1: Virtual Layer Enhancement
1. Add UUID lookup functions to virtual layer
2. Add generic field lookup functions
3. Ensure all virtual functions handle both name and UUID lookups

### Phase 2: UUID Field Standardization
1. Audit all document creation to use "uuid" field
2. Update all lookups to check "uuid" field
3. Add migration logic for old field names

### Phase 3: RBAC Conversion
1. Convert RBAC to use enhanced virtual layer
2. Remove all direct storage access from RBAC
3. Test thoroughly

### Phase 4: API Conversion
1. Convert API endpoints to use virtual layer
2. Remove all direct storage access from APIs
3. Test thoroughly

## Next Steps

1. Implement enhanced virtual layer functions
2. Create comprehensive test suite
3. Convert one module at a time
4. Verify no regressions at each step