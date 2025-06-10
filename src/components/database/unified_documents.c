#include "database/unified_documents.h"
#include "database/database.h"
#include "rbac/rbac.h"
#include "utils/logger.h"
#include "utils/json_helpers.h"
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

/* Document type string mappings */
static const struct {
    document_type_t type;
    const char* name;
} type_mappings[] = {
    { DOC_TYPE_USER, "user" },
    { DOC_TYPE_ROLE, "role" },
    { DOC_TYPE_MEMBERSHIP, "membership" },
    { DOC_TYPE_PERMISSION, "permission" },
    { DOC_TYPE_SESSION, "session" },
    { DOC_TYPE_LIBRARY, "library" },
    { DOC_TYPE_COLLECTION, "collection" },
    { DOC_TYPE_FUNCTION, "function" },
    { DOC_TYPE_VALIDATOR, "validator" },
    { DOC_TYPE_TRANSFORMER, "transformer" },
    { DOC_TYPE_METRIC, "metric" },
    { DOC_TYPE_CONFIG, "config" },
    { DOC_TYPE_INDEX, "index" },
    { DOC_TYPE_SCHEMA, "schema" },
    { DOC_TYPE_VERSION, "version" },
    { DOC_TYPE_AUDIT, "audit" },
    { DOC_TYPE_CACHE_ENTRY, "cache_entry" }
};

const char* document_type_to_string(document_type_t type) {
    for (size_t i = 0; i < sizeof(type_mappings) / sizeof(type_mappings[0]); i++) {
        if (type_mappings[i].type == type) {
            return type_mappings[i].name;
        }
    }
    return "unknown";
}

document_type_t document_type_from_string(const char* type_str) {
    if (!type_str) return DOC_TYPE_UNKNOWN;
    
    for (size_t i = 0; i < sizeof(type_mappings) / sizeof(type_mappings[0]); i++) {
        if (strcmp(type_mappings[i].name, type_str) == 0) {
            return type_mappings[i].type;
        }
    }
    return DOC_TYPE_UNKNOWN;
}

/* Create system actor user */
static int create_system_actor(database_t* db, const char* username, const char* description,
                              const char* permissions_json) {
    LOG_INFO("Creating system actor: %s", username);
    
    /* Check if already exists */
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string("user"));
    json_object_set(query, "name", json_create_string(username));
    
    json_value_t* results = db_query_documents(db, DOCUMENTS_COLLECTION, query);
    json_free(query);
    
    if (results) {
        json_value_t* docs = json_object_get(results, "documents");
        if (docs && docs->type == JSON_ARRAY && json_array_size(docs) > 0) {
            LOG_DEBUG("System actor %s already exists", username);
            json_free(results);
            return 1;
        }
        json_free(results);
    }
    
    /* Create system actor document */
    json_value_t* actor = json_create_object();
    
    /* Standard document fields */
    json_object_set(actor, "type", json_create_string("user"));
    json_object_set(actor, "name", json_create_string(username));
    json_object_set(actor, "library", json_create_string("system"));
    json_object_set(actor, "collection", json_create_string("users"));
    
    /* User-specific fields */
    json_object_set(actor, "username", json_create_string(username));
    json_object_set(actor, "email", json_create_string(username));
    json_object_set(actor, "full_name", json_create_string(description));
    json_object_set(actor, "is_system", json_create_boolean(1));
    json_object_set(actor, "active", json_create_boolean(1));
    
    /* No password for system actors - they can't login */
    json_object_set(actor, "password_hash", json_create_null());
    
    /* Timestamps */
    char timestamp[64];
    time_t now = time(NULL);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    json_object_set(actor, "created_at", json_create_string(timestamp));
    json_object_set(actor, "updated_at", json_create_string(timestamp));
    
    /* Owner is self */
    json_object_set(actor, "owner", json_create_string(username));
    
    /* Permissions */
    if (permissions_json) {
        json_value_t* perms = json_parse(permissions_json);
        if (perms) {
            json_object_set(actor, "permissions", perms);
        }
    }
    
    /* Insert */
    json_value_t* result = db_insert_document(db, DOCUMENTS_COLLECTION, actor);
    json_free(actor);
    
    if (result) {
        LOG_INFO("Created system actor: %s", username);
        json_free(result);
        return 1;
    }
    
    LOG_ERROR("Failed to create system actor: %s", username);
    return 0;
}

