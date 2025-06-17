#ifndef RBAC_UNIFIED_DOCUMENTS_H
#define RBAC_UNIFIED_DOCUMENTS_H

/**
 * RBAC Unified Documents Architecture Header
 * 
 * CRITICAL: This header enforces the unified documents architecture for RBAC.
 * ALL RBAC operations MUST use the storage_* functions with type-based queries.
 * 
 * FORBIDDEN:
 * - Creating physical collections (db_create_collection)
 * - Direct collection access (db_get_collection) 
 * - Hierarchical paths like "system/users"
 * 
 * REQUIRED:
 * - Use storage_* functions exclusively
 * - Query by document type field
 * - All documents stored in unified "default/documents" collection
 */

#include "database/document_storage.h"

/* Document type constants for RBAC - these replace collection names */
#define RBAC_DOCTYPE_USER      DOC_TYPE_NAME_USER        /* "user" */
#define RBAC_DOCTYPE_ROLE      DOC_TYPE_NAME_ROLE        /* "role" */
#define RBAC_DOCTYPE_SESSION   DOC_TYPE_NAME_SESSION     /* "session" */
#define RBAC_DOCTYPE_PERMISSION "permission"              /* "permission" - not in document_storage.h yet */
#define RBAC_DOCTYPE_CONFIG    DOC_TYPE_NAME_CONFIG      /* "config" */

/* Library names for RBAC documents */
#define RBAC_LIBRARY_SYSTEM    "system"
#define RBAC_LIBRARY_DEFAULT   "default"

/**
 * Query builders for unified documents
 * These replace direct collection queries
 */
static inline json_value_t* rbac_build_user_query(const char* library) {
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string(RBAC_DOCTYPE_USER));
    json_object_set(query, "library", json_create_string(library ? library : RBAC_LIBRARY_SYSTEM));
    return query;
}

static inline json_value_t* rbac_build_role_query(const char* library) {
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string(RBAC_DOCTYPE_ROLE));
    json_object_set(query, "library", json_create_string(library ? library : RBAC_LIBRARY_SYSTEM));
    return query;
}

static inline json_value_t* rbac_build_session_query(const char* library) {
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string(RBAC_DOCTYPE_SESSION));
    json_object_set(query, "library", json_create_string(library ? library : RBAC_LIBRARY_SYSTEM));
    return query;
}

static inline json_value_t* rbac_build_permission_query(const char* library) {
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string(RBAC_DOCTYPE_PERMISSION));
    json_object_set(query, "library", json_create_string(library ? library : RBAC_LIBRARY_SYSTEM));
    return query;
}

static inline json_value_t* rbac_build_config_query(const char* library) {
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string(RBAC_DOCTYPE_CONFIG));
    json_object_set(query, "library", json_create_string(library ? library : RBAC_LIBRARY_SYSTEM));
    return query;
}

/**
 * Add required fields for RBAC documents
 */
static inline void rbac_add_document_fields(json_value_t* doc, const char* type, const char* library) {
    json_object_set(doc, "type", json_create_string(type));
    json_object_set(doc, "library", json_create_string(library ? library : RBAC_LIBRARY_SYSTEM));
    json_object_set(doc, "collection", json_create_string(type)); /* For compatibility */
    json_object_set(doc, "owner", json_create_string("system"));
}

/* Compile-time assertion to prevent old-style collection usage 
 * NOTE: Commented out temporarily since rbac_db.h still defines these for backward compatibility
 * TODO: Remove these constants from rbac_db.h once all code is converted
 */
/*
#ifdef RBAC_USERS_COLLECTION_NAME
  #error "RBAC_USERS_COLLECTION_NAME is forbidden! Use RBAC_DOCTYPE_USER with storage_* functions"
#endif

#ifdef RBAC_ROLES_COLLECTION_NAME  
  #error "RBAC_ROLES_COLLECTION_NAME is forbidden! Use RBAC_DOCTYPE_ROLE with storage_* functions"
#endif
*/

#endif /* RBAC_UNIFIED_DOCUMENTS_H */