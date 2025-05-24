#include "database/database.h"
#include "utils/logger.h"
#include "utils/json.h"
#include <string.h>

/* Check if a collection exists */
int db_collection_exists(database_t* db, const char* collection_name) {
    LOG_TRACE("DB: Checking if collection exists: %s", collection_name);
    
    if (!db || !collection_name) {
        LOG_ERROR("DB: Invalid parameters for collection exists check");
        return 0;
    }
    
    /* Lock database for reading */
    pthread_mutex_lock(&db->lock);
    
    /* Check if collection exists in the database */
    int exists = 0;
    for (size_t i = 0; i < db->collections->value.object.size; i++) {
        const char* name = db->collections->value.object.entries[i].key;
        if (strcmp(name, collection_name) == 0) {
            exists = 1;
            break;
        }
    }
    
    pthread_mutex_unlock(&db->lock);
    
    LOG_TRACE("DB: Collection %s %s", collection_name, exists ? "exists" : "does not exist");
    return exists;
}

/* Create a new collection */
int db_create_collection(database_t* db, const char* collection_name) {
    LOG_TRACE("DB: Creating collection: %s", collection_name);
    
    if (!db || !collection_name) {
        LOG_ERROR("DB: Invalid parameters for collection creation");
        return 0;
    }
    
    /* Lock database for writing */
    pthread_mutex_lock(&db->lock);
    
    /* Check if collection already exists */
    int exists = 0;
    for (size_t i = 0; i < db->collections->value.object.size; i++) {
        const char* name = db->collections->value.object.entries[i].key;
        if (strcmp(name, collection_name) == 0) {
            exists = 1;
            break;
        }
    }
    
    if (exists) {
        pthread_mutex_unlock(&db->lock);
        LOG_INFO("DB: Collection already exists: %s", collection_name);
        return 1; /* Success - collection already exists */
    }
    
    /* Create new collection as empty JSON array */
    json_value_t* collection = json_create_array();
    if (!collection) {
        pthread_mutex_unlock(&db->lock);
        LOG_ERROR("DB: Failed to create collection array");
        return 0;
    }
    
    /* Add collection to database */
    json_object_set(db->collections, collection_name, collection);
    
    /* Update modification tracking */
    db->is_modified = 1;
    
    pthread_mutex_unlock(&db->lock);
    
    /* Notify persistence thread of data change */
    db_notify_data_change_sync(db, strlen(collection_name) + 10);
    
    LOG_INFO("DB: Collection created successfully: %s", collection_name);
    return 1;
}