#include "utils/buffer_pool.h"
#include "database/library_metadata.h"
#include "database/database.h"
#include "database/document_storage.h"
#include "utils/logger.h"
#include "utils/json_helpers.h"
#include <string.h>
#include <stdlib.h>

/* System library initialization */
int library_system_init(database_t* db) {
    if (!db) return -1;
    
    LOG_INFO("Initializing library system");
    
    /* Create libraries collection if it doesn't exist */
    if (!db_collection_exists(db, "libraries")) {
        if (db_create_collection(db, "libraries") != 0) {
            LOG_ERROR("Failed to create libraries collection");
            return -1;
        }
    }
    
    /* Create system library */
    library_metadata_t* system_lib = library_metadata_create_system();
    if (!system_lib) {
        LOG_ERROR("Failed to create system library metadata");
        return -1;
    }
    
    /* Check if system library already exists */
    json_value_t* existing = db_get_document(db, STORAGE_LIBRARY, STORAGE_COLLECTION, "system");
    if (!existing) {
        /* Save system library */
        if (library_metadata_save(db, "system", system_lib) != 1) {
            LOG_ERROR("Failed to save system library");
            library_metadata_free(system_lib);
            return -1;
        }
        LOG_INFO("Created system library");
    } else {
        json_free(existing);
        LOG_DEBUG("System library already exists");
    }
    library_metadata_free(system_lib);
    
    /* Create default library */
    library_metadata_t* default_lib = library_metadata_create_default();
    if (!default_lib) {
        LOG_ERROR("Failed to create default library metadata");
        return -1;
    }
    
    /* Check if default library already exists */
    existing = db_get_document(db, STORAGE_LIBRARY, STORAGE_COLLECTION, "default");
    if (!existing) {
        /* Save default library */
        if (library_metadata_save(db, "default", default_lib) != 1) {
            LOG_ERROR("Failed to save default library");
            library_metadata_free(default_lib);
            return -1;
        }
        LOG_INFO("Created default library");
    } else {
        json_free(existing);
        LOG_DEBUG("Default library already exists");
    }
    library_metadata_free(default_lib);
    
    /* Migrate system collections */
    const char* system_collections[] = {
        "users",       /* from _users */
        "roles",       /* from _roles */
        "memberships", /* new - for user-role relationships */
        "permissions", /* from _permissions */
        "collections", /* from _collections */
        "sessions",    /* from _sessions */
        "config",      /* from _system_config */
        "metrics",     /* from _metrics */
        "validators",  /* from _validators */
        "transformers",/* from _transformers */
        "functions",   /* from _functions */
        "permission_cache" /* from _permission_cache */
    };
    
    /* Create system collections */
    for (size_t i = 0; i < sizeof(system_collections) / sizeof(system_collections[0]); i++) {
        char full_name[256];
        snprintf(full_name, sizeof(full_name), "system/%s", system_collections[i]);
        
        if (!db_collection_exists(db, full_name)) {
            if (db_create_collection(db, full_name) != 0) {
                LOG_ERROR("Failed to create system collection: %s", full_name);
                return -1;
            }
            LOG_INFO("Created system collection: %s", full_name);
        }
    }
    
    return 0;
}

/* Create system library metadata */
library_metadata_t* library_metadata_create_system(void) {
    library_metadata_t* metadata = BUFFER_ALLOC(sizeof(library_metadata_t));
    if (!metadata) return NULL;
    
    metadata->library_name = BUFFER_STRDUP("system");
    metadata->type = LIBRARY_TYPE_SYSTEM;
    metadata->description = BUFFER_STRDUP("Core system collections and functionality");
    
    /* System library permissions - only admin can modify */
    metadata->permissions.owner_perms = RBAC_READ | RBAC_WRITE | RBAC_DELETE | RBAC_EXECUTE | RBAC_ADMIN;
    metadata->permissions.world_perms = RBAC_READ; /* World can read system config */
    metadata->permissions.owner_ids = BUFFER_ALLOC(sizeof(char*));
    metadata->permissions.owner_ids[0] = BUFFER_STRDUP("system");
    metadata->permissions.owner_count = 1;
    
    /* System library configuration */
    metadata->config.allow_collection_creation = false; /* System collections are fixed */
    metadata->config.allow_collection_deletion = false;
    metadata->config.default_collection_template = NULL;
    
    /* System library collections */
    const char* collections[] = {
        "users", "roles", "memberships", "permissions", 
        "collections", "sessions", "config", "metrics",
        "validators", "transformers", "functions", "permission_cache"
    };
    
    metadata->collection_count = sizeof(collections) / sizeof(collections[0]);
    metadata->collections = BUFFER_ALLOC(sizeof(char*));
    for (size_t i = 0; i < metadata->collection_count; i++) {
        metadata->collections[i] = BUFFER_STRDUP(collections[i]);
    }
    
    /* No cross-library restrictions for system */
    metadata->access.allowed_libraries = NULL;
    metadata->access.allowed_count = 0;
    metadata->access.denied_libraries = NULL;
    metadata->access.denied_count = 0;
    
    return metadata;
}

