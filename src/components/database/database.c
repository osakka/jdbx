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
#include <stdbool.h>
#include <dirent.h>

/* Suppress unused parameter warnings */
#define UNUSED(x) ((void)(x))

#include "database/database.h"
#include "storage/mmap_storage.h"
#include "index/hash_index.h"
#include "index/btree_disk.h"
#include "utils/json_helpers.h"
#include "utils/logger.h"
#include "utils/generic_cache.h"
#include "utils/buffer_pool.h"
#include "utils/production_config.h"
#include "utils/metrics.h"
//#include "database/js_integration.h"
//#include "transaction/transaction.h"

/* Maximum limits */
#define MAX_COLLECTIONS 1024
#define MAX_INDEXES_PER_COLLECTION 16
#undef DEFAULT_MMAP_SIZE
#undef DEFAULT_CACHE_SIZE

/* Production-ready sizes for billion-document scale */
#ifdef PRODUCTION_MODE
#define DEFAULT_MMAP_SIZE (1024ULL * 1024 * 1024 * 10)  /* 10GB per collection */
#define DEFAULT_CACHE_SIZE (1024ULL * 1024 * 1024 * 1)  /* 1GB cache */
#else
#define DEFAULT_MMAP_SIZE (1024 * 1024 * 256)   /* 256MB per collection (dev) */
#define DEFAULT_CACHE_SIZE (1024 * 1024 * 50)   /* 50MB cache (dev) */
#endif

/* Secondary index structure */
typedef struct {
    char field_name[128];
    btree_disk_t* btree;
    bool active;
} secondary_index_t;

/* High-performance collection */
typedef struct {
    char name[256];
    mmap_storage_t* storage;
    hash_index_t* primary_index;
    
    /* Secondary indexes */
    secondary_index_t indexes[MAX_INDEXES_PER_COLLECTION];
    int num_indexes;
    
    generic_cache_t* cache;
    pthread_rwlock_t lock;
    
    /* Statistics */
    atomic_uint_fast64_t doc_count;
    atomic_uint_fast64_t total_size;
} hp_collection_t;

/* Forward declarations */
static json_value_t* extract_field_value(json_value_t* doc, const char* field);
static int rebuild_collection_indices(hp_collection_t* coll);

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

/* Internal function for batch operations - exposed for batch_operations.c */
hp_collection_t* find_collection_internal(const char* name) {
    if (!g_database.initialized) return NULL;
    pthread_rwlock_rdlock(&g_database.lock);
    hp_collection_t* coll = find_collection(name);
    pthread_rwlock_unlock(&g_database.lock);
    return coll;
}

