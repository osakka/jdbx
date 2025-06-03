/*
 * JSONdb High-Performance Database - Clean Implementation
 * No backward compatibility, pure performance focus
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <stdatomic.h>

/* Suppress unused parameter warnings */
#define UNUSED(x) ((void)(x))

#include "database/database.h"
#include "storage/mmap_storage.h"
#include "index/hash_index.h"
#include "utils/json_helpers.h"
#include "utils/logger.h"
#include "utils/generic_cache.h"
//#include "database/js_integration.h"
//#include "transaction/transaction.h"

/* Maximum limits */
#define MAX_COLLECTIONS 1024
#undef DEFAULT_MMAP_SIZE
#define DEFAULT_MMAP_SIZE (1024 * 1024 * 1024) /* 1GB */
#define DEFAULT_CACHE_SIZE (1024 * 1024 * 10)  /* 10MB */

/* High-performance collection */
typedef struct {
    char name[256];
    mmap_storage_t* storage;
    hash_index_t* primary_index;
    generic_cache_t* cache;
    pthread_rwlock_t lock;
    
    /* Statistics */
    atomic_uint_fast64_t doc_count;
    atomic_uint_fast64_t total_size;
} hp_collection_t;

/* Global database instance */
static struct {
    char path[1024];
    hp_collection_t* collections[MAX_COLLECTIONS];
    size_t num_collections;
    pthread_rwlock_t lock;
    int initialized;
} g_database = {0};

/* Helper: Create directory if needed */
static int ensure_directory(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }
    return mkdir(path, 0755);
}

/* Helper: Find collection by name */
static hp_collection_t* find_collection(const char* name) {
    for (size_t i = 0; i < g_database.num_collections; i++) {
        if (g_database.collections[i] && 
            strcmp(g_database.collections[i]->name, name) == 0) {
            return g_database.collections[i];
        }
    }
    return NULL;
}

/* Initialize database */
database_t* db_init(const char* path) {
    if (g_database.initialized) {
        LOG_INFO("Database already initialized");
        return (database_t*)&g_database;
    }
    
    strncpy(g_database.path, path, sizeof(g_database.path) - 1);
    
    if (ensure_directory(path) != 0) {
        LOG_ERROR("Failed to create database directory: %s", path);
        return NULL;
    }
    
    pthread_rwlock_init(&g_database.lock, NULL);
    g_database.initialized = 1;
    
    /* Initialize transaction system */
    //transaction_init();
    
    /* Initialize JavaScript engine */
    //js_engine_init();
    
    LOG_INFO("High-performance database initialized at: %s", path);
    return (database_t*)&g_database;
}

/* Close database */
void db_close(database_t* db) {
    UNUSED(db);
    if (!g_database.initialized) return;
    
    pthread_rwlock_wrlock(&g_database.lock);
    
    /* Close all collections */
    for (size_t i = 0; i < g_database.num_collections; i++) {
        if (g_database.collections[i]) {
            hp_collection_t* coll = g_database.collections[i];
            
            /* Sync storage */
            if (coll->storage) {
                mmap_storage_sync(coll->storage);
                mmap_storage_destroy(coll->storage);
            }
            
            /* Destroy index */
            if (coll->primary_index) {
                hash_index_destroy(coll->primary_index);
            }
            
            /* Destroy cache */
            if (coll->cache) {
                generic_cache_destroy(coll->cache);
            }
            
            pthread_rwlock_destroy(&coll->lock);
            free(coll);
        }
    }
    
    g_database.num_collections = 0;
    g_database.initialized = 0;
    
    pthread_rwlock_unlock(&g_database.lock);
    pthread_rwlock_destroy(&g_database.lock);
    
    /* Cleanup subsystems */
    //js_engine_cleanup();
    //transaction_cleanup();
    
    LOG_INFO("Database closed");
}

