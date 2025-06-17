#ifndef STORAGE_ASSERTIONS_H
#define STORAGE_ASSERTIONS_H

/**
 * Compile-time and runtime assertions to prevent architectural violations
 */

/* Compile-time string comparison for literal strings */
#define STORAGE_ASSERT_UNIFIED_COLLECTION(lib, coll) do { \
    _Static_assert(__builtin_constant_p(lib) && __builtin_constant_p(coll), \
                   "Library and collection must be compile-time constants for this check"); \
    if (__builtin_constant_p(lib) && __builtin_constant_p(coll)) { \
        if (__builtin_strcmp(lib, "default") != 0 || __builtin_strcmp(coll, "documents") != 0) { \
            _Static_assert(0, "ERROR: Using non-unified collection! Use 'default/documents' with type discrimination"); \
        } \
    } \
} while(0)

/* Runtime assertion with detailed error message */
#define STORAGE_RUNTIME_CHECK_UNIFIED(lib, coll) do { \
    if (strcmp(lib, "default") != 0 || strcmp(coll, "documents") != 0) { \
        LOG_ERROR("ARCHITECTURAL VIOLATION: Accessing non-unified collection %s/%s", lib, coll); \
        LOG_ERROR("ALL documents must be in 'default/documents' with type discrimination!"); \
        LOG_ERROR("Example: Use {\"type\": \"user\"} in query, not 'system/users' path"); \
        assert(0 && "Architectural violation: non-unified collection access"); \
    } \
} while(0)

/* Wrapper macro for safe db_query_documents calls */
#define DB_QUERY_UNIFIED(db, query) \
    db_query_documents(db, "default", "documents", query)

/* Deprecated macro to catch old-style calls */
#define DB_QUERY_COLLECTION(db, lib, coll, query) \
    _Static_assert(0, "DEPRECATED: Use DB_QUERY_UNIFIED with type discrimination instead")

/* Helper to ensure query has type field */
#define QUERY_ENSURE_TYPE(query, type_str) do { \
    if (!json_object_get(query, "type")) { \
        LOG_WARNING("Query missing 'type' field - adding type='%s'", type_str); \
        json_object_set(query, "type", json_create_string(type_str)); \
    } \
} while(0)

/* Validation helper for function entry */
#define VALIDATE_STORAGE_CALL() do { \
    LOG_TRACE("Storage layer call from %s() - ensure type discrimination is used", __func__); \
} while(0)

#define VALIDATE_VIRTUAL_CALL() do { \
    LOG_TRACE("Virtual layer call from %s() - business logic with automatic type handling", __func__); \
} while(0)

#endif /* STORAGE_ASSERTIONS_H */