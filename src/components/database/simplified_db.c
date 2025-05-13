#include "database/database.h"
#include "utils/logger.h"
#include "query/query_language.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>  /* For access() and F_OK */
#include <errno.h>   /* For errno and strerror() */

/**
 * Complete deadlock-free implementations of database operations
 * This file provides simplified thread-safe implementations without caching
 */

/* Local helper function to generate a simple ID */
static char* generate_simple_id() {
    char* id = (char*)malloc(64);
    if (!id) return NULL;
    
    time_t now = time(NULL);
    snprintf(id, 64, "doc-%ld-%d", (long)now, rand() % 10000);
    return id;
}

/**
 * Initialize database
 */
database_t* db_init(const char* path) {
    if (!path) {
        LOG_ERROR("Failed to initialize database: path is NULL");
        return NULL;
    }

    LOG_INFO("Initializing database with path: %s", path);

    database_t* db = (database_t*)malloc(sizeof(database_t));
    if (!db) {
        LOG_ERROR("Failed to allocate memory for database structure");
        return NULL;
    }

    /* Initialize database fields */
    db->path = strdup(path);
    db->collections = json_create_object();
    pthread_mutex_init(&db->lock, NULL);
    db->is_modified = 0;
    db->cache = NULL;
    db->cache_enabled = 0;  /* Cache always disabled in simplified version */

    LOG_DEBUG("Database structure initialized successfully");

    /* Load database if file exists */
    if (access(path, F_OK) != -1) {
        LOG_INFO("Existing database file found, loading from %s", path);
        db_load(db);
    } else {
        LOG_INFO("No existing database file found at %s, starting with empty database", path);
    }

    LOG_INFO("Database initialization complete");
    return db;
}

/**
 * Close database
 */
void db_close(database_t* db) {
    if (!db) {
        LOG_WARNING("Attempted to close NULL database");
        return;
    }

    LOG_INFO("Closing database at path: %s", db->path ? db->path : "unknown");

    /* Save database if modified */
    if (db->is_modified) {
        LOG_INFO("Database has unsaved changes, saving before close");
        db_save(db);
    } else {
        LOG_DEBUG("No unsaved changes, skipping save operation");
    }

    /* Free resources */
    LOG_DEBUG("Freeing database resources");
    free(db->path);
    json_free(db->collections);

    /* Free cache if enabled - should never be true in simplified version */
    if (db->cache) {
        LOG_DEBUG("Destroying database cache");
        cache_destroy(db->cache);
    }

    pthread_mutex_destroy(&db->lock);
    free(db);
    LOG_INFO("Database closed successfully");
}

/**
 * Save database to file
 */
int db_save(database_t* db) {
    if (!db || !db->path) {
        LOG_ERROR("Failed to save database: NULL database or path");
        return 0;
    }

    LOG_INFO("Saving database to path: %s", db->path);

    pthread_mutex_lock(&db->lock);

    /* Stringify collections */
    LOG_DEBUG("Serializing database collections to JSON");
    char* json_str = json_stringify(db->collections);
    if (!json_str) {
        LOG_ERROR("Failed to serialize database collections to JSON");
        pthread_mutex_unlock(&db->lock);
        return 0;
    }

    /* Write to file */
    LOG_DEBUG("Opening database file for writing: %s", db->path);
    FILE* file = fopen(db->path, "w");
    if (!file) {
        LOG_ERROR("Failed to open database file for writing: %s (error: %s)",
                 db->path, strerror(errno));
        free(json_str);
        pthread_mutex_unlock(&db->lock);
        return 0;
    }

    int result = fputs(json_str, file) != EOF;
    if (!result) {
        LOG_ERROR("Error writing to database file: %s", strerror(errno));
    }

    fclose(file);
    free(json_str);

    db->is_modified = 0;

    pthread_mutex_unlock(&db->lock);

    if (result) {
        LOG_INFO("Database saved successfully");
    }

    return result;
}

/**
 * Load database from file
 */