/* Create collection */
int db_create_collection(database_t* db, const char* name) {
    if (!g_database.initialized || !name) return -1;
    
    pthread_rwlock_wrlock(&g_database.lock);
    
    /* Check if exists */
    if (find_collection(name)) {
        pthread_rwlock_unlock(&g_database.lock);
        return 0; /* Already exists */
    }
    
    /* Check limit */
    if (g_database.num_collections >= MAX_COLLECTIONS) {
        pthread_rwlock_unlock(&g_database.lock);
        LOG_ERROR("Maximum collections reached");
        return -1;
    }
    
    /* Create collection */
    hp_collection_t* coll = calloc(1, sizeof(hp_collection_t));
    if (!coll) {
        pthread_rwlock_unlock(&g_database.lock);
        return -1;
    }
    
    strncpy(coll->name, name, sizeof(coll->name) - 1);
    
    /* Initialize storage */
    char storage_path[1024];
    snprintf(storage_path, sizeof(storage_path), "%s/%s.mmap", g_database.path, name);
    coll->storage = mmap_storage_create(storage_path, DEFAULT_MMAP_SIZE);
    
    if (!coll->storage) {
        free(coll);
        pthread_rwlock_unlock(&g_database.lock);
        LOG_ERROR("Failed to create storage for collection: %s", name);
        return -1;
    }
    
    /* Initialize index */
    char index_path[1024];
    snprintf(index_path, sizeof(index_path), "%s/%s.idx", g_database.path, name);
    coll->primary_index = hash_index_create(index_path, NULL);
    
    if (!coll->primary_index) {
        mmap_storage_destroy(coll->storage);
        free(coll);
        pthread_rwlock_unlock(&g_database.lock);
        LOG_ERROR("Failed to create index for collection: %s", name);
        return -1;
    }
    
    /* Initialize cache */
    coll->cache = generic_cache_create(DEFAULT_CACHE_SIZE);
    
    /* Initialize lock and stats */
    pthread_rwlock_init(&coll->lock, NULL);
    atomic_init(&coll->doc_count, 0);
    atomic_init(&coll->total_size, 0);
    
    /* Add to collections */
    g_database.collections[g_database.num_collections++] = coll;
    
    pthread_rwlock_unlock(&g_database.lock);
    
    LOG_INFO("Created collection: %s", name);
    return 0;
}

/* Get collection */
db_collection_t* db_get_collection(database_t* db, const char* name) {
    if (!g_database.initialized || !name) return NULL;
    
    pthread_rwlock_rdlock(&g_database.lock);
    hp_collection_t* coll = find_collection(name);
    pthread_rwlock_unlock(&g_database.lock);
    
    return (db_collection_t*)coll;
}

/* Check if collection exists */
int db_collection_exists(database_t* db, const char* name) {
    return db_get_collection(db, name) != NULL;
}

/* Generate document ID */
static void generate_doc_id(char* id_buf, size_t buf_size) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    snprintf(id_buf, buf_size, "doc-%ld-%09ld-%04x", 
             ts.tv_sec, ts.tv_nsec, rand() & 0xFFFF);
}

/* Insert document */
json_value_t* db_insert_document(database_t* db, const char* collection_name, 
                                json_value_t* document) {
    if (!g_database.initialized || !collection_name || !document) return NULL;
    
    pthread_rwlock_rdlock(&g_database.lock);
    hp_collection_t* coll = find_collection(collection_name);
    pthread_rwlock_unlock(&g_database.lock);
    
    if (!coll) {
        /* Auto-create collection */
        if (db_create_collection(db, collection_name) != 0) {
            return NULL;
        }
        
        pthread_rwlock_rdlock(&g_database.lock);
        coll = find_collection(collection_name);
        pthread_rwlock_unlock(&g_database.lock);
        
        if (!coll) return NULL;
    }
    
    /* Get or generate ID */
    char doc_id[256];
    json_value_t* id_field = json_object_get(document, "id");
    
    if (!id_field || id_field->type != JSON_STRING) {
        generate_doc_id(doc_id, sizeof(doc_id));
        json_value_t* id_value = json_create_string(doc_id);
        json_object_set(document, "id", id_value);
    } else {
        strncpy(doc_id, id_field->value.string, sizeof(doc_id) - 1);
    }
    
    /* Serialize document */
    char* json_str = json_stringify(document);
    if (!json_str) return NULL;
    
    size_t doc_size = strlen(json_str);
    
    /* Insert with write lock */
    pthread_rwlock_wrlock(&coll->lock);
    
    /* Check if ID already exists */
    uint64_t existing_offset;
    if (hash_index_search(coll->primary_index, doc_id, strlen(doc_id), &existing_offset) == 0) {
        pthread_rwlock_unlock(&coll->lock);
        free(json_str);
        LOG_ERROR("Document already exists: %s", doc_id);
        return NULL;
    }
    
    /* Store in mmap */
    uint64_t offset = coll->storage->header->free_offset;
    if (mmap_storage_put(coll->storage, doc_id, strlen(doc_id), json_str, doc_size) != 0) {
        pthread_rwlock_unlock(&coll->lock);
        free(json_str);
        return NULL;
    }
    
    /* Update index */
    if (hash_index_insert(coll->primary_index, doc_id, strlen(doc_id), offset) != 0) {
        pthread_rwlock_unlock(&coll->lock);
        free(json_str);
        return NULL;
    }
    
    /* Update stats */
    atomic_fetch_add(&coll->doc_count, 1);
    atomic_fetch_add(&coll->total_size, doc_size);
    
    /* Add to cache */
    if (coll->cache) {
        json_value_t* cached_doc = json_deep_copy(document);
        if (cached_doc) {
            generic_cache_put(coll->cache, doc_id, strlen(doc_id), 
                            &cached_doc, sizeof(void*));
        }
    }
    
    pthread_rwlock_unlock(&coll->lock);
    free(json_str);
    
    /* Run JS triggers */
    //js_run_insert_triggers(collection_name, document);
    
    return json_deep_copy(document);
}