/* Create collection metadata helper */
static int create_collection_metadata(database_t* db, const char* library, const char* collection_name,
                                     const char* owner, int is_system) {
    /* Check if already exists */
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string("collection"));
    json_object_set(query, "name", json_create_string(collection_name));
    json_object_set(query, "library", json_create_string(library));
    
    json_value_t* existing = db_query_documents(db, DOCUMENTS_COLLECTION, query);
    json_free(query);
    
    if (existing && json_object_get(existing, "documents") &&
        json_object_get(existing, "documents")->value.array.size > 0) {
        json_free(existing);
        return 1; /* Already exists */
    }
    if (existing) json_free(existing);
    
    /* Create collection metadata */
    json_value_t* coll = json_create_object();
    add_document_system_fields(coll, "collection", library, "collections", owner);
    json_object_set(coll, "name", json_create_string(collection_name));
    json_object_set(coll, "library", json_create_string(library));
    json_object_set(coll, "is_system", json_create_boolean(is_system));
    
    /* Add versioning policy */
    json_value_t* versioning = json_create_object();
    json_object_set(versioning, "enabled", json_create_boolean(1));
    json_object_set(versioning, "max_versions", json_create_number(10));
    json_object_set(coll, "versioning", versioning);
    
    /* Add collection-specific settings */
    json_value_t* settings = json_create_object();
    if (strcmp(collection_name, "users") == 0) {
        json_object_set(settings, "unique_fields", json_create_array());
        json_array_append(json_object_get(settings, "unique_fields"), json_create_string("username"));
    }
    json_object_set(coll, "settings", settings);
    
    json_value_t* result = db_insert_document(db, DOCUMENTS_COLLECTION, coll);
    json_free(coll);
    
    if (result) {
        LOG_DEBUG("Created collection metadata: %s.%s", library, collection_name);
        json_free(result);
        
        /* Also create the actual collection in the library */
        char collection_path[256];
        snprintf(collection_path, sizeof(collection_path), "%s/%s", library, collection_name);
        
        if (!db_collection_exists(db, collection_path)) {
            if (db_create_collection(db, collection_path) != 0) {
                LOG_ERROR("Failed to create physical collection: %s", collection_path);
                return 0;
            }
            LOG_DEBUG("Created physical collection: %s", collection_path);
        }
        
        return 1;
    }
    
    return 0;
}

/* Create library metadata document */
static int create_library_metadata(database_t* db, const char* name, const char* display_name,
                                  const char* description, const char* owner, int is_system) {
    /* Check if already exists */
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string("library"));
    json_object_set(query, "name", json_create_string(name));
    
    json_value_t* existing = db_query_documents(db, DOCUMENTS_COLLECTION, query);
    json_free(query);
    
    if (existing && json_object_get(existing, "documents") &&
        json_object_get(existing, "documents")->value.array.size > 0) {
        json_free(existing);
        return 1; /* Already exists */
    }
    if (existing) json_free(existing);
    
    /* Create library metadata */
    json_value_t* lib = json_create_object();
    json_object_set(lib, "type", json_create_string("library"));
    json_object_set(lib, "name", json_create_string(name));
    json_object_set(lib, "display_name", json_create_string(display_name));
    json_object_set(lib, "description", json_create_string(description));
    json_object_set(lib, "owner", json_create_string(owner));
    json_object_set(lib, "is_system", json_create_boolean(is_system));
    
    /* Library settings */
    json_value_t* settings = json_create_object();
    json_object_set(settings, "default_versioning", json_create_boolean(1));
    json_object_set(settings, "max_collections", json_create_number(is_system ? -1 : 100));
    json_object_set(settings, "max_storage", json_create_string(is_system ? "unlimited" : "10GB"));
    json_object_set(lib, "settings", settings);
    
    /* Add timestamps */
    add_document_system_fields(lib, "library", "system", "libraries", owner);
    
    json_value_t* result = db_insert_document(db, DOCUMENTS_COLLECTION, lib);
    json_free(lib);
    
    if (result) {
        LOG_INFO("Created library metadata: %s", name);
        json_free(result);
        
        /* The database layer will handle creating directories when collections are created */
        LOG_DEBUG("Library metadata created for: %s", name);
        
        return 1;
    }
    
    return 0;
}

