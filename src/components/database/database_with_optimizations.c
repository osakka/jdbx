#include "database/database.h"
#include "utils/cache.h"
#include "utils/logger.h"
#include "query/query_language.h"
#include "database/optimized_ops.h"  
#include "database/indexed_ops.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>   /* For time-based UUID generation */
#include <regex.h>  /* For regex pattern validation */
#include <ctype.h>  /* For isdigit() */

/**
 * This file is a modified version of database.c with optimized and indexed operations
 * integrated for improved performance.
 */

/* Forward declare functions */
static int process_cache_invalidations(database_t* db);

/* Structure to hold collection names for invalidation */
typedef struct {
    char names[20][256];
    int count;
} invalidation_list_t;

/* Generate a simple UUID replacement since uuid/uuid.h may not be available */
static char* generate_simple_uuid() {
    char* uuid = (char*)malloc(37);  /* 36 chars + null terminator */
    if (!uuid) return NULL;
    
    /* Format: 8-4-4-4-12 hexadecimal digits */
    srand(time(NULL));
    sprintf(uuid, "%08x-%04x-%04x-%04x-%04x%08x",
            rand() & 0xFFFFFFFF,
            rand() & 0xFFFF,
            ((rand() & 0xFFFF) & 0x0FFF) | 0x4000,  /* Version 4 */
            ((rand() & 0xFFFF) & 0x3FFF) | 0x8000,  /* Variant 1 */
            rand() & 0xFFFF,
            rand() & 0xFFFFFFFF);
    
    return uuid;
}

/* Cache management functions */

/* Generate cache key for a document */
static char* generate_cache_key(const char* collection, const char* id) {
    if (!collection || !id) return NULL;

    /* Format: "collection:id" */
    size_t len = strlen(collection) + 1 + strlen(id) + 1;  /* +1 for ':' and +1 for null terminator */
    char* key = (char*)malloc(len);
    if (!key) return NULL;

    snprintf(key, len, "%s:%s", collection, id);
    return key;
}

/* Generate cache key for a collection */
static char* generate_collection_cache_key(const char* collection) {
    if (!collection) return NULL;

    /* Format: "collection:" */
    size_t len = strlen(collection) + 2;  /* +1 for ':' and +1 for null terminator */
    char* key = (char*)malloc(len);
    if (!key) return NULL;

    snprintf(key, len, "%s:", collection);
    return key;
}

/**
 * Generate a cache key for a query on a collection
 *
 * Format: "query:collection:hash"
 * Where hash is a simple hash of the query JSON string
 */
