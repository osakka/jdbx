#ifndef INDEXED_MMAP_STORAGE_H
#define INDEXED_MMAP_STORAGE_H

#include "storage/mmap_storage.h"
#include "index/btree_disk.h"
#include "index/hash_index.h"
#include "utils/skiplist.h"

/* Indexed storage combining mmap storage with multiple index types
 * Provides automatic index management and query optimization
 */

/* Index types */
typedef enum index_type {
    INDEX_BTREE,      /* B+tree for range queries */
    INDEX_HASH,       /* Hash for point queries */
    INDEX_BITMAP,     /* Bitmap for low-cardinality fields */
    INDEX_FULLTEXT    /* Full-text search index */
} index_type_t;

/* Index metadata */
typedef struct index_meta {
    char name[256];
    index_type_t type;
    char** fields;
    size_t field_count;
    bool is_unique;
    bool is_covering;
    
    /* Index implementation */
    union {
        btree_disk_t* btree;
        hash_index_t* hash;
        void* bitmap;
        void* fulltext;
    } impl;
} index_meta_t;

/* Indexed storage structure */
typedef struct indexed_storage {
    /* Base storage */
    mmap_storage_t* storage;
    
    /* Indexes */
    index_meta_t** indexes;
    size_t index_count;
    pthread_rwlock_t index_lock;
    
    /* Primary key index (always hash) */
    hash_index_t* primary_index;
    
    /* Query optimizer hints */
    bool prefer_hash_for_point;
    bool prefer_btree_for_range;
    
    /* Statistics */
    _Atomic uint64_t index_hits;
    _Atomic uint64_t index_misses;
    _Atomic uint64_t optimizer_hints;
} indexed_storage_t;

/* Create indexed storage */
indexed_storage_t* indexed_storage_create(const char* path, size_t initial_size);
void indexed_storage_destroy(indexed_storage_t* storage);

/* Document operations with automatic indexing */
int indexed_storage_put(indexed_storage_t* storage, 
                       const void* key, size_t key_len,
                       const void* value, size_t value_len);

int indexed_storage_get(indexed_storage_t* storage,
                       const void* key, size_t key_len,
                       void** value, size_t* value_len);

int indexed_storage_delete(indexed_storage_t* storage,
                          const void* key, size_t key_len);

/* Index management */
int indexed_storage_create_index(indexed_storage_t* storage,
                                const char* index_name,
                                index_type_t type,
                                const char** fields,
                                size_t field_count,
                                bool is_unique);

int indexed_storage_drop_index(indexed_storage_t* storage,
                              const char* index_name);

/* Query operations */
typedef struct query_result {
    void** keys;
    size_t* key_lens;
    void** values;
    size_t* value_lens;
    size_t count;
    size_t capacity;
} query_result_t;

query_result_t* indexed_storage_query(indexed_storage_t* storage,
                                     const char* index_name,
                                     const void* start_key, size_t start_len,
                                     const void* end_key, size_t end_len);

void query_result_destroy(query_result_t* result);

/* Batch operations */
int indexed_storage_put_batch(indexed_storage_t* storage,
                             const void** keys, const size_t* key_lens,
                             const void** values, const size_t* value_lens,
                             size_t count);

/* Statistics and optimization */
void indexed_storage_stats(indexed_storage_t* storage);
void indexed_storage_optimize(indexed_storage_t* storage);

#endif /* INDEXED_MMAP_STORAGE_H */