/* Get document */
json_value_t* db_get_document(database_t* db, const char* collection_name, 
                             const char* id) {
    if (!g_database.initialized || !collection_name || !id) return NULL;
    
    pthread_rwlock_rdlock(&g_database.lock);
    hp_collection_t* coll = find_collection(collection_name);
    pthread_rwlock_unlock(&g_database.lock);
    
    if (!coll) return NULL;
    
    /* Check cache first */
    if (coll->cache) {
        json_value_t** cached = (json_value_t**)generic_cache_get(coll->cache, id, strlen(id));
        if (cached && *cached) {
            return json_deep_copy(*cached);
        }
    }
    
    /* Read lock for lookup */
    pthread_rwlock_rdlock(&coll->lock);
    
    /* Find in index */
    uint64_t offset;
    if (hash_index_search(coll->primary_index, id, strlen(id), &offset) != 0) {
        pthread_rwlock_unlock(&coll->lock);
        return NULL;
    }
    
    /* Get from storage */
    void* data;
    size_t data_len;
    if (mmap_storage_get(coll->storage, id, strlen(id), &data, &data_len) != 0) {
        pthread_rwlock_unlock(&coll->lock);
        return NULL;
    }
    
    pthread_rwlock_unlock(&coll->lock);
    
    /* Parse JSON */
    json_value_t* doc = json_parse((char*)data);
    free(data);
    
    /* Update cache */
    if (doc && coll->cache) {
        json_value_t* cached_doc = json_deep_copy(doc);
        if (cached_doc) {
            generic_cache_put(coll->cache, id, strlen(id), 
                            &cached_doc, sizeof(void*));
        }
    }
    
    return doc;
}

/* Update document */
json_value_t* db_update_document(database_t* db, const char* collection_name,
                                const char* id, json_value_t* document) {
    if (!g_database.initialized || !collection_name || !id || !document) return NULL;
    
    pthread_rwlock_rdlock(&g_database.lock);
    hp_collection_t* coll = find_collection(collection_name);
    pthread_rwlock_unlock(&g_database.lock);
    
    if (!coll) return NULL;
    
    /* Ensure ID matches */
    json_value_t* id_value = json_create_string(id);
    json_object_set(document, "id", id_value);
    
    /* Serialize */
    char* json_str = json_stringify(document);
    if (!json_str) return NULL;
    
    size_t doc_size = strlen(json_str);
    
    /* Update with write lock */
    pthread_rwlock_wrlock(&coll->lock);
    
    /* Check if exists */
    uint64_t old_offset;
    if (hash_index_search(coll->primary_index, id, strlen(id), &old_offset) != 0) {
        pthread_rwlock_unlock(&coll->lock);
        free(json_str);
        return NULL;
    }
    
    /* Store new version */
    uint64_t new_offset = coll->storage->header->free_offset;
    if (mmap_storage_put(coll->storage, id, strlen(id), json_str, doc_size) != 0) {
        pthread_rwlock_unlock(&coll->lock);
        free(json_str);
        return NULL;
    }
    
    /* Update index */
    if (hash_index_insert(coll->primary_index, id, strlen(id), new_offset) != 0) {
        pthread_rwlock_unlock(&coll->lock);
        free(json_str);
        return NULL;
    }
    
    /* Update stats */
    atomic_fetch_add(&coll->total_size, doc_size);
    
    /* Invalidate cache */
    if (coll->cache) {
        generic_cache_remove(coll->cache, id, strlen(id));
    }
    
    pthread_rwlock_unlock(&coll->lock);
    free(json_str);
    
    /* Run JS triggers */
    //js_run_update_triggers(collection_name, document);
    
    return json_deep_copy(document);
}

