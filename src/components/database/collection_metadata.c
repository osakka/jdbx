#include "database/collection_metadata.h"
#include "database/document_storage.h"
#include "database/database.h"
#include "database/virtual_layer.h"
#include "utils/logger.h"
#include "utils/json_helpers.h"
#include "utils/json_deep_copy.h"
#include "utils/buffer_pool.h"
#include <string.h>
#include <stdlib.h>

/* Forward declaration */
static uint8_t parse_permission_string(const char* perm_str);

/* Load collection metadata from database */
collection_metadata_t* collection_metadata_load(database_t* db, const char* collection_name) {
    if (!db || !collection_name) {
        return NULL;
    }
    
    /* Try to load _meta document */
    json_value_t* meta_doc = db_get_document(db, STORAGE_LIBRARY, STORAGE_COLLECTION, COLLECTION_META_ID);
    if (!meta_doc) {
        /* No metadata exists, create default */
        LOG_DEBUG("No metadata found for collection %s, using defaults", collection_name);
        return collection_metadata_create_default(collection_name);
    }
    
    LOG_DEBUG("Loaded metadata for collection %s", collection_name);
    
    collection_metadata_t* metadata = BUFFER_ALLOC(sizeof(collection_metadata_t));
    if (!metadata) {
        json_free(meta_doc);
        return NULL;
    }
    
    metadata->collection_name = BUFFER_STRDUP(collection_name);
    
    /* Parse permissions */
    json_value_t* perms = json_object_get(meta_doc, "permissions");
    if (perms && perms->type == JSON_OBJECT) {
        /* Parse owner permissions */
        json_value_t* owner_perms = json_object_get(perms, "owner_perms");
        if (owner_perms && owner_perms->type == JSON_STRING) {
            metadata->permissions.owner_perms = parse_permission_string(owner_perms->value.string);
        }
        
        /* Parse world permissions */
        json_value_t* world_perms = json_object_get(perms, "world_perms");
        if (world_perms && world_perms->type == JSON_STRING) {
            metadata->permissions.world_perms = parse_permission_string(world_perms->value.string);
        }
        
        /* Parse multiple owners */
        json_value_t* owners = json_object_get(perms, "owners");
        if (owners && owners->type == JSON_ARRAY) {
            metadata->permissions.owner_count = owners->value.array.size;
            metadata->permissions.owner_ids = BUFFER_ALLOC(sizeof(char*));
            for (size_t i = 0; i < owners->value.array.size; i++) {
                json_value_t* owner = json_array_get(owners, i);
                if (owner->type == JSON_STRING) {
                    metadata->permissions.owner_ids[i] = BUFFER_STRDUP(owner->value.string);
                }
            }
        }
        
        /* Parse role permissions - simplified for now */
        metadata->permissions.role_perms = NULL;
        metadata->permissions.role_count = 0;
    }
    
    /* Keep references to JSON schema and functions */
    metadata->schema = json_object_get(meta_doc, "schema");
    metadata->functions = json_object_get(meta_doc, "functions");
    metadata->collection_functions = json_object_get(meta_doc, "collection_functions");
    
    /* Parse versioning config */
    json_value_t* versioning = json_object_get(meta_doc, "versioning");
    if (versioning && versioning->type == JSON_OBJECT) {
        json_value_t* enabled = json_object_get(versioning, "enabled");
        json_value_t* max_versions = json_object_get(versioning, "max_versions");
        json_value_t* exclude = json_object_get(versioning, "exclude_fields");
        
        metadata->versioning.enabled = enabled && enabled->type == JSON_BOOLEAN && enabled->value.boolean;
        metadata->versioning.max_versions = max_versions && max_versions->type == JSON_INTEGER ? 
                                           max_versions->value.integer : 10;
        
        if (exclude && exclude->type == JSON_ARRAY) {
            metadata->versioning.exclude_count = exclude->value.array.size;
            metadata->versioning.exclude_fields = BUFFER_ALLOC(sizeof(char*));
            for (size_t i = 0; i < exclude->value.array.size; i++) {
                json_value_t* field = json_array_get(exclude, i);
                if (field->type == JSON_STRING) {
                    metadata->versioning.exclude_fields[i] = BUFFER_STRDUP(field->value.string);
                }
            }
        }
    }
    
    /* Parse indexes */
    json_value_t* indexes = json_object_get(meta_doc, "indexes");
    if (indexes && indexes->type == JSON_ARRAY) {
        metadata->index_count = indexes->value.array.size;
        metadata->indexes = BUFFER_ALLOC(sizeof(index_metadata_t));
        
        for (size_t i = 0; i < indexes->value.array.size; i++) {
            json_value_t* idx = json_array_get(indexes, i);
            if (idx->type != JSON_OBJECT) continue;
            
            json_value_t* field = json_object_get(idx, "field");
            json_value_t* type = json_object_get(idx, "type");
            json_value_t* unique = json_object_get(idx, "unique");
            
            if (field && field->type == JSON_STRING) {
                metadata->indexes[i].field_name = BUFFER_STRDUP(field->value.string);
                metadata->indexes[i].type = INDEX_META_TYPE_HASH; /* Default */
                
                if (type && type->type == JSON_STRING) {
                    if (strcmp(type->value.string, "btree") == 0) {
                        metadata->indexes[i].type = INDEX_META_TYPE_BTREE;
                    }
                }
                
                metadata->indexes[i].unique = unique && unique->type == JSON_BOOLEAN && 
                                              unique->value.boolean;
            }
        }
    }
    
    /* Don't free meta_doc as we're keeping references to its contents */
    return metadata;
}

