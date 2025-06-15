#ifndef UNIFIED_DOCUMENTS_H
#define UNIFIED_DOCUMENTS_H

#include "utils/json.h"
#include "database/database.h"

/* Document types */
typedef enum {
    DOC_TYPE_UNKNOWN = 0,
    DOC_TYPE_USER,
    DOC_TYPE_ROLE,
    DOC_TYPE_MEMBERSHIP,
    DOC_TYPE_PERMISSION,
    DOC_TYPE_SESSION,
    DOC_TYPE_LIBRARY,
    DOC_TYPE_COLLECTION,
    DOC_TYPE_FUNCTION,
    DOC_TYPE_VALIDATOR,
    DOC_TYPE_TRANSFORMER,
    DOC_TYPE_METRIC,
    DOC_TYPE_CONFIG,
    DOC_TYPE_INDEX,
    DOC_TYPE_SCHEMA,
    DOC_TYPE_VERSION,
    DOC_TYPE_AUDIT,
    DOC_TYPE_CACHE_ENTRY
} document_type_t;

/* System actors */
#define SYSTEM_USER_ADMIN          "system-admin"
#define SYSTEM_USER_METRICS        "system-metrics"
#define SYSTEM_USER_INDEXER        "system-indexer"
#define SYSTEM_USER_PERSISTENCE    "system-persistence"
#define SYSTEM_USER_CACHE          "system-cache"
#define SYSTEM_USER_AUDIT          "system-audit"

/* Core collection name */
#define DOCUMENTS_COLLECTION       "documents"

/* Document type strings */
const char* document_type_to_string(document_type_t type);
document_type_t document_type_from_string(const char* type_str);

/* Initialize unified documents system */
int unified_documents_init(database_t* db);

/* Create system actors */
int create_system_actors(database_t* db);

/* Query documents by type */
json_value_t* query_documents_by_type(database_t* db, const char* type,
                                      json_value_t* additional_query);

/* Query documents by type and library/collection */
json_value_t* query_documents_by_location(database_t* db, const char* type,
                                         const char* library, const char* collection,
                                         json_value_t* additional_query);

/* Create document with proper type and ownership */
json_value_t* create_typed_document(database_t* db, const char* type,
                                   const char* name, const char* owner_id,
                                   json_value_t* content);

/* Validate document has required fields */
int validate_document_structure(json_value_t* doc);

/* Add system fields to document */
void add_document_system_fields(json_value_t* doc, const char* type,
                               const char* library, const char* collection,
                               const char* owner_id);

#endif /* UNIFIED_DOCUMENTS_H */