int db_load(database_t* db) {
    if (!db || !db->path) {
        LOG_ERROR("Failed to load database: NULL database or path");
        return 0;
    }

    LOG_INFO("Loading database from path: %s", db->path);

    pthread_mutex_lock(&db->lock);

    /* Read file */
    LOG_DEBUG("Opening database file for reading: %s", db->path);
    FILE* file = fopen(db->path, "r");
    if (!file) {
        LOG_ERROR("Failed to open database file for reading: %s (error: %s)",
                 db->path, strerror(errno));
        pthread_mutex_unlock(&db->lock);
        return 0;
    }

    /* Get file size */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    LOG_DEBUG("Database file size: %ld bytes", file_size);

    /* Allocate buffer */
    char* buffer = (char*)malloc(file_size + 1);
    if (!buffer) {
        LOG_ERROR("Failed to allocate memory for database file content (%ld bytes)", file_size + 1);
        fclose(file);
        pthread_mutex_unlock(&db->lock);
        return 0;
    }

    /* Read file content */
    size_t read_size = fread(buffer, 1, file_size, file);
    buffer[read_size] = '\0';

    LOG_DEBUG("Read %zu bytes from database file", read_size);

    fclose(file);

    /* Parse JSON */
    LOG_DEBUG("Parsing database JSON");
    json_value_t* collections = json_parse(buffer);
    free(buffer);

    if (!collections || collections->type != JSON_OBJECT) {
        LOG_ERROR("Failed to parse database file as JSON object");
        if (collections) {
            json_free(collections);
        }
        pthread_mutex_unlock(&db->lock);
        return 0;
    }

    LOG_DEBUG("Successfully parsed database JSON");

    /* Free old collections */
    LOG_DEBUG("Freeing old database collections");
    json_free(db->collections);

    /* Set new collections */
    LOG_DEBUG("Setting new database collections");
    db->collections = collections;

    /* Count the number of collections for logging */
    size_t collection_count = 0;
    if (collections && collections->type == JSON_OBJECT) {
        collection_count = json_object_size(collections);
    }

    LOG_DEBUG("Found %zu collections in database", collection_count);

    LOG_INFO("Database loaded successfully with %zu collections", collection_count);

    pthread_mutex_unlock(&db->lock);

    return 1;
}

/**
 * Create collection
 */
int db_create_collection(database_t* db, const char* name) {
    if (!db || !name) {
        return 0;
    }
    
    pthread_mutex_lock(&db->lock);

    /* Check if collection already exists */
    if (json_object_has(db->collections, name)) {
        pthread_mutex_unlock(&db->lock);
        return 0;
    }

    /* Create new collection (array of documents) */
    json_value_t* collection = json_create_array();
    if (!collection) {
        pthread_mutex_unlock(&db->lock);
        return 0;
    }

    /* Add collection to database */
    json_object_set(db->collections, name, collection);

    db->is_modified = 1;

    pthread_mutex_unlock(&db->lock);
    
    return 1;
}

/**
 * Drop collection
 */
int db_drop_collection(database_t* db, const char* name) {
    if (!db || !name) {
        return 0;
    }
    
    pthread_mutex_lock(&db->lock);
    
    /* Check if collection exists */
    if (!json_object_has(db->collections, name)) {
        pthread_mutex_unlock(&db->lock);
        return 0;
    }
    
    /* Remove collection */
    json_object_remove(db->collections, name);
    
    db->is_modified = 1;
    
    pthread_mutex_unlock(&db->lock);
    
    return 1;
}

/**
 * Get collection
 */
