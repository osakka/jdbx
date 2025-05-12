#include "database/database.h"
#include "utils/cache.h"
#include "utils/logger.h"
#include "query/query_language.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>   /* For time-based UUID generation */
#include <regex.h>  /* For regex pattern validation */
#include <ctype.h>  /* For isdigit() */

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

/* Create collection */
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
    
    pthread_mutex_unlock(&db->lock);
    
    return result;
}

/* List collections */
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

/* Generate UUID using our simple implementation */
static char* generate_uuid() {
    return generate_simple_uuid();
}

/* Insert document */
json_value_t* db_insert_document(database_t* db, const char* collection_name, json_value_t* document) {
    if (!db || !collection_name || !document || document->type != JSON_OBJECT) {
        return NULL;
    }
    
    pthread_mutex_lock(&db->lock);
    
    /* Get collection */
    json_value_t* collection = json_object_get(db->collections, collection_name);
    if (!collection || collection->type != JSON_ARRAY) {
        pthread_mutex_unlock(&db->lock);
        return NULL;
    }
    
    /* Lock collection */
    pthread_mutex_t* collection_lock = &db_get_collection(db, collection_name)->lock;
    pthread_mutex_lock(collection_lock);
    
    /* Generate ID if not provided */
    if (!json_object_has(document, "_id")) {
        char* id = generate_uuid();
        if (id) {
            json_object_set(document, "_id", json_create_string(id));
            free(id);
        } else {
            pthread_mutex_unlock(collection_lock);
            pthread_mutex_unlock(&db->lock);
            return NULL;
        }
    }
    
    /* Add document to collection */
    json_array_append(collection, document);
    
    db->is_modified = 1;
    
    /* Create result */
    json_value_t* result = json_create_object();
    if (result) {
        json_value_t* id = json_object_get(document, "_id");
        if (id) {
            json_object_set(result, "_id", json_create_string(id->value.string));
        }
    }
    
    /* Invalidate collection cache if cache is enabled */
    if (db->cache_enabled && db->cache) {
        /* Invalidate collection cache */
        char* coll_key = generate_collection_cache_key(collection_name);
        if (coll_key) {
            cache_remove(db->cache, coll_key);
            free(coll_key);
        }
    }
    
    pthread_mutex_unlock(collection_lock);
    pthread_mutex_unlock(&db->lock);
    
    return result;
}

/* Get document by ID */
json_value_t* db_get_document(database_t* db, const char* collection_name, const char* id) {
    if (!db || !collection_name || !id) {
        return NULL;
    }
    
    /* Check cache first if enabled */
    if (db->cache_enabled && db->cache) {
        /* Generate cache key */
        char* cache_key = generate_cache_key(collection_name, id);
        if (cache_key) {
            /* Try to get from cache */
            json_value_t* cached_doc = cache_get(db->cache, cache_key);
            
            free(cache_key);
            
            if (cached_doc) {
                /* Document found in cache */
                return cached_doc;
            }
        }
    }
    
    pthread_mutex_lock(&db->lock);
    
    /* Get collection */
    json_value_t* collection = json_object_get(db->collections, collection_name);
    if (!collection || collection->type != JSON_ARRAY) {
        pthread_mutex_unlock(&db->lock);
        return NULL;
    }
    
    /* Lock collection */
    pthread_mutex_t* collection_lock = &db_get_collection(db, collection_name)->lock;
    pthread_mutex_lock(collection_lock);
    
    /* Find document by ID */
    json_value_t* document = NULL;
    
    for (size_t i = 0; i < collection->value.array.size; i++) {
        json_value_t* doc = collection->value.array.items[i];
        if (doc->type == JSON_OBJECT) {
            json_value_t* doc_id = json_object_get(doc, "_id");
            if (doc_id && doc_id->type == JSON_STRING && 
                strcmp(doc_id->value.string, id) == 0) {
                /* Create deep copy of document */
                char* doc_str = json_stringify(doc);
                document = json_parse(doc_str);
                free(doc_str);
                break;
            }
        }
    }
    
    pthread_mutex_unlock(collection_lock);
    pthread_mutex_unlock(&db->lock);
    
    /* Add to cache if found and cache is enabled */
    if (document && db->cache_enabled && db->cache) {
        char* cache_key = generate_cache_key(collection_name, id);
        if (cache_key) {
            cache_put(db->cache, cache_key, document, 0);  /* Use default TTL */
            free(cache_key);
        }
    }
    
    return document;
}

