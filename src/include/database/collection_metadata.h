#ifndef COLLECTION_METADATA_H
#define COLLECTION_METADATA_H

#include "utils/json.h"
#include "database/index_metadata.h"
#include "rbac/rbac.h"

/* Collection metadata - stored as _meta document in each collection */
typedef struct collection_metadata {
    char* collection_name;
    
    /* Multi-role permissions for the collection */
    rbac_permissions_t permissions;
    
    /* JSON Schema for validation */
    json_value_t* schema;
    
    /* Versioning configuration */
    struct {
        bool enabled;
        int max_versions;
        char** exclude_fields;
        size_t exclude_count;
    } versioning;
    
    /* Indexes with unique constraints */
    index_metadata_t* indexes;
    size_t index_count;
    
    /* Embedded functions */
    json_value_t* functions;  /* Object with field names and function arrays */
    
    /* Collection-level functions */
    json_value_t* collection_functions;  /* pre_insert, post_update, etc. */
    
} collection_metadata_t;

/* Reserved document ID for collection metadata */
#define COLLECTION_META_ID "_meta"

/* Function trigger points */
#define TRIGGER_PRE_INSERT   "pre_insert"
#define TRIGGER_POST_INSERT  "post_insert"
#define TRIGGER_PRE_UPDATE   "pre_update"
#define TRIGGER_POST_UPDATE  "post_update"
#define TRIGGER_PRE_DELETE   "pre_delete"
#define TRIGGER_POST_DELETE  "post_delete"
#define TRIGGER_VALIDATE     "validate"
#define TRIGGER_TRANSFORM    "transform"

/* Load collection metadata from database */
collection_metadata_t* collection_metadata_load(struct database* db, 
                                               const char* collection_name);

/* Save collection metadata to database */
int collection_metadata_save(struct database* db,
                            const char* collection_name,
                            collection_metadata_t* metadata);

/* Free collection metadata */
void collection_metadata_free(collection_metadata_t* metadata);

/* Create default metadata for new collection */
collection_metadata_t* collection_metadata_create_default(const char* collection_name);

/* Check if a field has unique constraint */
bool collection_field_is_unique(collection_metadata_t* metadata, const char* field_name);

/* Get functions for a specific trigger point */
json_value_t* collection_get_functions(collection_metadata_t* metadata,
                                      const char* trigger_point,
                                      const char* field_name);

#endif /* COLLECTION_METADATA_H */