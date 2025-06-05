#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "storage/mmap_storage.h"
#include "index/hash_index.h"
#include "utils/json.h"
#include "utils/logger.h"

/* Collection v2 - High-performance collection implementation */

typedef struct {
    char name[256];
    mmap_storage_t* storage;
    hash_index_t* primary_index;
    pthread_rwlock_t lock;
    
    /* Statistics */
    atomic_uint_fast64_t doc_count;
    atomic_uint_fast64_t total_size;
    atomic_uint_fast64_t insert_count;
    atomic_uint_fast64_t query_count;
} collection_v2_t;

/* Create a new high-performance collection */
collection_v2_t* collection_v2_create(const char* name, const char* storage_path) {
    if (!name || !storage_path) return NULL;
    
    collection_v2_t* coll = calloc(1, sizeof(collection_v2_t));
    if (!coll) return NULL;
    
    strncpy(coll->name, name, sizeof(coll->name) - 1);
    
    /* Initialize storage with 1GB initial size */
    char mmap_path[512];
    snprintf(mmap_path, sizeof(mmap_path), "%s/%s.mmap", storage_path, name);
    coll->storage = mmap_storage_create(mmap_path, 1024 * 1024 * 1024);
    if (!coll->storage) {
        free(coll);
        return NULL;
    }
    
    /* Initialize primary index */
    char index_path[512];
    snprintf(index_path, sizeof(index_path), "%s/%s.idx", storage_path, name);
    coll->primary_index = hash_index_create(index_path, NULL);
    if (!coll->primary_index) {
        mmap_storage_destroy(coll->storage);
        free(coll);
        return NULL;
    }
    
    /* Initialize lock */
    pthread_rwlock_init(&coll->lock, NULL);
    
    /* Initialize statistics */
    atomic_init(&coll->doc_count, 0);
    atomic_init(&coll->total_size, 0);
    atomic_init(&coll->insert_count, 0);
    atomic_init(&coll->query_count, 0);
    
    LOG_INFO("Created high-performance collection: %s", name);
    return coll;
}

/* Insert a document */
int collection_v2_insert(collection_v2_t* coll, const char* id, cjson_t* doc) {
    if (!coll || !id || !doc) return -1;
    
    /* Serialize document */
    char* json_str = cjson_to_string(doc);
    if (!json_str) return -1;
    
    size_t doc_size = strlen(json_str);
    
    /* Write lock for insert */
    pthread_rwlock_wrlock(&coll->lock);
    
    /* Store in mmap storage */
    uint64_t offset = coll->storage->header->free_offset;
    int result = mmap_storage_put(coll->storage, id, strlen(id), json_str, doc_size);
    
    if (result == 0) {
        /* Update index */
        result = hash_index_insert(coll->primary_index, id, strlen(id), offset);
        
        if (result == 0) {
            /* Update statistics */
            atomic_fetch_add(&coll->doc_count, 1);
            atomic_fetch_add(&coll->total_size, doc_size);
            atomic_fetch_add(&coll->insert_count, 1);
        }
    }
    
    pthread_rwlock_unlock(&coll->lock);
    free(json_str);
    
    return result;
}

/* Find a document by ID */
cjson_t* collection_v2_find_by_id(collection_v2_t* coll, const char* id) {
    if (!coll || !id) return NULL;
    
    /* Read lock for query */
    pthread_rwlock_rdlock(&coll->lock);
    
    /* Lookup in index */
    uint64_t offset;
    if (hash_index_search(coll->primary_index, id, strlen(id), &offset) != 0) {
        pthread_rwlock_unlock(&coll->lock);
        return NULL;
    }
    
    /* Retrieve from storage */
    void* data;
    size_t data_len;
    if (mmap_storage_get(coll->storage, id, strlen(id), &data, &data_len) != 0) {
        pthread_rwlock_unlock(&coll->lock);
        return NULL;
    }
    
    pthread_rwlock_unlock(&coll->lock);
    
    /* Parse JSON */
    cjson_t* doc = cjson_parse((char*)data);
    free(data);
    
    /* Update statistics */
    atomic_fetch_add(&coll->query_count, 1);
    
    return doc;
}

/* Update a document */
int collection_v2_update(collection_v2_t* coll, const char* id, cjson_t* doc) {
    if (!coll || !id || !doc) return -1;
    
    /* For simplicity, update is delete + insert */
    /* In production, would implement in-place update */
    
    pthread_rwlock_wrlock(&coll->lock);
    
    /* Check if document exists */
    uint64_t old_offset;
    if (hash_index_search(coll->primary_index, id, strlen(id), &old_offset) != 0) {
        pthread_rwlock_unlock(&coll->lock);
        return -1; /* Document not found */
    }
    
    /* Serialize new document */
    char* json_str = cjson_to_string(doc);
    if (!json_str) {
        pthread_rwlock_unlock(&coll->lock);
        return -1;
    }
    
    size_t doc_size = strlen(json_str);
    
    /* Store new version */
    uint64_t new_offset = coll->storage->header->free_offset;
    int result = mmap_storage_put(coll->storage, id, strlen(id), json_str, doc_size);
    
    if (result == 0) {
        /* Update index to point to new location */
        result = hash_index_insert(coll->primary_index, id, strlen(id), new_offset);
        
        if (result == 0) {
            /* Update statistics */
            atomic_fetch_add(&coll->total_size, doc_size);
        }
    }
    
    pthread_rwlock_unlock(&coll->lock);
    free(json_str);
    
    return result;
}

/* Delete a document */
int collection_v2_delete(collection_v2_t* coll, const char* id) {
    if (!coll || !id) return -1;
    
    pthread_rwlock_wrlock(&coll->lock);
    
    /* Remove from index */
    int result = hash_index_delete(coll->primary_index, id, strlen(id));
    
    if (result == 0) {
        /* Update statistics */
        atomic_fetch_sub(&coll->doc_count, 1);
        /* Note: In production, would track deleted space for reuse */
    }
    
    pthread_rwlock_unlock(&coll->lock);
    
    return result;
}

/* Get collection statistics */
void collection_v2_stats(collection_v2_t* coll, uint64_t* doc_count, 
                        uint64_t* total_size, uint64_t* insert_count,
                        uint64_t* query_count) {
    if (!coll) return;
    
    if (doc_count) *doc_count = atomic_load(&coll->doc_count);
    if (total_size) *total_size = atomic_load(&coll->total_size);
    if (insert_count) *insert_count = atomic_load(&coll->insert_count);
    if (query_count) *query_count = atomic_load(&coll->query_count);
}

/* Destroy collection */
void collection_v2_destroy(collection_v2_t* coll) {
    if (!coll) return;
    
    pthread_rwlock_destroy(&coll->lock);
    hash_index_destroy(coll->primary_index);
    mmap_storage_destroy(coll->storage);
    free(coll);
}