db_collection_t* db_get_collection(database_t* db, const char* name) {
    if (!db || !name) {
        return NULL;
    }

    pthread_mutex_lock(&db->lock);

    /* Check if collection exists */
    json_value_t* collection = json_object_get(db->collections, name);
    if (!collection || collection->type != JSON_ARRAY) {
        pthread_mutex_unlock(&db->lock);
        return NULL;
    }

    /* Create collection structure */
    db_collection_t* result = (db_collection_t*)malloc(sizeof(db_collection_t));
    if (!result) {
        pthread_mutex_unlock(&db->lock);
        return NULL;
    }

    /* Initialize collection fields */
    result->name = strdup(name);
    result->documents = collection;
    pthread_mutex_init(&result->lock, NULL);
    result->schema = NULL; /* Initialize schema to NULL */
    result->indexes = NULL; /* Initialize indexes to NULL */

    pthread_mutex_unlock(&db->lock);

    return result;
}

/**
 * List collections
 */
json_value_t* db_list_collections(database_t* db) {
    if (!db) {
        return NULL;
    }
    
    pthread_mutex_lock(&db->lock);
    
    /* Create array of collection names */
    json_value_t* result = json_create_array();
    if (!result) {
        pthread_mutex_unlock(&db->lock);
        return NULL;
    }
    
    /* Iterate over collections */
    for (size_t i = 0; i < db->collections->value.object.size; i++) {
        json_value_t* name = json_create_string(db->collections->value.object.entries[i].key);
        if (name) {
            json_array_append(result, name);
        }
    }
    
    pthread_mutex_unlock(&db->lock);
    
    return result;
}

/**
 * Insert document - simplified thread-safe implementation
 */
json_value_t* db_insert_document(database_t* db, const char* collection_name, json_value_t* document) {
    LOG_INFO("Starting document insertion for collection '%s'", collection_name ? collection_name : "NULL");
    
    if (!db || !collection_name || !document || document->type != JSON_OBJECT) {
        LOG_ERROR("Invalid parameters for db_insert_document");
        return NULL;
    }
    
    LOG_DEBUG("Cloning document for insertion");
    json_value_t* doc_copy = json_clone(document);
    if (!doc_copy) {
        LOG_ERROR("Failed to clone document for insertion");
        return NULL;
    }
    
    LOG_DEBUG("Generating document ID if needed");
    if (!json_object_has(doc_copy, "_id")) {
        char* id = generate_simple_id();
        if (id) {
            LOG_DEBUG("Generated new ID: %s", id);
            json_object_set(doc_copy, "_id", json_create_string(id));
            free(id);
        } else {
            LOG_ERROR("Failed to generate ID for document");
            json_free(doc_copy);
            return NULL;
        }
    }
    
    LOG_DEBUG("Verifying document has valid ID");
    json_value_t* id = json_object_get(doc_copy, "_id");
    if (!id || id->type != JSON_STRING) {
        LOG_ERROR("Document has no valid ID after preparation");
        json_free(doc_copy);
        return NULL;
    }
    const char* id_str = id->value.string;
    
    LOG_DEBUG("Creating result object with ID: %s", id_str);
    json_value_t* result = json_create_object();
    if (!result) {
        LOG_ERROR("Failed to create result object");
        json_free(doc_copy);
        return NULL;
    }
    json_object_set(result, "_id", json_create_string(id_str));
    
    LOG_DEBUG("Acquiring database lock");
    pthread_mutex_lock(&db->lock);
    
    LOG_DEBUG("Getting collection: %s", collection_name);
    json_value_t* collection = json_object_get(db->collections, collection_name);
    if (!collection || collection->type != JSON_ARRAY) {
        LOG_ERROR("Collection '%s' not found or not an array", collection_name);
        pthread_mutex_unlock(&db->lock);
        json_free(result);
        json_free(doc_copy);
        return NULL;
    }
    
    LOG_DEBUG("Adding document to collection");
    json_array_append(collection, doc_copy);
    db->is_modified = 1;
    
    LOG_DEBUG("Releasing database lock");
    pthread_mutex_unlock(&db->lock);
    
    LOG_INFO("Document inserted successfully with ID: %s", id_str);
    
    return result;
}

/**
 * Get document by ID - simplified thread-safe implementation
 */
