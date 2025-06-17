# Old Style Database Violations - Complete Audit

**Date**: June 17, 2025  
**Status**: CRITICAL - Multiple violations found

## Summary

Found numerous old-style database calls that violate the unified documents architecture. These MUST be converted to use storage_* functions.

## Violations by File

### 1. `/opt/jdbx/src/components/utils/library_metrics.c`
- Line: `db_delete_document(db, STORAGE_LIBRARY, collection_name, id->value.string)`
- **Fix**: Convert to `storage_delete_document(db, id->value.string)`

### 2. `/opt/jdbx/src/components/utils/metrics_persistence.c`
- Line: `db_get_document(mp->db, STORAGE_LIBRARY, STORAGE_COLLECTION, metric_id)`
- **Fix**: Convert to `storage_get_document(mp->db, metric_id)`

### 3. `/opt/jdbx/src/components/utils/database_config.c`
- Multiple violations with `db_get_document(db, STORAGE_LIBRARY, STORAGE_COLLECTION, CONFIG_DOCUMENT_ID)`
- **Fix**: Convert to `storage_get_document(db, CONFIG_DOCUMENT_ID)`

### 4. `/opt/jdbx/src/components/core/api.c`
- Multiple violations for document CRUD operations
- All using `db_get_document(ctx->db, STORAGE_LIBRARY, STORAGE_COLLECTION, doc_id)`
- **Fix**: Convert ALL to `storage_get_document(ctx->db, doc_id)`

### 5. `/opt/jdbx/src/components/database/collection_metadata.c`
- Line: `db_get_document(db, STORAGE_LIBRARY, collection_name, COLLECTION_META_ID)`
- **Fix**: Convert to `storage_get_document(db, COLLECTION_META_ID)`

### 6. `/opt/jdbx/src/components/database/collection_defaults.c`
- Multiple violations with hardcoded collections "users", "roles"
- **Fix**: Convert to unified storage queries

### 7. `/opt/jdbx/src/components/database/versioning_policy.c`
- Version document operations using old style
- **Fix**: Convert to storage_* functions

### 8. `/opt/jdbx/src/components/database/database.c`
- Contains the IMPLEMENTATION of old functions!
- `db_get_document`, `db_delete_document` etc.
- **Fix**: These should be marked DEPRECATED or removed

### 9. `/opt/jdbx/src/components/database/js_integration.c`
- JavaScript integration using old style
- **Fix**: Convert to storage_* functions

## Priority Order

1. **HIGHEST**: Fix `api.c` - This handles all REST API endpoints
2. **HIGH**: Fix authentication-related files that affect login/session
3. **MEDIUM**: Fix utility files (metrics, config)
4. **LOW**: Fix database internal files

## Next Steps

1. Start with `api.c` as it directly affects all API endpoints
2. Test each fix thoroughly before moving to next
3. Ensure zero regressions
4. Compile and test after each file
5. Update this document as we progress

## Fix Template

For each violation:
```c
// ❌ OLD (FORBIDDEN)
db_get_document(ctx->db, STORAGE_LIBRARY, STORAGE_COLLECTION, doc_id)

// ✅ NEW (CORRECT)
storage_get_document(ctx->db, doc_id)
```

## Progress Tracking

- [x] api.c (7 violations fixed)
- [ ] rbac_db.c (9 violations remaining)
- [ ] js_engine.c (8 violations)
- [ ] library_metadata.c (4 violations)
- [ ] rbac_api.c (3 violations)
- [ ] js_integration.c (3 violations)
- [ ] database_config.c (2 violations)
- [ ] versioning_policy.c (2 violations)
- [ ] collection_defaults.c (2 violations)
- [ ] metrics_persistence.c (1 violation)
- [ ] library_metrics.c (1 violation)
- [ ] collection_metadata.c (1 violation)
- [ ] session_terminate_api.c (1 violation)
- [ ] js_native_storage.c (1 violation)
- [ ] Mark old functions as DEPRECATED