static char* generate_query_cache_key(const char* collection, json_value_t* query) {
    if (!collection || !query) return NULL;

    /* Convert query to string for hashing */
    char* query_str = json_stringify(query);
    if (!query_str) return NULL;

    /* Generate a simple hash of the query string */
    unsigned int hash = 5381; /* djb2 hash function starting value */
    char* p = query_str;
    int c;

    while ((c = *p++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    /* Format: "query:collection:hash" */
    size_t len = strlen(collection) + 20; /* "query:" + collection + ":" + hash + null */
    char* key = (char*)malloc(len);
    if (!key) {
        free(query_str);
        return NULL;
    }

    snprintf(key, len, "query:%s:%u", collection, hash);

    free(query_str);
    return key;
}

/**
 * Store query result in cache
 *
 * @param db Database instance
 * @param collection_name Collection name
 * @param query_json Query used to generate the result
 * @param result Query result to cache
 * @param ttl Time to live for the cache entry (0 for default)
 * @return 1 on success, 0 on failure
 */
static int store_query_result(database_t* db, const char* collection_name,
                             json_value_t* query_json, json_value_t* result, time_t ttl) {
    if (!db || !db->cache_enabled || !db->cache ||
        !collection_name || !query_json || !result) {
        return 0;
    }

    char* cache_key = generate_query_cache_key(collection_name, query_json);
    if (!cache_key) return 0;

    int ret = cache_put(db->cache, cache_key, result, ttl);

    free(cache_key);
    return ret;
}

/**
 * Safely mark collection as needing cache invalidation
 *
 * @param db Database instance
 * @param collection_name Collection name
 * @return 1 if successful, 0 otherwise
 */
static int mark_collection_for_invalidation(database_t* db, const char* collection_name) {
    if (!db || !db->cache_enabled || !db->cache || !collection_name) {
        return 0;
    }

    /* Generate the collection invalidation key */
    char invalidation_key[512];
    snprintf(invalidation_key, sizeof(invalidation_key), "invalidate:%s", collection_name);

    /* Store a marker in the cache to indicate this collection needs invalidation */
    json_value_t* marker = json_create_boolean(1);
    if (!marker) {
        return 0;
    }

    /* Add to cache with very short TTL (1 second) */
    int result = cache_put(db->cache, invalidation_key, marker, 1);

    /* Free the marker as cache_put creates a copy */
    json_free(marker);

    return result;
}

/**
 * Process cache invalidation for collections marked for invalidation
 * This function should be called periodically by the cache cleanup process
 * or when acquiring the cache in read operations
 *
 * @param db Database instance
 * @return Number of collections processed
 */
static int process_cache_invalidations(database_t* db) {
    if (!db || !db->cache_enabled || !db->cache) {
        return 0;
    }

    int processed = 0;
    invalidation_list_t list = {0};

    /* Get all keys in the cache */
    char** all_keys = NULL;
    size_t key_count = cache_get_keys(db->cache, &all_keys);

    if (!all_keys) {
        return 0;
    }

    /* First pass: find all invalidation markers */
    for (size_t i = 0; i < key_count && list.count < 20; i++) {
        if (strncmp(all_keys[i], "invalidate:", 11) == 0) {
            /* Extract collection name */
            const char* collection_name = all_keys[i] + 11;

            /* Add to list if not already present */
            int already_in_list = 0;
            for (int j = 0; j < list.count; j++) {
                if (strcmp(list.names[j], collection_name) == 0) {
                    already_in_list = 1;
                    break;
                }
            }

            if (!already_in_list) {
                strncpy(list.names[list.count], collection_name, 255);
                list.names[list.count][255] = '\0';
                list.count++;
            }

            /* Remove the invalidation marker */
            cache_remove(db->cache, all_keys[i]);
            processed++;
        }
    }

    /* Free the keys from the first pass */
    for (size_t i = 0; i < key_count; i++) {
        free(all_keys[i]);
    }
    free(all_keys);

    /* Second pass: invalidate all matching keys for each collection */
    for (int i = 0; i < list.count; i++) {
        const char* collection = list.names[i];

        /* Get the keys again (since we modified the cache) */
        char** keys = NULL;
        size_t count = cache_get_keys(db->cache, &keys);

        if (!keys) {
            continue;
        }

        /* Create prefixes for matching */
        char query_prefix[280];
        snprintf(query_prefix, sizeof(query_prefix), "query:%s:", collection);
        size_t query_prefix_len = strlen(query_prefix);

        char coll_key[280];
        snprintf(coll_key, sizeof(coll_key), "%s:", collection);

        /* Remove matching entries */
        int invalidated = 0;
        for (size_t j = 0; j < count; j++) {
            if (strncmp(keys[j], query_prefix, query_prefix_len) == 0 ||
                strcmp(keys[j], coll_key) == 0) {
                cache_remove(db->cache, keys[j]);
                invalidated++;
                processed++;
            }
            free(keys[j]);
        }

        free(keys);

        if (invalidated > 0) {
            LOG_DEBUG("Invalidated %d cache entries for collection '%s'", invalidated, collection);
        }
    }

    return processed;
}

/**
 * Invalidate query cache entries for a collection
 *
 * @param db Database instance
 * @param collection_name Collection name
 * @return 1 if successful, 0 otherwise
 * 
 * Note: This function is currently unused but is kept for future use
 */
static int __attribute__((unused)) invalidate_query_cache(database_t* db, const char* collection_name) {
    if (!db || !db->cache_enabled || !db->cache || !collection_name) {
        return 0;
    }

    /* Mark the collection for invalidation - this is a lightweight operation
       that doesn't require complex lock coordination */
    return mark_collection_for_invalidation(db, collection_name);
}

/* Initialize database */
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
    db->cache_enabled = 0;

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

/* Close database */
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

    /* Free cache if enabled */
    if (db->cache) {
        LOG_DEBUG("Destroying database cache");
        cache_destroy(db->cache);
    }

    pthread_mutex_destroy(&db->lock);

    free(db);
    LOG_INFO("Database closed successfully");
}

/* Enable document cache */
int db_enable_cache(database_t* db, int capacity, int ttl) {
    if (!db) {
        return 0;
    }
    
    pthread_mutex_lock(&db->lock);
    
    /* If cache already exists, destroy it */
    if (db->cache) {
        cache_destroy(db->cache);
        db->cache = NULL;
    }
    
    /* Create cache configuration */
    cache_config_t config;
    
    /* Set default configuration */
    config.capacity = capacity > 0 ? capacity : 1000;  /* Default: 1000 entries */
    config.default_ttl = ttl > 0 ? ttl : 0;            /* Default: no expiration */
    config.type = CACHE_TYPE_LRU;                      /* Default: LRU */
    config.max_memory_mb = 0;                          /* Default: no memory limit */
    config.cleanup_interval = 60;                      /* Default: cleanup every minute */
    
    /* Create cache */
    db->cache = cache_create(&config);
    if (!db->cache) {
        pthread_mutex_unlock(&db->lock);
        return 0;
    }
    
    /* Enable cache */
    db->cache_enabled = 1;
    
    pthread_mutex_unlock(&db->lock);
    
    return 1;
}

/* Disable document cache */
int db_disable_cache(database_t* db) {
    if (!db) {
        return 0;
    }
    
    pthread_mutex_lock(&db->lock);
    
    /* If cache exists, destroy it */
    if (db->cache) {
        cache_destroy(db->cache);
        db->cache = NULL;
    }
    
    /* Disable cache */
    db->cache_enabled = 0;
    
    pthread_mutex_unlock(&db->lock);
    
    return 1;
}

/* Configure document cache */
int db_configure_cache(database_t* db, int capacity, int ttl, const char* type, double max_memory_mb) {
    if (!db || !db->cache) {
        return 0;
    }
    
    pthread_mutex_lock(&db->lock);
    
    /* Create cache configuration */
    cache_config_t config;
    
    /* Set configuration */
    config.capacity = capacity > 0 ? capacity : 1000;  /* Default: 1000 entries */
    config.default_ttl = ttl > 0 ? ttl : 0;            /* Default: no expiration */
    
    /* Set cache type */
    if (type) {
        if (strcasecmp(type, "lru") == 0) {
            config.type = CACHE_TYPE_LRU;
        } else if (strcasecmp(type, "lfu") == 0) {
            config.type = CACHE_TYPE_LFU;
        } else if (strcasecmp(type, "fifo") == 0) {
            config.type = CACHE_TYPE_FIFO;
        } else {
            config.type = CACHE_TYPE_LRU;  /* Default: LRU */
        }
    } else {
        config.type = CACHE_TYPE_LRU;  /* Default: LRU */
    }
    
    config.max_memory_mb = max_memory_mb > 0 ? max_memory_mb : 0;  /* Default: no memory limit */
    config.cleanup_interval = 60;                                 /* Default: cleanup every minute */
    
    /* Update cache configuration */
    cache_set_config(db->cache, &config);
    
    pthread_mutex_unlock(&db->lock);
    
    return 1;
}

/* Get cache statistics */
json_value_t* db_get_cache_stats(database_t* db) {
    if (!db || !db->cache) {
        /* Return empty stats if cache is not enabled */
        json_value_t* empty = json_create_object();
        json_object_set(empty, "enabled", json_create_boolean(0));
        return empty;
    }
    
    /* Get cache statistics */
    json_value_t* stats = cache_get_stats_json(db->cache);
    if (!stats) {
        /* Return empty stats on error */
        json_value_t* empty = json_create_object();
        json_object_set(empty, "enabled", json_create_boolean(1));
        json_object_set(empty, "error", json_create_string("Failed to get cache statistics"));
        return empty;
    }
    
    /* Add enabled flag */
    json_object_set(stats, "enabled", json_create_boolean(db->cache_enabled));
    
    return stats;
}

/* Clear cache */
int db_clear_cache(database_t* db) {
    if (!db || !db->cache) {
        return 0;
    }
    
    /* Clear cache */
    cache_clear(db->cache);
    
    return 1;
}

/* Save database to file */
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

/* Load database from file */
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

/* Collection operations */

/* Create collection - use optimized implementation */
int db_create_collection(database_t* db, const char* name) {
    LOG_DEBUG("Using optimized db_create_collection implementation");
    return optimized_db_create_collection(db, name);
}

/* Drop collection */
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

/* Get collection */
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

/* List collections - use optimized implementation */
json_value_t* db_list_collections(database_t* db) {
    LOG_DEBUG("Using optimized db_list_collections implementation");
    return optimized_db_list_collections(db);
}

/* Document operations - all using optimized/indexed implementations */

/* Insert document - use indexed implementation */
json_value_t* db_insert_document(database_t* db, const char* collection_name, json_value_t* document) {
    LOG_DEBUG("Using indexed db_insert_document implementation");
    return indexed_db_insert_document(db, collection_name, document);
}

/* Get document - use indexed implementation */
json_value_t* db_get_document(database_t* db, const char* collection_name, const char* id) {
    LOG_DEBUG("Using indexed db_get_document implementation");
    return indexed_db_get_document(db, collection_name, id);
}

/* Update document */
json_value_t* db_update_document(database_t* db, const char* collection_name, const char* id,
                                json_value_t* document) {
    if (!db || !collection_name || !id || !document || document->type != JSON_OBJECT) {
        return NULL;
    }
    
    pthread_mutex_lock(&db->lock);
    
    /* Get collection */
    json_value_t* collection = json_object_get(db->collections, collection_name);
    if (!collection || collection->type != JSON_ARRAY) {
        pthread_mutex_unlock(&db->lock);
        return NULL;
    }
    
    /* Clone document */
    json_value_t* doc_copy = json_clone(document);
    if (!doc_copy) {
        pthread_mutex_unlock(&db->lock);
        return NULL;
    }
    
    /* Set ID */
    json_object_set(doc_copy, "_id", json_create_string(id));
    
    /* Create result */
    json_value_t* result = json_create_object();
    if (!result) {
        json_free(doc_copy);
        pthread_mutex_unlock(&db->lock);
        return NULL;
    }
    json_object_set(result, "_id", json_create_string(id));
    
    /* Find document */
    int found = 0;
    for (size_t i = 0; i < collection->value.array.size; i++) {
        json_value_t* doc = collection->value.array.items[i];
        if (doc && doc->type == JSON_OBJECT) {
            json_value_t* doc_id = json_object_get(doc, "_id");
            if (doc_id && doc_id->type == JSON_STRING && 
                strcmp(doc_id->value.string, id) == 0) {
                /* Replace document */
                json_free(doc);
                collection->value.array.items[i] = doc_copy;
                found = 1;
                db->is_modified = 1;
                break;
            }
        }
    }
    
    pthread_mutex_unlock(&db->lock);
    
    if (!found) {
        json_free(doc_copy);
        json_free(result);
        return NULL;
    }
    
    return result;
}

/* Delete document */
int db_delete_document(database_t* db, const char* collection_name, const char* id) {
    if (!db || !collection_name || !id) {
        return 0;
    }
    
    pthread_mutex_lock(&db->lock);
    
    /* Get collection */
    json_value_t* collection = json_object_get(db->collections, collection_name);
    if (!collection || collection->type != JSON_ARRAY) {
        pthread_mutex_unlock(&db->lock);
        return 0;
    }
    
    /* Find document */
    int found = 0;
    for (size_t i = 0; i < collection->value.array.size; i++) {
        json_value_t* doc = collection->value.array.items[i];
        if (doc && doc->type == JSON_OBJECT) {
            json_value_t* doc_id = json_object_get(doc, "_id");
            if (doc_id && doc_id->type == JSON_STRING && 
                strcmp(doc_id->value.string, id) == 0) {
                /* Free document */
                json_free(doc);
                
                /* Move remaining documents */
                for (size_t j = i; j < collection->value.array.size - 1; j++) {
                    collection->value.array.items[j] = collection->value.array.items[j + 1];
                }
                
                /* Update size */
                collection->value.array.size--;
                
                found = 1;
                db->is_modified = 1;
                break;
            }
        }
    }
    
    pthread_mutex_unlock(&db->lock);
    
    return found;
}

/* Query documents - use indexed implementation */
json_value_t* db_query_documents(database_t* db, const char* collection_name, json_value_t* query_json) {
    LOG_DEBUG("Using indexed db_query_documents implementation");
    return indexed_db_query_documents(db, collection_name, query_json);
}

/* Rebuild document indices */
int db_rebuild_indices(database_t* db) {
    LOG_DEBUG("Rebuilding all indices for improved performance");
    return rebuild_all_indices(db);
}