/* Parse permission string like "rwxd" to bitmask */
static uint8_t parse_permission_string(const char* perm_str) {
    uint8_t perms = 0;
    if (!perm_str) return 0;
    
    if (strchr(perm_str, 'r')) perms |= RBAC_READ;
    if (strchr(perm_str, 'w')) perms |= RBAC_WRITE;
    if (strchr(perm_str, 'x')) perms |= RBAC_EXECUTE;
    if (strchr(perm_str, 'd')) perms |= RBAC_DELETE;
    if (strchr(perm_str, 'a')) perms |= RBAC_ADMIN;
    
    return perms;
}

/* Create default metadata for new collection */
collection_metadata_t* collection_metadata_create_default(const char* collection_name) {
    collection_metadata_t* metadata = BUFFER_ALLOC(sizeof(collection_metadata_t));
    if (!metadata) return NULL;
    
    metadata->collection_name = BUFFER_STRDUP(collection_name);
    
    /* Default permissions: owner=all, world=read */
    metadata->permissions.owner_perms = RBAC_READ | RBAC_WRITE | RBAC_DELETE | RBAC_EXECUTE | RBAC_ADMIN;
    metadata->permissions.world_perms = RBAC_READ;
    metadata->permissions.owner_ids = NULL;
    metadata->permissions.owner_count = 0;
    
    /* Default versioning: enabled with 10 versions */
    metadata->versioning.enabled = true;
    metadata->versioning.max_versions = 10;
    
    return metadata;
}

/* Check if a field has unique constraint */
bool collection_field_is_unique(collection_metadata_t* metadata, const char* field_name) {
    if (!metadata || !field_name) return false;
    
    /* Check in indexes */
    for (size_t i = 0; i < metadata->index_count; i++) {
        if (metadata->indexes[i].field_name &&
            strcmp(metadata->indexes[i].field_name, field_name) == 0 &&
            metadata->indexes[i].unique) {
            return true;
        }
    }
    
    /* Check in schema if marked as unique */
    if (metadata->schema && metadata->schema->type == JSON_OBJECT) {
        json_value_t* properties = json_object_get(metadata->schema, "properties");
        if (properties && properties->type == JSON_OBJECT) {
            json_value_t* field_schema = json_object_get(properties, field_name);
            if (field_schema && field_schema->type == JSON_OBJECT) {
                json_value_t* unique = json_object_get(field_schema, "unique");
                if (unique && unique->type == JSON_BOOLEAN && unique->value.boolean) {
                    return true;
                }
            }
        }
    }
    
    return false;
}

/* Free collection metadata */
void collection_metadata_free(collection_metadata_t* metadata) {
    if (!metadata) return;
    
    BUFFER_FREE(metadata->collection_name);
    
    /* Free multiple owners */
    for (size_t i = 0; i < metadata->permissions.owner_count; i++) {
        BUFFER_FREE(metadata->permissions.owner_ids[i]);
    }
    BUFFER_FREE(metadata->permissions.owner_ids);
    
    /* Free versioning exclude fields */
    for (size_t i = 0; i < metadata->versioning.exclude_count; i++) {
        BUFFER_FREE(metadata->versioning.exclude_fields[i]);
    }
    BUFFER_FREE(metadata->versioning.exclude_fields);
    
    /* Free indexes */
    for (size_t i = 0; i < metadata->index_count; i++) {
        BUFFER_FREE(metadata->indexes[i].field_name);
        BUFFER_FREE(metadata->indexes[i].transform_function);
        BUFFER_FREE(metadata->indexes[i].validate_function);
    }
    BUFFER_FREE(metadata->indexes);
    
    /* Note: We don't free schema, functions, etc. as they're references to the JSON document */
    
    BUFFER_FREE(metadata);
}