json_value_t* db_get_document(database_t* db, const char* collection_name, const char* id) {
    LOG_INFO("Starting document retrieval for collection '%s', ID '%s'", 
             collection_name ? collection_name : "NULL", 
             id ? id : "NULL");
    
    if (!db || !collection_name || !id) {
        LOG_ERROR("Invalid parameters for db_get_document");
        return NULL;
    }
    
    LOG_DEBUG("Acquiring database lock");
    pthread_mutex_lock(&db->lock);
    
    LOG_DEBUG("Getting collection: %s", collection_name);
    json_value_t* collection = json_object_get(db->collections, collection_name);
    if (!collection || collection->type != JSON_ARRAY) {
        LOG_ERROR("Collection '%s' not found or not an array", collection_name);
        pthread_mutex_unlock(&db->lock);
        return NULL;
    }
    
    LOG_DEBUG("Finding document by ID: %s", id);
    json_value_t* document = NULL;
    
    for (size_t i = 0; i < collection->value.array.size; i++) {
        json_value_t* doc = collection->value.array.items[i];
        if (doc && doc->type == JSON_OBJECT) {
            json_value_t* doc_id = json_object_get(doc, "_id");
            if (doc_id && doc_id->type == JSON_STRING &&
                strcmp(doc_id->value.string, id) == 0) {
                
                LOG_DEBUG("Document found, creating clone");
                document = json_clone(doc);
                break;
            }
        }
    }
    
    LOG_DEBUG("Releasing database lock");
    pthread_mutex_unlock(&db->lock);
    
    if (document) {
        LOG_INFO("Document retrieved successfully");
    } else {
        LOG_WARNING("Document with ID '%s' not found in collection '%s'", id, collection_name);
    }
    
    return document;
}

/**
 * Update document by ID - simplified thread-safe implementation
 */
json_value_t* db_update_document(database_t* db, const char* collection_name, const char* id,
                                json_value_t* document) {
    LOG_INFO("Starting document update for collection '%s', ID '%s'", 
             collection_name ? collection_name : "NULL", 
             id ? id : "NULL");
    
    if (!db || !collection_name || !id || !document || document->type != JSON_OBJECT) {
        LOG_ERROR("Invalid parameters for db_update_document");
        return NULL;
    }
    
    LOG_DEBUG("Cloning document for update");
    json_value_t* doc_copy = json_clone(document);
    if (!doc_copy) {
        LOG_ERROR("Failed to clone document for update");
        return NULL;
    }
    
    LOG_DEBUG("Adding ID to document: %s", id);
    json_object_set(doc_copy, "_id", json_create_string(id));
    
    LOG_DEBUG("Creating result object");
    json_value_t* result = json_create_object();
    if (!result) {
        LOG_ERROR("Failed to create result object");
        json_free(doc_copy);
        return NULL;
    }
    json_object_set(result, "_id", json_create_string(id));
    
    LOG_DEBUG("Acquiring database lock");
    pthread_mutex_lock(&db->lock);
    
    LOG_DEBUG("Getting collection: %s", collection_name);
    json_value_t* collection = json_object_get(db->collections, collection_name);
    if (!collection || collection->type != JSON_ARRAY) {
        LOG_ERROR("Collection '%s' not found or not an array", collection_name);
        pthread_mutex_unlock(&db->lock);
        json_free(result);
        json_free(doc_copy);
        return NULL;
    }
    
    LOG_DEBUG("Finding document by ID: %s", id);
    int found = 0;
    
    for (size_t i = 0; i < collection->value.array.size; i++) {
        json_value_t* doc = collection->value.array.items[i];
        if (doc && doc->type == JSON_OBJECT) {
            json_value_t* doc_id = json_object_get(doc, "_id");
            
            if (doc_id && doc_id->type == JSON_STRING &&
                strcmp(doc_id->value.string, id) == 0) {
                
                LOG_DEBUG("Document found, replacing with updated version");
                json_free(doc);
                collection->value.array.items[i] = doc_copy;
                
                found = 1;
                db->is_modified = 1;
                break;
            }
        }
    }
    
    LOG_DEBUG("Releasing database lock");
    pthread_mutex_unlock(&db->lock);
    
    if (!found) {
        LOG_ERROR("Document with ID '%s' not found in collection '%s'", id, collection_name);
        json_free(doc_copy);
        json_free(result);
        return NULL;
    }
    
    LOG_INFO("Document updated successfully");
    
    return result;
}