/* Create all system actors */
int create_system_actors(database_t* db) {
    LOG_INFO("Creating system actors");
    
    /* System admin - owns system library and collections */
    create_system_actor(db, SYSTEM_USER_ADMIN, "System Administrator",
        "{\"*\": [\"read\", \"write\", \"delete\", \"execute\", \"admin\"]}");
    
    /* Metrics collector - can write to system/metrics collection */
    create_system_actor(db, SYSTEM_USER_METRICS, "Metrics Collector",
        "{\"system/metrics\": [\"write\"], \"documents\": [\"read\"]}");
    
    /* Indexer - can manage indexes and read all collections */
    create_system_actor(db, SYSTEM_USER_INDEXER, "Index Manager",
        "{\"*/*\": [\"read\"], \"documents\": [\"read\", \"write\"]}");
    
    /* Persistence - can read all collections for backup */
    create_system_actor(db, SYSTEM_USER_PERSISTENCE, "Persistence Manager",
        "{\"*/*\": [\"read\"], \"documents\": [\"read\"], \"binary\": [\"write\"]}");
    
    /* Cache manager - manages cache metadata */
    create_system_actor(db, SYSTEM_USER_CACHE, "Cache Manager",
        "{\"documents\": [\"read\", \"write\"]}");
    
    /* Audit logger - writes to system/audit */
    create_system_actor(db, SYSTEM_USER_AUDIT, "Audit Logger",
        "{\"system/audit\": [\"write\"], \"documents\": [\"read\"]}");
    
    return 1;
}