/* Delete document */
int db_delete_document(database_t* db, const char* collection_name, const char* id) {
    if (!g_database.initialized || !collection_name || !id) return -1;
    
    pthread_rwlock_rdlock(&g_database.lock);
    hp_collection_t* coll = find_collection(collection_name);
    pthread_rwlock_unlock(&g_database.lock);
    
    if (!coll) return -1;
    
    pthread_rwlock_wrlock(&coll->lock);
    
    /* Remove from index */
    int result = hash_index_delete(coll->primary_index, id, strlen(id));
    
    if (result == 0) {
        /* Update stats */
        atomic_fetch_sub(&coll->doc_count, 1);
        
        /* Remove from cache */
        if (coll->cache) {
            generic_cache_remove(coll->cache, id, strlen(id));
        }
        
        /* Run JS triggers */
        //js_run_delete_triggers(collection_name, id);
    }
    
    pthread_rwlock_unlock(&coll->lock);
    
    return result;
}

/* Query documents (simple version) */
json_value_t* db_query_documents(database_t* db, const char* collection_name,
                                json_value_t* query) {
    if (!g_database.initialized || !collection_name) return NULL;
    
    pthread_rwlock_rdlock(&g_database.lock);
    hp_collection_t* coll = find_collection(collection_name);
    pthread_rwlock_unlock(&g_database.lock);
    
    if (!coll) return NULL;
    
    /* Create result array */
    json_value_t* results = json_create_array();
    if (!results) return NULL;
    
    /* For now, return empty array (TODO: implement query) */
    pthread_rwlock_rdlock(&coll->lock);
    
    /* Simple implementation - just return empty for now */
    // TODO: Implement proper query functionality
    
    pthread_rwlock_unlock(&coll->lock);
    
    return results;
}

/* List collections */
json_value_t* db_list_collections(database_t* db) {
    if (!g_database.initialized) return NULL;
    
    json_value_t* array = json_create_array();
    if (!array) return NULL;
    
    pthread_rwlock_rdlock(&g_database.lock);
    
    for (size_t i = 0; i < g_database.num_collections; i++) {
        if (g_database.collections[i]) {
            json_value_t* name = json_create_string(g_database.collections[i]->name);
            if (name) {
                json_array_append(array, name);
            }
        }
    }
    
    pthread_rwlock_unlock(&g_database.lock);
    
    return array;
}

/* Drop collection */
int db_drop_collection(database_t* db, const char* name) {
    if (!g_database.initialized || !name) return -1;
    
    pthread_rwlock_wrlock(&g_database.lock);
    
    /* Find and remove collection */
    for (size_t i = 0; i < g_database.num_collections; i++) {
        if (g_database.collections[i] && 
            strcmp(g_database.collections[i]->name, name) == 0) {
            
            hp_collection_t* coll = g_database.collections[i];
            
            /* Clean up resources */
            if (coll->storage) {
                mmap_storage_destroy(coll->storage);
            }
            if (coll->primary_index) {
                hash_index_destroy(coll->primary_index);
            }
            if (coll->cache) {
                generic_cache_destroy(coll->cache);
            }
            pthread_rwlock_destroy(&coll->lock);
            
            /* Remove files */
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s.mmap", g_database.path, name);
            unlink(path);
            snprintf(path, sizeof(path), "%s/%s.idx", g_database.path, name);
            unlink(path);
            
            free(coll);
            
            /* Shift remaining collections */
            for (size_t j = i; j < g_database.num_collections - 1; j++) {
                g_database.collections[j] = g_database.collections[j + 1];
            }
            g_database.num_collections--;
            
            pthread_rwlock_unlock(&g_database.lock);
            return 0;
        }
    }
    
    pthread_rwlock_unlock(&g_database.lock);
    return -1;
}

/* Persistence functions (no-op for mmap) */
int db_save(database_t* db) {
    if (!g_database.initialized) return -1;
    
    pthread_rwlock_rdlock(&g_database.lock);
    
    for (size_t i = 0; i < g_database.num_collections; i++) {
        if (g_database.collections[i] && g_database.collections[i]->storage) {
            mmap_storage_sync(g_database.collections[i]->storage);
        }
    }
    
    pthread_rwlock_unlock(&g_database.lock);
    return 0;
}