/* Load existing collections from disk */
static void load_existing_collections(const char* path) {
    DIR* dir = opendir(path);
    if (!dir) {
        LOG_WARNING("Failed to open database directory: %s", path);
        return;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Look for .mmap files */
        char* ext = strrchr(entry->d_name, '.');
        if (ext && strcmp(ext, ".mmap") == 0) {
            /* Extract collection name */
            char coll_name[256];
            size_t name_len = ext - entry->d_name;
            if (name_len >= sizeof(coll_name)) continue;
            
            strncpy(coll_name, entry->d_name, name_len);
            coll_name[name_len] = '\0';
            
            LOG_INFO("Loading existing collection: %s", coll_name);
            
            /* Create/load the collection */
            if (db_create_collection((database_t*)&g_database, coll_name) == 0) {
                /* Rebuild indices for the loaded collection */
                pthread_rwlock_rdlock(&g_database.lock);
                hp_collection_t* coll = find_collection(coll_name);
                pthread_rwlock_unlock(&g_database.lock);
                
                if (coll) {
                    rebuild_collection_indices(coll);
                }
            }
        }
    }
    
    closedir(dir);
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
    
    /* Initialize random seed for UUID generation */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    srand((unsigned int)(ts.tv_sec ^ ts.tv_nsec ^ getpid()));
    
    /* Load existing collections from disk */
    if (path) {
        load_existing_collections(path);
    }
    
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
    
    /* Initialize storage with production configuration */
    const production_config_t* prod_config = production_config_get();
    char storage_path[1024];
    snprintf(storage_path, sizeof(storage_path), "%s/%s.mmap", g_database.path, name);
    
    LOG_DEBUG("Creating collection '%s' with MMAP size: %s", name, 
              format_bytes(prod_config->mmap_size_per_collection));
    
    coll->storage = mmap_storage_create(storage_path, prod_config->mmap_size_per_collection);
    
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
    
    /* Initialize cache with production configuration */
    if (prod_config->cache_size_total > 0) {
        /* Divide cache equally among collections (simplified approach) */
        size_t cache_per_collection = prod_config->cache_size_total / MAX_COLLECTIONS;
        coll->cache = generic_cache_create(cache_per_collection);
        if (coll->cache) {
            LOG_DEBUG("Cache enabled for collection '%s' with size: %s", 
                     name, format_bytes(cache_per_collection));
        }
    } else {
        coll->cache = NULL;
    }
    
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

/* Generate UUID v4 document ID */
static void generate_doc_id(char* id_buf, size_t buf_size) {
    /* Generate 16 bytes of random data for UUID v4 */
    unsigned char uuid_bytes[16];
    for (int i = 0; i < 16; i++) {
        uuid_bytes[i] = rand() & 0xFF;
    }
    
    /* Set version (4) and variant bits according to RFC 4122 */
    uuid_bytes[6] = (uuid_bytes[6] & 0x0F) | 0x40; /* Version 4 */
    uuid_bytes[8] = (uuid_bytes[8] & 0x3F) | 0x80; /* Variant bits */
    
    /* Format as standard UUID string: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx */
    snprintf(id_buf, buf_size, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             uuid_bytes[0], uuid_bytes[1], uuid_bytes[2], uuid_bytes[3],
             uuid_bytes[4], uuid_bytes[5], uuid_bytes[6], uuid_bytes[7],
             uuid_bytes[8], uuid_bytes[9], uuid_bytes[10], uuid_bytes[11],
             uuid_bytes[12], uuid_bytes[13], uuid_bytes[14], uuid_bytes[15]);
}

/* Insert document */
json_value_t* db_insert_document(database_t* db, const char* collection_name, 
                                json_value_t* document) {
    LOG_TRACE("db_insert_document: collection=%s, doc=%p", collection_name, document);
    if (!g_database.initialized || !collection_name || !document) {
        LOG_ERROR("db_insert_document: invalid params - init=%d, coll=%s, doc=%p", 
                  g_database.initialized, collection_name, document);
        return NULL;
    }
    
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
        
        if (!coll) {
            return NULL;
        }
    }
    
    /* Get or generate ID - check uuid field */
    char doc_id[256];
    json_value_t* id_field = json_object_get(document, "uuid");
    
    if (!id_field || id_field->type != JSON_STRING) {
        generate_doc_id(doc_id, sizeof(doc_id));
        json_value_t* id_value = json_create_string(doc_id);
        json_object_set(document, "uuid", id_value);
    } else {
        strncpy(doc_id, id_field->value.string, sizeof(doc_id) - 1);
        /* Ensure document has uuid field */
        if (!json_object_get(document, "uuid")) {
            json_object_set(document, "uuid", json_create_string(doc_id));
        }
    }
    
    /* Add timestamps for new documents */
    if (!json_object_get(document, "created_at")) {
        time_t now = time(NULL);
        char timestamp[64];
        struct tm* utc_tm = gmtime(&now);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", utc_tm);
        json_object_set(document, "created_at", json_create_string(timestamp));
    }
    if (!json_object_get(document, "updated_at")) {
        time_t now = time(NULL);
        char timestamp[64];
        struct tm* utc_tm = gmtime(&now);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", utc_tm);
        json_object_set(document, "updated_at", json_create_string(timestamp));
    }
    
    /* Clone document after adding ID to return to caller */
    json_value_t* result_doc = json_clone(document);
    if (!result_doc) {
        LOG_ERROR("db_insert_document: failed to clone document");
        return NULL;
    }
    
    /* Serialize document */
    char* json_str = json_stringify(document);
    if (!json_str) {
        LOG_ERROR("Failed to serialize document");
        json_free(result_doc);
        return NULL;
    }
    
    size_t doc_size = strlen(json_str);
    
    /* Insert with write lock */
    pthread_rwlock_wrlock(&coll->lock);
    
    /* Check if ID already exists */
    uint64_t existing_offset;
    if (hash_index_search(coll->primary_index, doc_id, strlen(doc_id), &existing_offset) == 0) {
        pthread_rwlock_unlock(&coll->lock);
        buffer_pool_free_safe(json_str);
        json_free(result_doc);
        LOG_ERROR("Document already exists: %s", doc_id);
        return NULL;
    }
    
    /* Store in mmap */
    /* Calculate the offset where the document will be stored */
    size_t entry_size = sizeof(doc_entry_t) + strlen(doc_id) + doc_size;
    uint64_t offset = coll->storage->header->free_offset;
    
    LOG_TRACE("db_insert_document: storing doc_id=%s at offset=%lu, size=%zu", 
              doc_id, offset, doc_size);
    if (mmap_storage_put(coll->storage, doc_id, strlen(doc_id), json_str, doc_size) != 0) {
        LOG_ERROR("db_insert_document: mmap_storage_put failed for doc_id=%s", doc_id);
        pthread_rwlock_unlock(&coll->lock);
        buffer_pool_free_safe(json_str);
        json_free(result_doc);
        return NULL;
    }
    
    /* Update primary index with the actual storage offset */
    LOG_TRACE("db_insert_document: updating primary index for doc_id=%s at offset=%lu", 
              doc_id, offset);
    if (hash_index_insert(coll->primary_index, doc_id, strlen(doc_id), offset) != 0) {
        LOG_ERROR("db_insert_document: hash_index_insert failed for doc_id=%s", doc_id);
        pthread_rwlock_unlock(&coll->lock);
        buffer_pool_free_safe(json_str);
        return NULL;
    }
    LOG_TRACE("db_insert_document: primary index updated successfully");
    
    /* Update secondary indexes */
    for (int i = 0; i < coll->num_indexes; i++) {
        if (!coll->indexes[i].active) continue;
        
        /* Extract field value */
        json_value_t* field_val = extract_field_value(document, coll->indexes[i].field_name);
        if (!field_val) continue;
        
        /* Convert field value to string for indexing */
        char field_str[256];
        if (field_val->type == JSON_STRING) {
            strncpy(field_str, field_val->value.string, sizeof(field_str) - 1);
        } else if (field_val->type == JSON_NUMBER) {
            snprintf(field_str, sizeof(field_str), "%.15g", field_val->value.number);
        } else if (field_val->type == JSON_INTEGER) {
            snprintf(field_str, sizeof(field_str), "%ld", (long)field_val->value.integer);
        } else {
            continue; /* Skip non-indexable types */
        }
        
        /* Insert into B+tree index */
        if (btree_disk_insert(coll->indexes[i].btree, field_str, strlen(field_str), offset) != 0) {
            LOG_WARNING("Failed to update secondary index %s for doc %s", 
                       coll->indexes[i].field_name, doc_id);
        }
    }
    LOG_TRACE("db_insert_document: about to skip stats update");
    
    /* Update stats */
    atomic_fetch_add(&coll->doc_count, 1);
    atomic_fetch_add(&coll->total_size, doc_size);
    LOG_TRACE("db_insert_document: stats updated - count=%lu, size=%lu", 
              (unsigned long)atomic_load(&coll->doc_count),
              (unsigned long)atomic_load(&coll->total_size));
    // For now, skip stats to get admin login working
    
    /* Add to cache */
    LOG_TRACE("db_insert_document: checking cache - cache pointer: %p", coll->cache);
    if (coll->cache) {
        LOG_TRACE("db_insert_document: adding to cache");
        json_value_t* cached_doc = json_clone(document);
        if (cached_doc) {
            generic_cache_put(coll->cache, doc_id, strlen(doc_id), 
                            &cached_doc, sizeof(void*));
        }
        LOG_TRACE("db_insert_document: cache updated");
    }
    LOG_TRACE("db_insert_document: cache operations completed");
    
    LOG_TRACE("db_insert_document: unlocking collection");
    pthread_rwlock_unlock(&coll->lock);
    LOG_TRACE("db_insert_document: about to free json_str=%p", json_str);
    /* Free json_str using buffer pool aware function */
    if (json_str) {
        buffer_pool_free_safe(json_str);
        LOG_TRACE("db_insert_document: json_str freed successfully");
    } else {
        LOG_ERROR("db_insert_document: json_str is NULL, not freeing");
    }
    
    /* Run JS triggers */
    //js_run_insert_triggers(collection_name, document);
    
    /* Increment metrics */
    metric_t* write_ops = get_db_write_operations_metric();
    if (write_ops) {
        metrics_counter_inc(write_ops, 1);
    }
    metric_t* db_ops = get_db_operations_metric();
    if (db_ops) {
        metrics_counter_inc(db_ops, 1);
    }
    
    /* Add the actual document ID to the result */
    json_object_set(result_doc, "uuid", json_create_string(doc_id));
    
    /* Return the cloned document */
    LOG_TRACE("db_insert_document: returning cloned document with _id: %s", doc_id);
    LOG_TRACE("db_insert_document: exiting function");
    return result_doc;
}

