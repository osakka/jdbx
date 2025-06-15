#include "database/collection_metadata.h"
#include "database/database.h"
#include "database/document_storage.h"
#include "utils/logger.h"
#include "utils/json.h"

/* Create default metadata for system collections */
static void create_users_metadata(database_t* db) {
    /* Don't create metadata during bootstrap - collections may not exist yet */
    if (db_collection_exists(db, "users")) {
        /* Check if metadata already exists */
        json_value_t* existing = db_get_document(db, STORAGE_LIBRARY, "users", COLLECTION_META_ID);
        if (existing) {
            LOG_DEBUG("Users collection metadata already exists");
            json_free(existing);
            return;
        }
        
        collection_metadata_t* metadata = collection_metadata_create_default("users");
        if (!metadata) return;
        
        /* Add unique constraints for username and email */
        metadata->index_count = 2;
        metadata->indexes = calloc(2, sizeof(index_metadata_t));
        
        /* Username unique index */
        metadata->indexes[0].field_name = strdup("username");
        metadata->indexes[0].type = INDEX_META_TYPE_HASH;
        metadata->indexes[0].unique = true;
        metadata->indexes[0].case_insensitive = false;
        
        /* Email unique index */
        metadata->indexes[1].field_name = strdup("email");
        metadata->indexes[1].type = INDEX_META_TYPE_HASH;
        metadata->indexes[1].unique = true;
        metadata->indexes[1].case_insensitive = true;
        
        /* Create schema */
        const char* schema_json = "{"
            "\"type\": \"object\","
            "\"required\": [\"username\", \"email\", \"password_hash\"],"
            "\"properties\": {"
                "\"username\": {"
                    "\"type\": \"string\","
                    "\"minLength\": 3,"
                    "\"maxLength\": 32,"
                    "\"pattern\": \"^[a-zA-Z0-9_]+$\""
                "},"
                "\"email\": {"
                    "\"type\": \"string\","
                    "\"format\": \"email\""
                "},"
                "\"password_hash\": {"
                    "\"type\": \"string\","
                    "\"minLength\": 60"
                "},"
                "\"roles\": {"
                    "\"type\": \"array\","
                    "\"items\": {\"type\": \"string\"}"
                "},"
                "\"active\": {"
                    "\"type\": \"boolean\","
                    "\"default\": true"
                "}"
            "}"
        "}";
        
        metadata->schema = json_parse(schema_json);
        
        /* Save metadata */
        if (collection_metadata_save(db, "users", metadata)) {
            LOG_INFO("Created users collection metadata with unique constraints");
        } else {
            LOG_ERROR("Failed to save users collection metadata");
        }
        collection_metadata_free(metadata);
    }
}

/* Create default metadata for roles collection */
static void create_roles_metadata(database_t* db) {
    /* Don't create metadata during bootstrap - collections may not exist yet */
    if (db_collection_exists(db, "roles")) {
        /* Check if metadata already exists */
        json_value_t* existing = db_get_document(db, STORAGE_LIBRARY, "roles", COLLECTION_META_ID);
        if (existing) {
            LOG_DEBUG("Roles collection metadata already exists");
            json_free(existing);
            return;
        }
        
        collection_metadata_t* metadata = collection_metadata_create_default("roles");
        if (!metadata) return;
        
        /* Add unique constraint for role name */
        metadata->index_count = 1;
        metadata->indexes = calloc(1, sizeof(index_metadata_t));
        
        metadata->indexes[0].field_name = strdup("name");
        metadata->indexes[0].type = INDEX_META_TYPE_HASH;
        metadata->indexes[0].unique = true;
        metadata->indexes[0].case_insensitive = false;
        
        /* Create schema */
        const char* schema_json = "{"
            "\"type\": \"object\","
            "\"required\": [\"name\"],"
            "\"properties\": {"
                "\"name\": {"
                    "\"type\": \"string\","
                    "\"minLength\": 1,"
                    "\"maxLength\": 64"
                "},"
                "\"parent_roles\": {"
                    "\"type\": \"array\","
                    "\"items\": {\"type\": \"string\"}"
                "},"
                "\"child_roles\": {"
                    "\"type\": \"array\","
                    "\"items\": {\"type\": \"string\"}"
                "},"
                "\"permissions\": {"
                    "\"type\": \"object\""
                "}"
            "}"
        "}";
        
        metadata->schema = json_parse(schema_json);
        
        /* Save metadata */
        if (collection_metadata_save(db, "roles", metadata)) {
            LOG_INFO("Created roles collection metadata with unique constraints");
        } else {
            LOG_ERROR("Failed to save roles collection metadata");
        }
        collection_metadata_free(metadata);
    }
}

/* Initialize system collection metadata */
void collection_defaults_init(database_t* db) {
    if (!db) return;
    
    /* Create metadata for system collections */
    create_users_metadata(db);
    create_roles_metadata(db);
    
    /* Add more system collections as needed */
}