/* Update document by ID */
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
    
    /* Lock collection */
    pthread_mutex_t* collection_lock = &db_get_collection(db, collection_name)->lock;
    pthread_mutex_lock(collection_lock);
    
    /* Find document by ID */
    int found = 0;
    
    for (size_t i = 0; i < collection->value.array.size; i++) {
        json_value_t* doc = collection->value.array.items[i];
        if (doc->type == JSON_OBJECT) {
            json_value_t* doc_id = json_object_get(doc, "_id");
            if (doc_id && doc_id->type == JSON_STRING && 
                strcmp(doc_id->value.string, id) == 0) {
                /* Keep _id field */
                json_object_set(document, "_id", json_create_string(id));
                
                /* Replace document */
                json_free(doc);
                collection->value.array.items[i] = document;
                
                found = 1;
                db->is_modified = 1;
                break;
            }
        }
    }
    
    /* Create result */
    json_value_t* result = NULL;
    
    if (found) {
        result = json_create_object();
        if (result) {
            json_object_set(result, "_id", json_create_string(id));
        }
        
        /* Remove from cache if cache is enabled */
        if (db->cache_enabled && db->cache) {
            char* cache_key = generate_cache_key(collection_name, id);
            if (cache_key) {
                cache_remove(db->cache, cache_key);
                
                /* Also invalidate collection cache */
                char* coll_key = generate_collection_cache_key(collection_name);
                if (coll_key) {
                    cache_remove(db->cache, coll_key);
                    free(coll_key);
                }
                
                free(cache_key);
            }
        }
    }
    
    pthread_mutex_unlock(collection_lock);
    pthread_mutex_unlock(&db->lock);
    
    return result;
}