/* Create default library metadata */
library_metadata_t* library_metadata_create_default(void) {
    library_metadata_t* metadata = BUFFER_ALLOC(sizeof(library_metadata_t));
    if (!metadata) return NULL;
    
    metadata->library_name = BUFFER_STRDUP("default");
    metadata->type = LIBRARY_TYPE_DEFAULT;
    metadata->description = BUFFER_STRDUP("Default library for user collections");
    
    /* Default library permissions - users can create collections */
    metadata->permissions.owner_perms = RBAC_READ | RBAC_WRITE | RBAC_DELETE | RBAC_EXECUTE | RBAC_ADMIN;
    metadata->permissions.world_perms = RBAC_READ | RBAC_WRITE; /* Users can create collections */
    metadata->permissions.owner_ids = NULL;
    metadata->permissions.owner_count = 0;
    
    /* Default library configuration */
    metadata->config.allow_collection_creation = true;
    metadata->config.allow_collection_deletion = true;
    metadata->config.default_collection_template = BUFFER_STRDUP("basic_collection");
    
    /* Start with no collections */
    metadata->collection_count = 0;
    metadata->collections = NULL;
    
    /* Allow access from all libraries by default */
    metadata->access.allowed_libraries = NULL;
    metadata->access.allowed_count = 0;
    metadata->access.denied_libraries = NULL;
    metadata->access.denied_count = 0;
    
    return metadata;
}

/* Load library metadata */
library_metadata_t* library_metadata_load(database_t* db, const char* library_name) {
    if (!db || !library_name) return NULL;
    
    /* Load library document */
    json_value_t* lib_doc = db_get_document(db, STORAGE_LIBRARY, STORAGE_COLLECTION, library_name);
    if (!lib_doc) {
        LOG_DEBUG("Library %s not found", library_name);
        return NULL;
    }
    
    library_metadata_t* metadata = BUFFER_ALLOC(sizeof(library_metadata_t));
    if (!metadata) {
        json_free(lib_doc);
        return NULL;
    }
    
    metadata->library_name = BUFFER_STRDUP(library_name);
    
    /* Parse type */
    json_value_t* type_val = json_object_get(lib_doc, "type");
    if (type_val && type_val->type == JSON_STRING) {
        if (strcmp(type_val->value.string, "system") == 0) {
            metadata->type = LIBRARY_TYPE_SYSTEM;
        } else if (strcmp(type_val->value.string, "default") == 0) {
            metadata->type = LIBRARY_TYPE_DEFAULT;
        } else {
            metadata->type = LIBRARY_TYPE_USER;
        }
    }
    
    /* Parse description */
    json_value_t* desc_val = json_object_get(lib_doc, "description");
    if (desc_val && desc_val->type == JSON_STRING) {
        metadata->description = BUFFER_STRDUP(desc_val->value.string);
    }
    
    /* Parse collections */
    json_value_t* colls = json_object_get(lib_doc, "collections");
    if (colls && colls->type == JSON_ARRAY) {
        metadata->collection_count = colls->value.array.size;
        metadata->collections = BUFFER_ALLOC(sizeof(char*));
        for (size_t i = 0; i < metadata->collection_count; i++) {
            json_value_t* coll = json_array_get(colls, i);
            if (coll && coll->type == JSON_STRING) {
                metadata->collections[i] = BUFFER_STRDUP(coll->value.string);
            }
        }
    }
    
    /* TODO: Parse permissions, config, and access rules */
    
    json_free(lib_doc);
    return metadata;
}