/* Get document */
json_value_t* db_get_document(database_t* db, const char* collection_name, 
                             const char* id) {
    LOG_DEBUG("db_get_document: collection=%s, id=%s", collection_name, id);
    if (!g_database.initialized || !collection_name || !id) {
        LOG_ERROR("db_get_document: invalid params - init=%d, coll=%s, id=%s",
                  g_database.initialized, collection_name, id);
        return NULL;
    }
    
    pthread_rwlock_rdlock(&g_database.lock);
    hp_collection_t* coll = find_collection(collection_name);
    pthread_rwlock_unlock(&g_database.lock);
    
    if (!coll) {
        LOG_ERROR("db_get_document: collection %s not found", collection_name);
        return NULL;
    }
    
    /* Check cache first */
    if (coll->cache) {
        json_value_t** cached = (json_value_t**)generic_cache_get(coll->cache, id, strlen(id));
        if (cached && *cached) {
            return json_clone(*cached);
        }
    }
    
    /* Read lock for lookup */
    pthread_rwlock_rdlock(&coll->lock);
    
    /* Find in index */
    uint64_t offset;
    int search_result = hash_index_search(coll->primary_index, id, strlen(id), &offset);
    LOG_DEBUG("db_get_document: hash_index_search returned %d, offset=%lu", search_result, offset);
    if (search_result != 0) {
        pthread_rwlock_unlock(&coll->lock);
        LOG_ERROR("db_get_document: document %s not found in index", id);
        return NULL;
    }
    
    /* Get from storage using offset */
    void* data;
    size_t data_len;
    if (mmap_storage_get_by_offset(coll->storage, offset, NULL, NULL, &data, &data_len) != 0) {
        pthread_rwlock_unlock(&coll->lock);
        return NULL;
    }
    
    pthread_rwlock_unlock(&coll->lock);
    
    /* Parse JSON */
    json_value_t* doc = json_parse((char*)data);
    free(data);
    
    /* Update cache */
    if (doc && coll->cache) {
        json_value_t* cached_doc = json_clone(doc);
        if (cached_doc) {
            generic_cache_put(coll->cache, id, strlen(id), 
                            &cached_doc, sizeof(void*));
        }
    }
    
    /* Increment metrics */
    if (doc) {
        metric_t* read_ops = get_db_read_operations_metric();
        if (read_ops) {
            metrics_counter_inc(read_ops, 1);
        }
        metric_t* db_ops = get_db_operations_metric();
        if (db_ops) {
            metrics_counter_inc(db_ops, 1);
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
    
    /* Ensure ID matches - use uuid field */
    json_value_t* id_value = json_create_string(id);
    json_object_set(document, "uuid", id_value);
    
    /* Update timestamp for document updates */
    time_t now = time(NULL);
    char timestamp[64];
    struct tm* utc_tm = gmtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", utc_tm);
    json_object_set(document, "updated_at", json_create_string(timestamp));
    
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
        buffer_pool_free_safe(json_str);
        return NULL;
    }
    
    /* Mark old document as deleted */
    char* base = (char*)coll->storage->base_addr;
    doc_entry_t* old_entry = (doc_entry_t*)(base + old_offset);
    old_entry->flags |= DOC_FLAG_DELETED;
    
    /* Store new version */
    uint64_t new_offset = coll->storage->header->free_offset;
    if (mmap_storage_put(coll->storage, id, strlen(id), json_str, doc_size) != 0) {
        pthread_rwlock_unlock(&coll->lock);
        buffer_pool_free_safe(json_str);
        return NULL;
    }
    
    /* Update index - first remove old entry, then insert new */
    hash_index_delete(coll->primary_index, id, strlen(id));
    if (hash_index_insert(coll->primary_index, id, strlen(id), new_offset) != 0) {
        pthread_rwlock_unlock(&coll->lock);
        buffer_pool_free_safe(json_str);
        return NULL;
    }
    
    /* Update stats - disabled for now */
    // atomic_fetch_add(&coll->total_size, doc_size);
    
    /* Invalidate cache */
    if (coll->cache) {
        generic_cache_remove(coll->cache, id, strlen(id));
    }
    
    pthread_rwlock_unlock(&coll->lock);
    buffer_pool_free_safe(json_str);
    
    /* Run JS triggers */
    //js_run_update_triggers(collection_name, document);
    
    /* Increment metrics */
    metric_t* write_ops = get_db_write_operations_metric();
    if (write_ops) {
        metrics_counter_inc(write_ops, 1);
    }
    metric_t* db_ops = get_db_operations_metric();
    if (db_ops) {
        metrics_counter_inc(db_ops, 1);
    }
    
    /* Return success indicator */
    json_value_t* result = json_create_object();
    if (result) {
        json_object_set(result, "uuid", json_create_string(id));
        json_object_set(result, "status", json_create_string("updated"));
    }
    return result;
}

/* Delete document */
int db_delete_document(database_t* db, const char* collection_name, const char* id) {
    if (!g_database.initialized || !collection_name || !id) return -1;
    
    pthread_rwlock_rdlock(&g_database.lock);
    hp_collection_t* coll = find_collection(collection_name);
    pthread_rwlock_unlock(&g_database.lock);
    
    if (!coll) return -1;
    
    pthread_rwlock_wrlock(&coll->lock);
    
    /* First, get the document to extract field values for index removal */
    uint64_t offset;
    json_value_t* doc = NULL;
    if (hash_index_search(coll->primary_index, id, strlen(id), &offset) == 0) {
        /* Load document from storage */
        void* data = NULL;
        size_t value_size;
        if (mmap_storage_get_by_offset(coll->storage, offset, NULL, NULL, &data, &value_size) == 0 && data) {
            doc = json_parse((char*)data);
            free(data);  /* mmap_storage_get_by_offset allocates memory */
        }
        
        /* Mark document as deleted in storage */
        char* base = (char*)coll->storage->base_addr;
        doc_entry_t* entry = (doc_entry_t*)(base + offset);
        entry->flags |= DOC_FLAG_DELETED;
    }
    
    /* Remove from primary index */
    int result = hash_index_delete(coll->primary_index, id, strlen(id));
    
    if (result == 0) {
        /* Remove from secondary indexes */
        if (doc) {
            for (int i = 0; i < coll->num_indexes; i++) {
                if (!coll->indexes[i].active) continue;
                
                /* Extract field value */
                json_value_t* field_val = extract_field_value(doc, coll->indexes[i].field_name);
                if (!field_val) continue;
                
                /* Convert field value to string */
                char field_str[256];
                if (field_val->type == JSON_STRING) {
                    strncpy(field_str, field_val->value.string, sizeof(field_str) - 1);
                } else if (field_val->type == JSON_NUMBER) {
                    snprintf(field_str, sizeof(field_str), "%.15g", field_val->value.number);
                } else if (field_val->type == JSON_INTEGER) {
                    snprintf(field_str, sizeof(field_str), "%ld", (long)field_val->value.integer);
                } else {
                    continue;
                }
                
                /* Remove from B+tree - note: we need to implement btree_disk_delete */
                // btree_disk_delete(coll->indexes[i].btree, field_str, strlen(field_str));
            }
        }
        
        /* Update stats */
        atomic_fetch_sub(&coll->doc_count, 1);
        /* Note: We can't accurately update total_size without knowing the document size */
        
        /* Remove from cache */
        if (coll->cache) {
            generic_cache_remove(coll->cache, id, strlen(id));
        }
        
        /* Run JS triggers */
        //js_run_delete_triggers(collection_name, id);
    }
    
    if (doc) json_free(doc);
    
    pthread_rwlock_unlock(&coll->lock);
    
    /* Increment metrics on success */
    if (result == 0) {
        metric_t* write_ops = get_db_write_operations_metric();
        if (write_ops) {
            metrics_counter_inc(write_ops, 1);
        }
        metric_t* db_ops = get_db_operations_metric();
        if (db_ops) {
            metrics_counter_inc(db_ops, 1);
        }
    }
    
    return result;
}

/* Forward declaration for optimized query */
json_value_t* db_query_with_index(hp_collection_t* coll, json_value_t* query);

/* Query documents (simple version) */
json_value_t* db_query_documents(database_t* db, const char* collection_name,
                                json_value_t* query) {
    if (!g_database.initialized || !collection_name) return NULL;
    
    pthread_rwlock_rdlock(&g_database.lock);
    hp_collection_t* coll = find_collection(collection_name);
    pthread_rwlock_unlock(&g_database.lock);
    
    if (!coll) return NULL;
    
    /* Try to use index-optimized query first */
    if (query && query->type == JSON_OBJECT && json_object_size(query) > 0) {
        json_value_t* index_results = db_query_with_index(coll, query);
        if (index_results) {
            return index_results;
        }
    }
    
    /* Create result object with documents array */
    json_value_t* results = json_create_object();
    if (!results) return NULL;
    
    json_value_t* documents = json_create_array();
    if (!documents) {
        json_free(results);
        return NULL;
    }
    
    /* Track seen document IDs to prevent duplicates */
    typedef struct seen_id {
        char id[64];
        struct seen_id* next;
    } seen_id_t;
    seen_id_t* seen_ids = NULL;
    
    /* For now, implement a simple full scan through mmap storage */
    pthread_rwlock_rdlock(&coll->lock);
    
    /* Use mmap_storage_iterate to go through all documents */
    storage_header_t* header = coll->storage->header;
    char* base = (char*)coll->storage->base_addr;
    uint64_t offset = header->data_offset;
    
    LOG_TRACE("db_query_documents: Starting scan from offset %lu to %lu", 
              offset, header->free_offset);
    
    int doc_num = 0;
    while (offset < header->free_offset) {
        /* Check if we have enough space for a doc_entry header */
        if (offset + sizeof(doc_entry_t) > header->free_offset) {
            break;
        }
        
        doc_entry_t* entry = (doc_entry_t*)(base + offset);
        
        /* Validate magic number */
        if (entry->magic != 0x444F4355) { /* "DOCU" */
            LOG_TRACE("db_query_documents: Invalid magic at offset %lu: 0x%x", 
                      offset, entry->magic);
            offset += 4; /* Try next aligned position */
            continue;
        }
        
        /* Skip deleted documents */
        if (entry->flags & DOC_FLAG_DELETED) {
            uint64_t entry_size = sizeof(doc_entry_t) + entry->key_len + entry->value_len;
            offset += entry_size;
            offset = (offset + 7) & ~7; /* Align to 8 bytes */
            continue;
        }
        
        doc_num++;
        
        /* Validate entry sizes */
        if (entry->key_len == 0 || entry->value_len == 0 || 
            entry->key_len > 1024 || entry->value_len > 1024*1024) {
            LOG_TRACE("db_query_documents: Invalid entry sizes at offset %lu: key_len=%u, value_len=%u", 
                      offset, entry->key_len, entry->value_len);
            offset += sizeof(doc_entry_t);
            continue;
        }
        
        /* Make sure we don't read past the end */
        uint64_t entry_size = sizeof(doc_entry_t) + entry->key_len + entry->value_len;
        if (offset + entry_size > header->free_offset) {
            LOG_TRACE("db_query_documents: Entry at offset %lu would exceed bounds", offset);
            break;
        }
        
        /* Extract the document key (ID) */
        char* key = (char*)entry + sizeof(doc_entry_t);
        char doc_id[256];
        size_t key_copy_len = entry->key_len < sizeof(doc_id) - 1 ? entry->key_len : sizeof(doc_id) - 1;
        memcpy(doc_id, key, key_copy_len);
        doc_id[key_copy_len] = '\0';
        
        /* Parse the JSON document */
        char* json_data = (char*)entry + sizeof(doc_entry_t) + entry->key_len;
        json_value_t* doc = json_parse(json_data);
        
        if (doc && doc->type == JSON_OBJECT) {
            /* Check if document matches query */
            int matches = 1;
            if (query && query->type == JSON_OBJECT) {
                /* Simple field matching */
                for (size_t i = 0; i < query->value.object.size; i++) {
                    json_object_entry_t* query_field = &query->value.object.entries[i];
                    json_value_t* doc_value = json_object_get(doc, query_field->key);
                    
                    if (!doc_value || !json_equals(doc_value, query_field->value)) {
                        matches = 0;
                        break;
                    }
                }
            }
            
            if (matches) {
                json_value_t* result_doc = json_clone(doc);
                if (result_doc) {
                    /* Add the ID to the document if it doesn't have one */
                    if (!json_object_get(result_doc, "uuid") && !json_object_get(result_doc, "uuid")) {
                        json_object_set(result_doc, "uuid", json_create_string(doc_id));
                    }
                    json_array_append(documents, result_doc);
                }
            }
        }
        
        if (doc) json_free(doc);
        
        /* Move to next entry */
        offset += entry_size;
        /* Align to 8 bytes */
        offset = (offset + 7) & ~7;
    }
    
    pthread_rwlock_unlock(&coll->lock);
    
    /* Increment read metrics */
    metric_t* read_ops = get_db_read_operations_metric();
    if (read_ops) {
        metrics_counter_inc(read_ops, 1);
    }
    metric_t* db_ops = get_db_operations_metric();
    if (db_ops) {
        metrics_counter_inc(db_ops, 1);
    }
    
    json_object_set(results, "documents", documents);
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

/* Rebuild indices for a collection from storage */
static int rebuild_collection_indices(hp_collection_t* coll) {
    if (!coll || !coll->storage || !coll->primary_index) return -1;
    
    LOG_INFO("Rebuilding indices for collection: %s", coll->name);
    
    /* Scan through storage and rebuild primary index */
    storage_header_t* header = coll->storage->header;
    char* base = (char*)coll->storage->base_addr;
    uint64_t offset = header->data_offset;
    int doc_count = 0;
    int indexed_count = 0;
    
    while (offset < header->free_offset) {
        /* Check if we have enough space for a doc_entry header */
        if (offset + sizeof(doc_entry_t) > header->free_offset) {
            break;
        }
        
        doc_entry_t* entry = (doc_entry_t*)(base + offset);
        
        /* Validate magic number */
        if (entry->magic != 0x444F4355) { /* "DOCU" */
            offset += 4; /* Try next aligned position */
            continue;
        }
        
        /* Skip deleted documents */
        if (entry->flags & DOC_FLAG_DELETED) {
            uint64_t entry_size = sizeof(doc_entry_t) + entry->key_len + entry->value_len;
            offset += entry_size;
            offset = (offset + 7) & ~7; /* Align to 8 bytes */
            continue;
        }
        
        doc_count++;
        
        /* Validate entry sizes */
        if (entry->key_len == 0 || entry->value_len == 0 || 
            entry->key_len > 1024 || entry->value_len > 1024*1024) {
            offset += sizeof(doc_entry_t);
            continue;
        }
        
        /* Make sure we don't read past the end */
        uint64_t entry_size = sizeof(doc_entry_t) + entry->key_len + entry->value_len;
        if (offset + entry_size > header->free_offset) {
            break;
        }
        
        /* Extract key (document ID) */
        char* key = (char*)entry + sizeof(doc_entry_t);
        
        /* Add to primary index */
        if (hash_index_insert(coll->primary_index, key, entry->key_len, offset) == 0) {
            indexed_count++;
        } else {
            LOG_WARNING("Failed to index document at offset %lu", offset);
        }
        
        /* Move to next entry */
        offset += entry_size;
        offset = (offset + 7) & ~7; /* Align to 8 bytes */
    }
    
    LOG_INFO("Rebuilt indices for collection %s: found %d documents, indexed %d", 
             coll->name, doc_count, indexed_count);
    
    /* Update collection stats */
    atomic_store(&coll->doc_count, indexed_count);
    
    return 0;
}

/* Rebuild all indices */
int db_rebuild_indices(database_t* db) {
    if (!g_database.initialized) return -1;
    
    LOG_INFO("Rebuilding all database indices...");
    
    pthread_rwlock_wrlock(&g_database.lock);
    
    for (size_t i = 0; i < g_database.num_collections; i++) {
        if (g_database.collections[i]) {
            rebuild_collection_indices(g_database.collections[i]);
        }
    }
    
    pthread_rwlock_unlock(&g_database.lock);
    
    LOG_INFO("Database indices rebuilt successfully");
    return 0;
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
    /* System schemas are automatically created on-demand in high-performance mode */
    
    /* Add welcome message to the database */
    if (!db_get_collection(db, "_system_config")) {
        db_create_collection(db, "_system_config");
        
        /* Create welcome message document */
        json_value_t* welcome_doc = json_create_object();
        json_object_set(welcome_doc, "type", json_create_string("welcome_message"));
        json_object_set(welcome_doc, "title", json_create_string("Welcome to JSONdb"));
        json_object_set(welcome_doc, "message", json_create_string("# Welcome to JSONdb!\n\nCongratulations! Your JSONdb instance is now running successfully.\n\n## Quick Start\n\n### 1. Create Collections\nUse the **Browser** tab to create new collections and add documents.\n\n### 2. Monitor Performance\nCheck the **Metrics** tab to monitor your database performance and JavaScript execution.\n\n### 3. Manage Access\nUse the **RBAC** tab to manage users, roles, and permissions.\n\n### 4. Database Operations\nThe **Operations** tab provides tools for backup, export, and maintenance.\n\n### 5. API Documentation\nExplore the **API** tab for complete REST API documentation.\n\n## Features\n\n- **High Performance**: Memory-mapped storage with O(log n) B+tree indexes\n- **JavaScript Integration**: Custom validators, transformers, and functions\n- **RBAC Security**: Role-based access control with JWT authentication\n- **Real-time Metrics**: Performance monitoring and analytics\n- **RESTful API**: Complete HTTP API for all operations\n\n## Getting Help\n\nFor documentation and examples, check the `/opt/jsondb/docs/` directory.\n\nHappy coding! 🚀"));
        json_object_set(welcome_doc, "created_at", json_create_string("2025-06-06T00:00:00Z"));
        json_object_set(welcome_doc, "version", json_create_string("3.0.0"));
        
        db_insert_document(db, "_system_config", welcome_doc);
    }
    
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
/* Persistence functions are implemented in persistence.c */

/* ==== Secondary Index Management ==== */

/* Extract field value from JSON document */
static json_value_t* extract_field_value(json_value_t* doc, const char* field) {
    if (!doc || doc->type != JSON_OBJECT || !field) return NULL;
    
    /* Handle nested fields with dot notation */
    char field_copy[256];
    strncpy(field_copy, field, sizeof(field_copy) - 1);
    field_copy[sizeof(field_copy) - 1] = '\0';
    
    json_value_t* current = doc;
    char* token = strtok(field_copy, ".");
    
    while (token && current && current->type == JSON_OBJECT) {
        current = json_object_get(current, token);
        token = strtok(NULL, ".");
    }
    
    return current;
}

/* Create secondary index on field */
index_t* db_create_index(database_t* db, const char* collection_name, const char* name,
                        const char* field_path, index_type_t type) {
    UNUSED(db);
    UNUSED(type); /* For now, we only support B+tree indexes */
    
    if (!g_database.initialized || !collection_name || !name || !field_path) {
        return NULL;
    }
    
    pthread_rwlock_rdlock(&g_database.lock);
    hp_collection_t* coll = find_collection(collection_name);
    pthread_rwlock_unlock(&g_database.lock);
    
    if (!coll) {
        LOG_ERROR("Collection not found: %s", collection_name);
        return NULL;
    }
    
    pthread_rwlock_wrlock(&coll->lock);
    
    /* Check if index already exists */
    for (int i = 0; i < coll->num_indexes; i++) {
        if (strcmp(coll->indexes[i].field_name, field_path) == 0) {
            pthread_rwlock_unlock(&coll->lock);
            LOG_INFO("Index already exists on field: %s", field_path);
            /* Return a dummy index_t to indicate success */
            index_t* existing = calloc(1, sizeof(index_t));
            if (existing) {
                existing->name = strdup(name);
                existing->field_path = strdup(field_path);
                existing->type = type;
            }
            return existing;
        }
    }
    
    /* Check if we have space for new index */
    if (coll->num_indexes >= MAX_INDEXES_PER_COLLECTION) {
        pthread_rwlock_unlock(&coll->lock);
        LOG_ERROR("Maximum indexes reached for collection: %s", collection_name);
        return NULL;
    }
    
    /* Create B+tree for index */
    char index_path[1024];
    snprintf(index_path, sizeof(index_path), "%s/%s_%s.idx", 
             g_database.path, collection_name, name);
    
    btree_disk_t* btree = btree_disk_create(index_path, 100, NULL);
    if (!btree) {
        pthread_rwlock_unlock(&coll->lock);
        LOG_ERROR("Failed to create B+tree index: %s", index_path);
        return NULL;
    }
    
    /* Add to collection */
    secondary_index_t* idx = &coll->indexes[coll->num_indexes];
    strncpy(idx->field_name, field_path, sizeof(idx->field_name) - 1);
    idx->btree = btree;
    idx->active = true;
    coll->num_indexes++;
    
    LOG_INFO("Created index %s on %s.%s", name, collection_name, field_path);
    
    /* TODO: Populate index with existing documents */
    
    pthread_rwlock_unlock(&coll->lock);
    
    /* Return index_t structure for compatibility */
    index_t* result = calloc(1, sizeof(index_t));
    if (result) {
        result->name = strdup(name);
        result->field_path = strdup(field_path);
        result->type = type;
        /* We don't populate other fields as they're not used in our implementation */
    }
    return result;
}

/* Drop index */
int db_drop_index(database_t* db, const char* collection_name, const char* name) {
    UNUSED(db);
    UNUSED(name); /* We identify indexes by field name for now */
    
    if (!g_database.initialized || !collection_name || !name) {
        return -1;
    }
    
    pthread_rwlock_rdlock(&g_database.lock);
    hp_collection_t* coll = find_collection(collection_name);
    pthread_rwlock_unlock(&g_database.lock);
    
    if (!coll) {
        LOG_ERROR("Collection not found: %s", collection_name);
        return -1;
    }
    
    pthread_rwlock_wrlock(&coll->lock);
    
    /* Find and remove index - for now we use name as field name */
    int found = -1;
    for (int i = 0; i < coll->num_indexes; i++) {
        if (strcmp(coll->indexes[i].field_name, name) == 0) {
            found = i;
            break;
        }
    }
    
    if (found < 0) {
        pthread_rwlock_unlock(&coll->lock);
        LOG_ERROR("Index not found with name: %s", name);
        return -1;
    }
    
    /* Destroy B+tree */
    btree_disk_destroy(coll->indexes[found].btree);
    
    /* Shift remaining indexes */
    for (int i = found; i < coll->num_indexes - 1; i++) {
        coll->indexes[i] = coll->indexes[i + 1];
    }
    coll->num_indexes--;
    
    LOG_INFO("Dropped index %s on %s", name, collection_name);
    
    pthread_rwlock_unlock(&coll->lock);
    return 0;
}

/* List indexes for collection */
json_value_t* db_list_indexes(database_t* db, const char* collection_name) {
    if (!g_database.initialized || !collection_name) {
        return NULL;
    }
    
    pthread_rwlock_rdlock(&g_database.lock);
    hp_collection_t* coll = find_collection(collection_name);
    pthread_rwlock_unlock(&g_database.lock);
    
    if (!coll) {
        LOG_ERROR("Collection not found: %s", collection_name);
        return NULL;
    }
    
    pthread_rwlock_rdlock(&coll->lock);
    
    json_value_t* result = json_create_object();
    json_value_t* indexes = json_create_array();
    
    /* Add primary index */
    json_value_t* primary = json_create_object();
    json_object_set(primary, "field", json_create_string("uuid"));
    json_object_set(primary, "type", json_create_string("hash"));
    json_object_set(primary, "unique", json_create_boolean(1));
    json_array_append(indexes, primary);
    
    /* Add secondary indexes */
    for (int i = 0; i < coll->num_indexes; i++) {
        if (coll->indexes[i].active) {
            json_value_t* index = json_create_object();
            json_object_set(index, "field", json_create_string(coll->indexes[i].field_name));
            json_object_set(index, "type", json_create_string("btree"));
            json_object_set(index, "unique", json_create_boolean(0));
            json_array_append(indexes, index);
        }
    }
    
    json_object_set(result, "collection", json_create_string(collection_name));
    json_object_set(result, "indexes", indexes);
    
    pthread_rwlock_unlock(&coll->lock);
    return result;
}
/* db_rebuild_indices is now fully implemented above */

/* Additional stub functions */
void db_complete_bootstrap(database_t* db) { UNUSED(db); }
int process_cache_invalidations(database_t* db) { UNUSED(db); return 0; }