/* Delete document by ID */
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
    
    /* Lock collection */
    pthread_mutex_t* collection_lock = &db_get_collection(db, collection_name)->lock;
    pthread_mutex_lock(collection_lock);
    
    /* Find document by ID */
    int found = 0;
    
    for (size_t i = 0; i < collection->value.array.size; i++) {
        json_value_t* doc = collection->value.array.items[i];
        if (doc->type == JSON_OBJECT) {
            json_value_t* doc_id = json_object_get(doc, "_id");
            if (doc_id && doc_id->type == JSON_STRING && 
                strcmp(doc_id->value.string, id) == 0) {
                /* Remove document */
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
    
    /* If document was found and cache is enabled, remove from cache */
    if (found && db->cache_enabled && db->cache) {
        char* cache_key = generate_cache_key(collection_name, id);
        if (cache_key) {
            cache_remove(db->cache, cache_key);
            free(cache_key);
        }
        
        /* Also invalidate collection cache */
        char* coll_key = generate_collection_cache_key(collection_name);
        if (coll_key) {
            cache_remove(db->cache, coll_key);
            free(coll_key);
        }
    }
    
    pthread_mutex_unlock(collection_lock);
    pthread_mutex_unlock(&db->lock);
    
    return found;
}

/* Helper function to match query with document */
/* Legacy match_query - kept for compatibility with older code paths */
#ifdef LEGACY_QUERY_SUPPORT
static int match_query(json_value_t* document, json_value_t* query) {
    /* Parse query using the new query language parser */
    query_parse_result_t result = query_parse(query);
    if (result.error) {
        free(result.error);
        return 0;
    }
    
    /* Execute query */
    int matches = query_match_document(result.expr, document);
    
    /* Free resources */
    query_free_parse_result(&result);
    
    return matches;
}
#endif

/* Check if we can use an index for a query */
static index_t* find_suitable_index(db_collection_t* collection, json_value_t* query, 
                                  const char** index_field, const char** index_value) {
    if (!collection || !query || query->type != JSON_OBJECT || !index_field || !index_value) {
        return NULL;
    }

    /* Only use index if query has a single field (for now) */
    if (query->value.object.size != 1) {
        return NULL;
    }

    /* Get the field and value from query */
    const char* field = query->value.object.entries[0].key;
    json_value_t* value = query->value.object.entries[0].value;
    
    /* Only use index for string or number fields (for now) */
    if (value->type != JSON_STRING && value->type != JSON_NUMBER && value->type != JSON_INTEGER) {
        return NULL;
    }
    
    /* Convert value to string */
    char value_str[1024];
    if (value->type == JSON_STRING) {
        strncpy(value_str, value->value.string, sizeof(value_str) - 1);
        value_str[sizeof(value_str) - 1] = '\0';
    } else if (value->type == JSON_NUMBER) {
        snprintf(value_str, sizeof(value_str), "%f", value->value.number);
    } else if (value->type == JSON_INTEGER) {
        snprintf(value_str, sizeof(value_str), "%lld", (long long)value->value.integer);
    }
    
    /* Look for an index on this field */
    index_t* index = collection->indexes;
    while (index) {
        if (strcmp(index->field_path, field) == 0) {
            /* Found a matching index */
            *index_field = field;
            *index_value = strdup(value_str);
            return index;
        }
        index = index->next;
    }
    
    return NULL;
}

/* Query documents */
json_value_t* db_query_documents(database_t* db, const char* collection_name, json_value_t* query_json) {
    if (!db || !collection_name) {
        return NULL;
    }
    
    /* Empty query matches all documents */
    if (!query_json) {
        query_json = json_create_object();
        
        /* Check cache for empty query (all documents) if cache is enabled */
        if (db->cache_enabled && db->cache) {
            /* Generate collection cache key */
            char* cache_key = generate_collection_cache_key(collection_name);
            if (cache_key) {
                /* Try to get from cache */
                json_value_t* cached_docs = cache_get(db->cache, cache_key);
                
                free(cache_key);
                
                if (cached_docs) {
                    /* Documents found in cache */
                    return cached_docs;
                }
            }
        }
    }
    
    /* Parse the query */
    query_parse_result_t query_result = query_parse(query_json);
    if (query_result.error) {
        fprintf(stderr, "Query parse error: %s\n", query_result.error);
        query_free_parse_result(&query_result);
        return NULL;
    }
    
    pthread_mutex_lock(&db->lock);
    
    /* Get collection */
    json_value_t* collection = json_object_get(db->collections, collection_name);
    if (!collection || collection->type != JSON_ARRAY) {
        pthread_mutex_unlock(&db->lock);
        query_free_parse_result(&query_result);
        return NULL;
    }
    
    /* Get collection object */
    db_collection_t* coll = db_get_collection(db, collection_name);
    if (!coll) {
        pthread_mutex_unlock(&db->lock);
        query_free_parse_result(&query_result);
        return NULL;
    }
    
    /* Lock collection */
    pthread_mutex_lock(&coll->lock);
    
    /* Check if we can use an index for this query */
    const char* index_field = NULL;
    const char* index_value = NULL;
    index_t* index = find_suitable_index(coll, query_json, &index_field, &index_value);
    
    /* Create a copy of the documents collection for query execution */
    json_value_t* documents_copy = NULL;
    
    if (index && index_value) {
        /* Use index for query */
        documents_copy = db_query_by_index(db, collection_name, index_field, index_value, 0, 0);
        free((void*)index_value);
    } else {
        /* No suitable index, create a copy of all documents */
        documents_copy = json_create_array();
        if (!documents_copy) {
            pthread_mutex_unlock(&coll->lock);
            pthread_mutex_unlock(&db->lock);
            query_free_parse_result(&query_result);
            return NULL;
        }
        
        for (size_t i = 0; i < collection->value.array.size; i++) {
            json_value_t* doc = collection->value.array.items[i];
            if (doc->type == JSON_OBJECT) {
                json_array_append(documents_copy, json_clone(doc));
            }
        }
    }
    
    pthread_mutex_unlock(&coll->lock);
    pthread_mutex_unlock(&db->lock);
    
    /* Execute query against documents */
    if (!documents_copy) {
        query_free_parse_result(&query_result);
        return json_create_array(); /* Return empty array on error */
    }
    
    query_result_t execute_result = query_execute(query_result.expr, documents_copy, &query_result.options);
    
    /* Free documents copy as we don't need it anymore */
    json_free(documents_copy);
    
    /* Create API response with pagination information */
    json_value_t* response = json_create_object();
    
    /* Add documents array */
    json_object_set(response, "documents", execute_result.documents);
    
    /* Add count and total_count */
    json_object_set(response, "count", json_create_integer(execute_result.count));
    json_object_set(response, "total_count", json_create_integer(execute_result.total_count));
    
    /* Add pagination information if available */
    if (execute_result.pagination) {
        json_value_t* pagination_json = query_pagination_to_json(execute_result.pagination);
        if (pagination_json) {
            json_object_set(response, "pagination", pagination_json);
        }
        query_free_pagination_info(execute_result.pagination);
    }
    
    /* Free query parse result */
    query_free_parse_result(&query_result);
    
    /* Cache the result for empty queries (all documents) if cache is enabled */
    if (query_json && query_json->type == JSON_OBJECT && json_object_size(query_json) == 0 && 
        db->cache_enabled && db->cache) {
        
        /* Generate collection cache key */
        char* cache_key = generate_collection_cache_key(collection_name);
        if (cache_key) {
            /* Store in cache with default TTL */
            cache_put(db->cache, cache_key, response, 0);
            free(cache_key);
        }
    }
    
    return response;
}