int db_load(database_t* db) {
    /* Collections are loaded on-demand */
    return 0;
}

/* Bootstrap functions */
int db_needs_bootstrap(database_t* db) {
    return db_collection_exists(db, "_system_config") ? 0 : 1;
}

int db_init_system_schemas(database_t* db) {
    UNUSED(db);
    /* System schemas are automatically created on-demand in high-performance mode */
    return 1; /* Return 1 for success */
}

/* Cache management */
int db_enable_cache(database_t* db, int capacity, int ttl) {
    /* Cache is always enabled in high-performance mode */
    return 0;
}

int db_disable_cache(database_t* db) {
    /* Cannot disable cache in high-performance mode */
    return -1;
}

int db_clear_cache(database_t* db) {
    pthread_rwlock_rdlock(&g_database.lock);
    
    for (size_t i = 0; i < g_database.num_collections; i++) {
        if (g_database.collections[i] && g_database.collections[i]->cache) {
            generic_cache_clear(g_database.collections[i]->cache);
        }
    }
    
    pthread_rwlock_unlock(&g_database.lock);
    return 0;
}

/* Additional API functions */
json_value_t* db_list_collections_with_info(database_t* db) {
    if (!g_database.initialized) return NULL;
    
    json_value_t* array = json_create_array();
    if (!array) return NULL;
    
    pthread_rwlock_rdlock(&g_database.lock);
    
    for (size_t i = 0; i < g_database.num_collections; i++) {
        if (g_database.collections[i]) {
            hp_collection_t* coll = g_database.collections[i];
            
            json_value_t* info = json_create_object();
            if (info) {
                json_value_t* name = json_create_string(coll->name);
                json_value_t* count = json_create_number((double)atomic_load(&coll->doc_count));
                json_value_t* size = json_create_number((double)atomic_load(&coll->total_size));
                
                if (name && count && size) {
                    json_object_set(info, "name", name);
                    json_object_set(info, "document_count", count);
                    json_object_set(info, "total_size", size);
                    json_array_append(array, info);
                }
            }
        }
    }
    
    pthread_rwlock_unlock(&g_database.lock);
    return array;
}

json_value_t* db_get_cache_stats(database_t* db) {
    if (!g_database.initialized) return NULL;
    
    json_value_t* stats = json_create_object();
    if (!stats) return NULL;
    
    uint64_t total_hits = 0, total_misses = 0, total_evictions = 0;
    
    pthread_rwlock_rdlock(&g_database.lock);
    
    for (size_t i = 0; i < g_database.num_collections; i++) {
        if (g_database.collections[i] && g_database.collections[i]->cache) {
            uint64_t hits, misses, evictions;
            generic_cache_stats(g_database.collections[i]->cache, &hits, &misses, &evictions);
            total_hits += hits;
            total_misses += misses;
            total_evictions += evictions;
        }
    }
    
    pthread_rwlock_unlock(&g_database.lock);
    
    json_value_t* hits = json_create_number((double)total_hits);
    json_value_t* misses = json_create_number((double)total_misses);
    json_value_t* evictions = json_create_number((double)total_evictions);
    
    if (hits && misses && evictions) {
        json_object_set(stats, "hits", hits);
        json_object_set(stats, "misses", misses);
        json_object_set(stats, "evictions", evictions);
    }
    
    return stats;
}

int db_configure_cache(database_t* db, int capacity, int ttl, const char* type, double max_memory_mb) {
    UNUSED(db);
    UNUSED(capacity);
    UNUSED(ttl);
    UNUSED(type);
    UNUSED(max_memory_mb);
    /* Cache is always enabled in high-performance mode */
    return 0;
}

/* Stub functions for compatibility */
int db_start_persistence_thread(database_t* db) { UNUSED(db); return 0; }
void db_stop_persistence_thread(database_t* db) { UNUSED(db); }
void db_notify_data_change(database_t* db, size_t size) { UNUSED(db); UNUSED(size); }
int db_notify_data_change_sync(database_t* db, size_t size) { UNUSED(db); UNUSED(size); return 0; }
int db_rebuild_indices(database_t* db) { UNUSED(db); return 0; }

/* Additional stub functions */
void db_complete_bootstrap(database_t* db) { UNUSED(db); }
int process_cache_invalidations(database_t* db) { UNUSED(db); return 0; }