/* Initialize unified documents system */
int unified_documents_init(database_t* db) {
    if (!db) return 0;
    
    LOG_INFO("Initializing unified documents system");
    
    /* Create documents collection at root level */
    if (!db_collection_exists(db, DOCUMENTS_COLLECTION)) {
        /* Use simple collection name to ensure it's at root level */
        if (db_create_collection(db, DOCUMENTS_COLLECTION) != 0) {
            LOG_ERROR("Failed to create documents collection");
            return 0;
        }
        LOG_INFO("Created documents collection at root level");
    }
    
    /* Create indexes */
    LOG_INFO("Creating unified document indexes");
    
    /* Type index for fast filtering */
    db_create_index(db, DOCUMENTS_COLLECTION, "idx_type", "type", INDEX_TYPE_NON_UNIQUE);
    
    /* Compound indexes for common queries */
    db_create_index(db, DOCUMENTS_COLLECTION, "idx_type_name", "type,name", INDEX_TYPE_UNIQUE);
    db_create_index(db, DOCUMENTS_COLLECTION, "idx_type_lib_coll", "type,library,collection", INDEX_TYPE_NON_UNIQUE);
    
    /* Library and collection indexes */
    db_create_index(db, DOCUMENTS_COLLECTION, "idx_library", "library", INDEX_TYPE_NON_UNIQUE);
    db_create_index(db, DOCUMENTS_COLLECTION, "idx_collection", "collection", INDEX_TYPE_NON_UNIQUE);
    
    /* Owner index for ownership queries */
    db_create_index(db, DOCUMENTS_COLLECTION, "idx_owner", "owner", INDEX_TYPE_NON_UNIQUE);
    
    /* Username index for users */
    db_create_index(db, DOCUMENTS_COLLECTION, "idx_username", "username", INDEX_TYPE_UNIQUE);
    
    /* Create system actors */
    create_system_actors(db);
    
    /* Create default libraries */
    LOG_INFO("Creating default libraries");
    
    /* System library */
    create_library_metadata(db, "system", "System Library", 
                           "Core system collections", SYSTEM_USER_ADMIN, 1);
    
    /* Default library */
    create_library_metadata(db, "default", "Default Library",
                           "User collections", SYSTEM_USER_ADMIN, 0);
    
    /* Define default collections for any library */
    const char* default_collections[] = {
        "users", "roles", "permissions", "sessions", "metrics", "audit"
    };
    
    /* Additional system-only collections */
    const char* system_only_collections[] = {
        "actors", "libraries", "memberships", "configs", "indexes", 
        "schemas", "functions", "validators", "transformers", "versions"
    };
    
    /* Create default collections for system library */
    for (size_t i = 0; i < sizeof(default_collections) / sizeof(default_collections[0]); i++) {
        if (!create_collection_metadata(db, "system", default_collections[i], SYSTEM_USER_ADMIN, 1)) {
            LOG_ERROR("Failed to create system collection: %s", default_collections[i]);
        }
    }
    
    /* Create system-only collections */
    for (size_t i = 0; i < sizeof(system_only_collections) / sizeof(system_only_collections[0]); i++) {
        if (!create_collection_metadata(db, "system", system_only_collections[i], SYSTEM_USER_ADMIN, 1)) {
            LOG_ERROR("Failed to create system collection: %s", system_only_collections[i]);
        }
    }
    
    /* Create default collections for default library */
    for (size_t i = 0; i < sizeof(default_collections) / sizeof(default_collections[0]); i++) {
        if (!create_collection_metadata(db, "default", default_collections[i], SYSTEM_USER_ADMIN, 0)) {
            LOG_ERROR("Failed to create default collection: %s", default_collections[i]);
        }
    }
    
    /* Create admin role */
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string("role"));
    json_object_set(query, "name", json_create_string("admin"));
    json_value_t* existing = db_query_documents(db, DOCUMENTS_COLLECTION, query);
    json_free(query);
    
    char* admin_role_id = NULL;
    if (!existing || !json_object_get(existing, "documents") ||
        json_object_get(existing, "documents")->value.array.size == 0) {
        /* Create admin role */
        json_value_t* admin_role = json_create_object();
        add_document_system_fields(admin_role, "role", "system", "roles", SYSTEM_USER_ADMIN);
        json_object_set(admin_role, "name", json_create_string("admin"));
        json_object_set(admin_role, "display_name", json_create_string("Administrator"));
        json_object_set(admin_role, "description", json_create_string("Full system access"));
        
        /* Admin has all permissions on all resources */
        json_value_t* permissions = json_create_object();
        json_value_t* all_perms = json_create_array();
        json_array_append(all_perms, json_create_string("read"));
        json_array_append(all_perms, json_create_string("write"));
        json_array_append(all_perms, json_create_string("delete"));
        json_array_append(all_perms, json_create_string("execute"));
        json_array_append(all_perms, json_create_string("admin"));
        json_object_set(permissions, "*/*", all_perms);
        json_object_set(admin_role, "permissions", permissions);
        
        json_value_t* result = db_insert_document(db, DOCUMENTS_COLLECTION, admin_role);
        if (result) {
            json_value_t* id_val = json_object_get(result, "uuid");
            if (id_val && id_val->type == JSON_STRING) {
                admin_role_id = strdup(id_val->value.string);
            }
            json_free(result);
        }
        json_free(admin_role);
        LOG_INFO("Created admin role");
    }
    if (existing) json_free(existing);
    
    /* Create admin user in system/users collection */
    query = json_create_object();
    json_object_set(query, "username", json_create_string("admin"));
    existing = db_query_documents(db, "system/users", query);
    json_free(query);
    
    if (!existing || !json_object_get(existing, "documents") ||
        json_object_get(existing, "documents")->value.array.size == 0) {
        /* Create admin user */
        json_value_t* admin_user = json_create_object();
        json_object_set(admin_user, "username", json_create_string("admin"));
        json_object_set(admin_user, "email", json_create_string("admin@localhost"));
        json_object_set(admin_user, "full_name", json_create_string("System Administrator"));
        
        /* Hash password "admin" for compatibility with authentication_handler.c */
        extern char* hash_password(const char* password);
        char* password_hash = hash_password("admin");
        json_object_set(admin_user, "password_hash", json_create_string(password_hash));
        free(password_hash);
        
        json_object_set(admin_user, "status", json_create_string("active"));
        json_object_set(admin_user, "is_admin", json_create_boolean(1));
        json_object_set(admin_user, "active", json_create_boolean(1));
        json_object_set(admin_user, "roles", json_create_array());
        
        /* Add admin role */
        if (admin_role_id) {
            json_array_append(json_object_get(admin_user, "roles"), json_create_string(admin_role_id));
        }
        
        json_value_t* result = db_insert_document(db, "system/users", admin_user);
        if (result) {
            LOG_INFO("Created admin user with password 'admin' in system/users");
            json_free(result);
        }
        json_free(admin_user);
    }
    if (existing) json_free(existing);
    if (admin_role_id) free(admin_role_id);
    
    LOG_INFO("Unified documents system initialized");
    return 1;
}

