#ifndef HASH_INDEX_H
#define HASH_INDEX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>
#include "storage/mmap_storage.h"

/* Hash index for O(1) point queries
 * Uses extendible hashing for dynamic growth
 */

#define HASH_BUCKET_SIZE 4096
#define HASH_INITIAL_DEPTH 4
#define HASH_MAX_DEPTH 32

/* Hash bucket header */
typedef struct hash_bucket {
    uint32_t local_depth;    /* Local depth for this bucket */
    uint32_t num_entries;    /* Number of entries in bucket */
    uint64_t next_bucket;    /* Overflow bucket pointer */
    uint32_t entry_offsets[]; /* Offsets to entries within bucket */
} hash_bucket_t;

/* Hash entry */
typedef struct hash_entry {
    uint32_t key_hash;       /* Full hash value */
    uint32_t key_len;
    uint32_t value_len;
    uint64_t value_offset;   /* Offset in data file */
    /* Key data follows */
} hash_entry_t;

/* Hash directory entry */
typedef struct hash_dir_entry {
    uint64_t bucket_offset;  /* Offset to bucket */
    uint32_t local_depth;    /* Cached local depth */
} hash_dir_entry_t;

/* Hash index structure */
typedef struct hash_index {
    mmap_storage_t* storage;
    
    /* Directory */
    uint32_t global_depth;
    uint32_t directory_size;
    hash_dir_entry_t* directory;
    pthread_rwlock_t dir_lock;
    
    /* Statistics */
    _Atomic uint64_t num_keys;
    _Atomic uint64_t num_buckets;
    _Atomic uint64_t splits;
    _Atomic uint64_t lookups;
    _Atomic uint64_t collisions;
    
    /* Hash function */
    uint32_t (*hash_fn)(const void* key, size_t len);
} hash_index_t;

/* Create and destroy hash index */
hash_index_t* hash_index_create(const char* path, 
                               uint32_t (*hash_fn)(const void*, size_t));
void hash_index_destroy(hash_index_t* index);

/* Basic operations */
int hash_index_insert(hash_index_t* index, const void* key, size_t key_len,
                     uint64_t value_offset);
int hash_index_search(hash_index_t* index, const void* key, size_t key_len,
                     uint64_t* value_offset);
int hash_index_delete(hash_index_t* index, const void* key, size_t key_len);

/* Batch operations */
int hash_index_bulk_insert(hash_index_t* index,
                          const void** keys, const size_t* key_lens,
                          const uint64_t* value_offsets, size_t count);

/* Statistics */
void hash_index_stats(hash_index_t* index, uint64_t* num_keys,
                     uint64_t* num_buckets, uint64_t* avg_chain_length);

/* Internal helpers */
static inline uint32_t hash_to_bucket(uint32_t hash, uint32_t depth) {
    return hash & ((1U << depth) - 1);
}

#endif /* HASH_INDEX_H */