/* Save collection metadata to database */
int collection_metadata_save(database_t* db, const char* collection_name,
                            collection_metadata_t* metadata) {
    if (!db || !collection_name || !metadata) {
        return 0;
    }
    
    /* Build metadata document */
    json_value_t* meta_doc = json_create_object();
    json_object_set(meta_doc, "uuid", json_create_string(COLLECTION_META_ID));
    json_object_set(meta_doc, "_collection", json_create_string(collection_name));
    
    /* Add permissions */
    json_value_t* perms = json_create_object();
    
    /* Convert permissions to string format */
    char perm_str[16];
    snprintf(perm_str, sizeof(perm_str), "%s%s%s%s%s",
             (metadata->permissions.owner_perms & RBAC_READ) ? "r" : "",
             (metadata->permissions.owner_perms & RBAC_WRITE) ? "w" : "",
             (metadata->permissions.owner_perms & RBAC_EXECUTE) ? "x" : "",
             (metadata->permissions.owner_perms & RBAC_DELETE) ? "d" : "",
             (metadata->permissions.owner_perms & RBAC_ADMIN) ? "a" : "");
    json_object_set(perms, "owner_perms", json_create_string(perm_str));
    
    snprintf(perm_str, sizeof(perm_str), "%s%s%s%s%s",
             (metadata->permissions.world_perms & RBAC_READ) ? "r" : "",
             (metadata->permissions.world_perms & RBAC_WRITE) ? "w" : "",
             (metadata->permissions.world_perms & RBAC_EXECUTE) ? "x" : "",
             (metadata->permissions.world_perms & RBAC_DELETE) ? "d" : "",
             (metadata->permissions.world_perms & RBAC_ADMIN) ? "a" : "");
    json_object_set(perms, "world_perms", json_create_string(perm_str));
    
    /* Add owners array */
    if (metadata->permissions.owner_count > 0) {
        json_value_t* owners = json_create_array();
        for (size_t i = 0; i < metadata->permissions.owner_count; i++) {
            if (metadata->permissions.owner_ids[i]) {
                json_array_append(owners, json_create_string(metadata->permissions.owner_ids[i]));
            }
        }
        json_object_set(perms, "owners", owners);
    }
    
    json_object_set(meta_doc, "permissions", perms);
    
    /* Add schema if present */
    if (metadata->schema) {
        json_object_set(meta_doc, "schema", json_deep_copy(metadata->schema));
    }
    
    /* Add versioning config */
    json_value_t* versioning = json_create_object();
    json_object_set(versioning, "enabled", json_create_boolean(metadata->versioning.enabled));
    json_object_set(versioning, "max_versions", json_create_integer(metadata->versioning.max_versions));
    
    if (metadata->versioning.exclude_count > 0) {
        json_value_t* exclude = json_create_array();
        for (size_t i = 0; i < metadata->versioning.exclude_count; i++) {
            json_array_append(exclude, json_create_string(metadata->versioning.exclude_fields[i]));
        }
        json_object_set(versioning, "exclude_fields", exclude);
    }
    json_object_set(meta_doc, "versioning", versioning);
    
    /* Add indexes */
    if (metadata->index_count > 0) {
        json_value_t* indexes = json_create_array();
        for (size_t i = 0; i < metadata->index_count; i++) {
            json_value_t* idx = json_create_object();
            json_object_set(idx, "field", json_create_string(metadata->indexes[i].field_name));
            json_object_set(idx, "unique", json_create_boolean(metadata->indexes[i].unique));
            json_object_set(idx, "type", json_create_string(
                metadata->indexes[i].type == INDEX_META_TYPE_BTREE ? "btree" : "hash"));
            json_object_set(idx, "case_insensitive", 
                           json_create_boolean(metadata->indexes[i].case_insensitive));
            json_array_append(indexes, idx);
        }
        json_object_set(meta_doc, "indexes", indexes);
    }
    
    /* Insert the metadata document */
    json_value_t* result = virtual_insert(db, DOC_TYPE_NAME_CONFIG, "system", VIRTUAL_COLLECTION_CONFIGS, meta_doc, SYSTEM_USER_ADMIN);
    json_free(meta_doc);
    
    if (result) {
        json_free(result);
        return 1;
    }
    
    return 0;
}