/* Query documents by type */
json_value_t* query_documents_by_type(database_t* db, const char* type,
                                     json_value_t* additional_query) {
    if (!db || !type) return NULL;
    
    json_value_t* query = additional_query ? json_clone(additional_query) : json_create_object();
    json_object_set(query, "type", json_create_string(type));
    
    json_value_t* results = db_query_documents(db, DOCUMENTS_COLLECTION, query);
    json_free(query);
    
    return results;
}

/* Query documents by type and library/collection */
json_value_t* query_documents_by_location(database_t* db, const char* type,
                                         const char* library, const char* collection,
                                         json_value_t* additional_query) {
    if (!db || !type) return NULL;
    
    json_value_t* query = additional_query ? json_clone(additional_query) : json_create_object();
    json_object_set(query, "type", json_create_string(type));
    
    if (library) {
        json_object_set(query, "library", json_create_string(library));
    }
    
    if (collection) {
        json_object_set(query, "collection", json_create_string(collection));
    }
    
    json_value_t* results = db_query_documents(db, DOCUMENTS_COLLECTION, query);
    json_free(query);
    
    return results;
}

/* Add system fields to document */
void add_document_system_fields(json_value_t* doc, const char* type,
                               const char* library, const char* collection,
                               const char* owner_id) {
    if (!doc) return;
    
    /* Type is mandatory */
    if (type) {
        json_object_set(doc, "type", json_create_string(type));
    }
    
    /* Library and collection for organization */
    if (library) {
        json_object_set(doc, "library", json_create_string(library));
    }
    
    if (collection) {
        json_object_set(doc, "collection", json_create_string(collection));
    }
    
    /* Owner */
    if (owner_id) {
        json_object_set(doc, "owner", json_create_string(owner_id));
    }
    
    /* Timestamps */
    char timestamp[64];
    time_t now = time(NULL);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    
    if (!json_object_get(doc, "created_at")) {
        json_object_set(doc, "created_at", json_create_string(timestamp));
    }
    json_object_set(doc, "updated_at", json_create_string(timestamp));
    
    /* Version if not set */
    if (!json_object_get(doc, "version")) {
        json_object_set(doc, "version", json_create_number(1));
    }
}

/* Create document with proper type and ownership */
json_value_t* create_typed_document(database_t* db, const char* type,
                                   const char* name, const char* owner_id,
                                   json_value_t* content) {
    if (!db || !type || !name || !owner_id) return NULL;
    
    json_value_t* doc = content ? json_clone(content) : json_create_object();
    
    /* Set name */
    json_object_set(doc, "name", json_create_string(name));
    
    /* Add system fields */
    add_document_system_fields(doc, type, NULL, NULL, owner_id);
    
    /* Insert document */
    return db_insert_document(db, DOCUMENTS_COLLECTION, doc);
}

/* Validate document has required fields */
int validate_document_structure(json_value_t* doc) {
    if (!doc || doc->type != JSON_OBJECT) return 0;
    
    /* Required fields */
    json_value_t* type = json_object_get(doc, "type");
    if (!type || type->type != JSON_STRING) {
        LOG_ERROR("Document missing required 'type' field");
        return 0;
    }
    
    json_value_t* owner = json_object_get(doc, "owner");
    if (!owner || owner->type != JSON_STRING) {
        LOG_ERROR("Document missing required 'owner' field");
        return 0;
    }
    
    return 1;
}