/* Save library metadata */
int library_metadata_save(database_t* db, const char* library_name, library_metadata_t* metadata) {
    if (!db || !library_name || !metadata) return 0;
    
    /* Build library document */
    json_value_t* lib_doc = json_create_object();
    json_object_set(lib_doc, "uuid", json_create_string(library_name));
    json_object_set(lib_doc, "name", json_create_string(metadata->library_name));
    
    /* Add type */
    const char* type_str = metadata->type == LIBRARY_TYPE_SYSTEM ? "system" :
                          metadata->type == LIBRARY_TYPE_DEFAULT ? "default" : "user";
    json_object_set(lib_doc, "type", json_create_string(type_str));
    
    /* Add description */
    if (metadata->description) {
        json_object_set(lib_doc, "description", json_create_string(metadata->description));
    }
    
    /* Add collections array */
    json_value_t* colls = json_create_array();
    for (size_t i = 0; i < metadata->collection_count; i++) {
        if (metadata->collections[i]) {
            json_array_append(colls, json_create_string(metadata->collections[i]));
        }
    }
    json_object_set(lib_doc, "collections", colls);
    
    /* Add configuration */
    json_value_t* config = json_create_object();
    json_object_set(config, "allow_collection_creation", 
                   json_create_boolean(metadata->config.allow_collection_creation));
    json_object_set(config, "allow_collection_deletion",
                   json_create_boolean(metadata->config.allow_collection_deletion));
    if (metadata->config.default_collection_template) {
        json_object_set(config, "default_collection_template",
                       json_create_string(metadata->config.default_collection_template));
    }
    json_object_set(lib_doc, "config", config);
    
    /* TODO: Add permissions and access rules */
    
    /* Insert or update the library document */
    json_value_t* existing = db_get_document(db, STORAGE_LIBRARY, STORAGE_COLLECTION, library_name);
    json_value_t* result;
    
    if (existing) {
        json_free(existing);
        result = storage_update_document(db, library_name, lib_doc);
    } else {
        result = storage_insert_document(db, lib_doc);
    }
    
    json_free(lib_doc);
    
    if (result) {
        json_free(result);
        return 1;
    }
    
    return 0;
}

/* Free library metadata */
void library_metadata_free(library_metadata_t* metadata) {
    if (!metadata) return;
    
    BUFFER_FREE(metadata->library_name);
    BUFFER_FREE(metadata->description);
    
    /* Free collections */
    for (size_t i = 0; i < metadata->collection_count; i++) {
        BUFFER_FREE(metadata->collections[i]);
    }
    BUFFER_FREE(metadata->collections);
    
    /* Free permissions */
    for (size_t i = 0; i < metadata->permissions.owner_count; i++) {
        BUFFER_FREE(metadata->permissions.owner_ids[i]);
    }
    BUFFER_FREE(metadata->permissions.owner_ids);
    
    /* Free config */
    BUFFER_FREE(metadata->config.default_collection_template);
    
    /* Free access rules */
    for (size_t i = 0; i < metadata->access.allowed_count; i++) {
        BUFFER_FREE(metadata->access.allowed_libraries[i]);
    }
    BUFFER_FREE(metadata->access.allowed_libraries);
    
    for (size_t i = 0; i < metadata->access.denied_count; i++) {
        BUFFER_FREE(metadata->access.denied_libraries[i]);
    }
    BUFFER_FREE(metadata->access.denied_libraries);
    
    BUFFER_FREE(metadata);
}

/* Add collection to library */
int library_add_collection(library_metadata_t* metadata, const char* collection_name) {
    if (!metadata || !collection_name) return -1;
    
    /* Check if already exists */
    for (size_t i = 0; i < metadata->collection_count; i++) {
        if (metadata->collections[i] && 
            strcmp(metadata->collections[i], collection_name) == 0) {
            return 0; /* Already exists */
        }
    }
    
    /* Add to array */
    char** new_collections = BUFFER_REALLOC(metadata->collections, 
                                   (metadata->collection_count + 1) * sizeof(char*));
    if (!new_collections) return -1;
    
    metadata->collections = new_collections;
    metadata->collections[metadata->collection_count] = BUFFER_STRDUP(collection_name);
    metadata->collection_count++;
    
    return 0;
}

/* Remove collection from library */
int library_remove_collection(library_metadata_t* metadata, const char* collection_name) {
    if (!metadata || !collection_name) return -1;
    
    /* Find and remove */
    for (size_t i = 0; i < metadata->collection_count; i++) {
        if (metadata->collections[i] && 
            strcmp(metadata->collections[i], collection_name) == 0) {
            BUFFER_FREE(metadata->collections[i]);
            
            /* Shift remaining */
            for (size_t j = i; j < metadata->collection_count - 1; j++) {
                metadata->collections[j] = metadata->collections[j + 1];
            }
            
            metadata->collection_count--;
            return 0;
        }
    }
    
    return -1; /* Not found */
}

/* Check if library has collection */
bool library_has_collection(library_metadata_t* metadata, const char* collection_name) {
    if (!metadata || !collection_name) return false;
    
    for (size_t i = 0; i < metadata->collection_count; i++) {
        if (metadata->collections[i] && 
            strcmp(metadata->collections[i], collection_name) == 0) {
            return true;
        }
    }
    
    return false;
}