/**
 * Delete document by ID - simplified thread-safe implementation
 */
int db_delete_document(database_t* db, const char* collection_name, const char* id) {
    LOG_INFO("Starting document deletion for collection '%s', ID '%s'", 
             collection_name ? collection_name : "NULL", 
             id ? id : "NULL");
    
    if (!db || !collection_name || !id) {
        LOG_ERROR("Invalid parameters for db_delete_document");
        return 0;
    }
    
    LOG_DEBUG("Acquiring database lock");
    pthread_mutex_lock(&db->lock);
    
    LOG_DEBUG("Getting collection: %s", collection_name);
    json_value_t* collection = json_object_get(db->collections, collection_name);
    if (!collection || collection->type != JSON_ARRAY) {
        LOG_ERROR("Collection '%s' not found or not an array", collection_name);
        pthread_mutex_unlock(&db->lock);
        return 0;
    }
    
    LOG_DEBUG("Finding document by ID: %s", id);
    int found = 0;
    
    for (size_t i = 0; i < collection->value.array.size; i++) {
        json_value_t* doc = collection->value.array.items[i];
        if (doc && doc->type == JSON_OBJECT) {
            json_value_t* doc_id = json_object_get(doc, "_id");
            if (doc_id && doc_id->type == JSON_STRING &&
                strcmp(doc_id->value.string, id) == 0) {
                
                LOG_DEBUG("Document found, removing from collection");
                json_free(doc);
                
                /* Move remaining documents */
                for (size_t j = i; j < collection->value.array.size - 1; j++) {
                    collection->value.array.items[j] = collection->value.array.items[j + 1];
                }
                
                collection->value.array.size--;
                
                found = 1;
                db->is_modified = 1;
                break;
            }
        }
    }
    
    LOG_DEBUG("Releasing database lock");
    pthread_mutex_unlock(&db->lock);
    
    if (found) {
        LOG_INFO("Document deleted successfully");
    } else {
        LOG_WARNING("Document with ID '%s' not found in collection '%s'", id, collection_name);
    }
    
    return found;
}

/**
 * Query documents - simplified thread-safe implementation
 */
