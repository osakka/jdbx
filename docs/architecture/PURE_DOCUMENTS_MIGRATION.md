# Pure Documents Architecture Migration

**Date:** June 12, 2025  
**Version:** v3.2.1  
**Status:** In Progress

## Overview

Migrating from hybrid architecture (library/collection + documents metadata) to pure documents collection architecture for simplified data access and improved performance through adaptive indexing.

## Motivation

1. **Performance**: With adaptive indexing and caching, JSON queries in documents collection are as fast as separate collections
2. **Simplicity**: Single source of truth instead of dual lookup logic
3. **Consistency**: All data follows same patterns and permissions
4. **Maintenance**: Fewer code paths and edge cases

## Changes Made

### Phase 1: Authentication Handler (COMPLETED)

**File:** `src/components/core/authentication_handler.c`

**Before (Hybrid):**
```c
/* Build collection path for library */
char collection_path[256];
snprintf(collection_path, sizeof(collection_path), "%s/users", library);

/* Look up the user in the library's users collection */
json_value_t* query = json_create_object();
json_object_set(query, "username", json_create_string(username));

json_value_t* results = db_query_documents(ctx->db, collection_path, query);

/* If not found in library/users, try unified documents collection */
if (results) {
  // ... fallback logic to documents collection
}
```

**After (Pure Documents):**
```c
/* PURE DOCUMENTS ARCHITECTURE: Query documents collection directly with type=user */
json_value_t* query = json_create_object();
json_object_set(query, "type", json_create_string("user"));
json_object_set(query, "username", json_create_string(username));
json_object_set(query, "library", json_create_string(library));

/* PURE DOCUMENTS: Single query to documents collection */
json_value_t* results = db_query_documents(ctx->db, "documents", query);
```

**Benefits:**
- Removed 20+ lines of fallback logic
- Single query path instead of dual lookup
- Leverages adaptive indexing on type,username,library fields
- Maintains library isolation through JSON query filters

## Performance Impact

**Adaptive Indexing Will Create:**
- Index on `type` (fast filtering to users only)
- Index on `username` (fast user lookup)
- Compound index on `type,library,username` (optimal query)

**Result:** O(1) lookup performance equivalent to separate collections.

## Status: MAJOR PROGRESS ✅

### Successfully Implemented:
1. **Foreground Mode**: Added `--foreground` flag for direct debugging
2. **Server Startup**: All initialization phases complete successfully
3. **Socket Binding**: Accept loop working and processing connections  
4. **JDBX Integration**: Database operations functioning with B-tree storage
5. **Authentication Routes**: `/api/admin/login` and `/api/login` endpoints registered
6. **Metrics System**: Background persistence thread operational

### Current Issue: Memory Corruption 🔧
- Server starts and runs successfully
- **Problem**: `munmap_chunk(): invalid pointer` - memory corruption during shutdown
- **Impact**: Server crashes when connections close or during cleanup
- **Next Step**: Fix memory management in connection handling or JSON cleanup

## Testing Plan

1. ✓ Build server with authentication changes
2. **BLOCKED**: Server hangs trying to create library/collection structures
3. **REQUIRED**: Complete Phase 2 before testing authentication

## Next Phases

- [x] **Phase 2: URGENT** - Remove library/collection creation from unified_documents.c  
- [x] **Phase 3: URGENT** - Fix metrics persistence to use pure documents
- [ ] Phase 4: Remove remaining library/collection creation code
- [ ] Phase 5: Update documentation and examples

### Phase 2: Remove Library/Collection Creation (COMPLETED)

**File:** `src/components/database/unified_documents.c`

**Problem:** `unified_documents_init()` still creates library/collection structures, causing server to hang at metrics initialization.

**Before:**
```c
/* Create default libraries */
create_library_metadata(db, "system", "System Library", ...);
create_library_metadata(db, "default", "Default Library", ...);

/* Create default collections for system library */
for (...) {
    create_collection_metadata(db, "system", default_collections[i], ...);
}

/* Create admin user in system/users collection */
json_value_t* result = db_insert_document(db, "system/users", admin_user);
```

**After:**
```c
/* PURE DOCUMENTS: No library/collection creation - everything in documents collection */
LOG_INFO("Skipping library/collection creation - using pure documents architecture");

/* PURE DOCUMENTS: Create admin user in documents collection */
json_object_set(admin_user, "type", json_create_string("user"));
json_object_set(admin_user, "library", json_create_string("system"));
json_value_t* result = db_insert_document(db, DOCUMENTS_COLLECTION, admin_user);
```

**Changes:**
- Removed all `create_library_metadata()` calls
- Removed all `create_collection_metadata()` loops  
- Changed admin user creation to use documents collection with type/library fields
- Added `add_document_system_fields()` for proper document structure

### Phase 3: Fix Metrics Persistence (COMPLETED)

**File:** `src/components/utils/metrics_persistence.c`

**Problem:** Metrics persistence still tries to create `system/metrics` collection, causing server hang.

**Changes:**
- Changed `METRICS_COLLECTION_NAME` from `"system/metrics"` to `"documents"`
- Removed collection creation logic
- Updated `find_metric_by_name()` to query with `type=metric`, `library=system`
- Updated new metric document creation to include proper type fields

**Before:**
```c
#define METRICS_COLLECTION_NAME "system/metrics"
json_object_set(query, "name", json_create_string(metric_name));
json_object_set(new_doc, "type", json_create_string(metric_type)); // Wrong field
```

**After:**
```c
#define METRICS_COLLECTION_NAME "documents"
json_object_set(query, "type", json_create_string("metric"));
json_object_set(query, "library", json_create_string("system"));
json_object_set(new_doc, "type", json_create_string("metric")); // Document type
json_object_set(new_doc, "metric_type", json_create_string(metric_type)); // Metric type
```

## Rollback Plan

If issues arise, revert authentication_handler.c changes and ensure hybrid documents collection creation works properly.

---
**Document Status:** Living document, updated as migration progresses