json_value_t* db_query_documents(database_t* db, const char* collection_name, json_value_t* query_json) {
    LOG_INFO("Starting document query for collection '%s'", collection_name ? collection_name : "NULL");
    
    if (!db || !collection_name) {
        LOG_ERROR("Invalid parameters for db_query_documents");
        return NULL;
    }
    
    /* Create query object if not provided */
    int empty_query = 0;
    if (!query_json) {
        LOG_DEBUG("Creating empty query object");
        query_json = json_create_object();
        empty_query = 1;
    }
    
    /* Parse the query */
    LOG_DEBUG("Parsing query");
    query_parse_result_t query_result = query_parse(query_json);
    if (query_result.error) {
        LOG_ERROR("Query parse error: %s", query_result.error);
        query_free_parse_result(&query_result);
        if (empty_query) {
            json_free(query_json);
        }
        return NULL;
    }
    
    LOG_DEBUG("Acquiring database lock");
    pthread_mutex_lock(&db->lock);
    
    LOG_DEBUG("Getting collection: %s", collection_name);
    json_value_t* collection = json_object_get(db->collections, collection_name);
    if (!collection || collection->type != JSON_ARRAY) {
        LOG_ERROR("Collection '%s' not found or not an array", collection_name);
        pthread_mutex_unlock(&db->lock);
        query_free_parse_result(&query_result);
        if (empty_query) {
            json_free(query_json);
        }
        return NULL;
    }
    
    LOG_DEBUG("Creating copy of collection documents");
    json_value_t* documents_copy = json_create_array();
    if (!documents_copy) {
        LOG_ERROR("Failed to create documents copy");
        pthread_mutex_unlock(&db->lock);
        query_free_parse_result(&query_result);
        if (empty_query) {
            json_free(query_json);
        }
        return NULL;
    }
    
    LOG_DEBUG("Cloning all documents in collection");
    for (size_t i = 0; i < collection->value.array.size; i++) {
        json_value_t* doc = collection->value.array.items[i];
        if (doc && doc->type == JSON_OBJECT) {
            json_array_append(documents_copy, json_clone(doc));
        }
    }
    
    LOG_DEBUG("Releasing database lock");
    pthread_mutex_unlock(&db->lock);
    
    LOG_DEBUG("Executing query against documents");
    query_result_t execute_result = query_execute(query_result.expr, documents_copy, &query_result.options);
    
    LOG_DEBUG("Query execution complete, freeing document copy");
    json_free(documents_copy);
    
    LOG_DEBUG("Creating response object");
    json_value_t* response = json_create_object();
    if (!response) {
        LOG_ERROR("Failed to create response object");
        query_free_parse_result(&query_result);
        if (empty_query) {
            json_free(query_json);
        }
        return NULL;
    }
    
    LOG_DEBUG("Adding documents to response");
    json_object_set(response, "documents", execute_result.documents);
    
    LOG_DEBUG("Adding count information");
    json_object_set(response, "count", json_create_integer(execute_result.count));
    json_object_set(response, "total_count", json_create_integer(execute_result.total_count));
    
    LOG_DEBUG("Adding pagination information if available");
    if (execute_result.pagination) {
        json_value_t* pagination_json = query_pagination_to_json(execute_result.pagination);
        if (pagination_json) {
            json_object_set(response, "pagination", pagination_json);
        }
        query_free_pagination_info(execute_result.pagination);
    }
    
    LOG_DEBUG("Freeing query parse result");
    query_free_parse_result(&query_result);
    
    if (empty_query) {
        LOG_DEBUG("Freeing empty query object");
        json_free(query_json);
    }
    
    LOG_INFO("Query completed successfully");
    
    return response;
}

/* Empty cache operations that do nothing but provide compatibility */

/**
 * Enable document cache - completely disabled
 */
int db_enable_cache(database_t* db, int capacity, int ttl) {
    if (!db) {
        return 0;
    }
    
    LOG_WARNING("Cache operations are permanently disabled in deadlock-free implementation");
    
    /* No cache creation at all */
    db->cache = NULL;
    db->cache_enabled = 0;
    
    /* Suppress unused parameter warnings */
    (void)capacity;
    (void)ttl;
    
    return 1;
}

/**
 * Disable document cache - already disabled
 */
int db_disable_cache(database_t* db) {
    if (!db) {
        return 0;
    }
    
    LOG_INFO("Cache already disabled in deadlock-free implementation");
    
    return 1;
}

/**
 * Configure document cache - does nothing
 */
int db_configure_cache(database_t* db, int capacity, int ttl, const char* type, double max_memory_mb) {
    if (!db) {
        return 0;
    }
    
    LOG_WARNING("Cache operations are permanently disabled in deadlock-free implementation");
    
    /* Suppress unused parameter warnings */
    (void)capacity;
    (void)ttl;
    (void)type;
    (void)max_memory_mb;
    
    return 1;
}

/**
 * Get cache statistics - returns empty stats
 */
json_value_t* db_get_cache_stats(database_t* db) {
    json_value_t* stats = json_create_object();
    
    if (stats) {
        json_object_set(stats, "enabled", json_create_boolean(0));
        json_object_set(stats, "message", json_create_string("Cache permanently disabled in deadlock-free implementation"));
    }
    
    /* Suppress unused parameter warnings */
    (void)db;
    
    return stats;
}

/**
 * Clear cache - does nothing
 */
int db_clear_cache(database_t* db) {
    /* Suppress unused parameter warnings */
    (void)db;
    
    return 1;
}