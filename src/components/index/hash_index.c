#include "index/hash_index.h"
#include "utils/logger.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Default hash function (FNV-1a) */
static uint32_t fnv1a_hash(const void* key, size_t len) {
    const uint8_t* data = (const uint8_t*)key;
    uint32_t hash = 2166136261U;
    
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    
    return hash;
}

/* Allocate a new bucket */
static uint64_t allocate_bucket(hash_index_t* index) {
    uint64_t offset = index->storage->header->free_offset;
    index->storage->header->free_offset += HASH_BUCKET_SIZE;
    
    /* Initialize bucket */
    hash_bucket_t* bucket = (hash_bucket_t*)((char*)index->storage->base_addr + offset);
    memset(bucket, 0, HASH_BUCKET_SIZE);
    bucket->local_depth = index->global_depth;
    bucket->num_entries = 0;
    bucket->next_bucket = 0;
    
    atomic_fetch_add(&index->num_buckets, 1);
    return offset;
}

/* Split a bucket */
static int split_bucket(hash_index_t* index, uint32_t bucket_index) {
    pthread_rwlock_wrlock(&index->dir_lock);
    
    hash_dir_entry_t* dir_entry = &index->directory[bucket_index];
    hash_bucket_t* old_bucket = (hash_bucket_t*)
        ((char*)index->storage->base_addr + dir_entry->bucket_offset);
    
    /* Check if we need to grow directory */
    if (old_bucket->local_depth >= index->global_depth) {
        /* Double directory size */
        uint32_t new_size = index->directory_size * 2;
        hash_dir_entry_t* new_dir = realloc(index->directory, 
                                           new_size * sizeof(hash_dir_entry_t));
        if (!new_dir) {
            pthread_rwlock_unlock(&index->dir_lock);
            return -1;
        }
        
        /* Copy entries */
        for (uint32_t i = 0; i < index->directory_size; i++) {
            new_dir[i + index->directory_size] = new_dir[i];
        }
        
        index->directory = new_dir;
        index->directory_size = new_size;
        index->global_depth++;
    }
    
    /* Create new bucket */
    uint64_t new_bucket_offset = allocate_bucket(index);
    hash_bucket_t* new_bucket = (hash_bucket_t*)
        ((char*)index->storage->base_addr + new_bucket_offset);
    
    /* Increase local depth */
    old_bucket->local_depth++;
    new_bucket->local_depth = old_bucket->local_depth;
    
    /* Update directory entries */
    uint32_t old_mask = (1U << (old_bucket->local_depth - 1)) - 1;
    uint32_t new_bit = 1U << (old_bucket->local_depth - 1);
    
    for (uint32_t i = 0; i < index->directory_size; i++) {
        if ((i & old_mask) == (bucket_index & old_mask)) {
            if (i & new_bit) {
                index->directory[i].bucket_offset = new_bucket_offset;
                index->directory[i].local_depth = new_bucket->local_depth;
            }
        }
    }
    
    /* Rehash entries */
    uint32_t old_count = old_bucket->num_entries;
    old_bucket->num_entries = 0;
    new_bucket->num_entries = 0;
    
    /* Temporary storage for entries */
    struct {
        hash_entry_t* entry;
        void* key;
    } *entries = malloc(old_count * sizeof(*entries));
    
    if (!entries) {
        pthread_rwlock_unlock(&index->dir_lock);
        return -1;
    }
    
    /* Collect all entries */
    uint32_t entry_count = 0;
    for (uint32_t i = 0; i < old_count; i++) {
        uint32_t offset = old_bucket->entry_offsets[i];
        hash_entry_t* entry = (hash_entry_t*)((char*)old_bucket + offset);
        entries[entry_count].entry = entry;
        entries[entry_count].key = (char*)(entry + 1);
        entry_count++;
    }
    
    /* Clear old bucket (keep header) */
    memset(old_bucket->entry_offsets, 0, 
           HASH_BUCKET_SIZE - offsetof(hash_bucket_t, entry_offsets));
    
    /* Redistribute entries */
    for (uint32_t i = 0; i < entry_count; i++) {
        hash_entry_t* entry = entries[i].entry;
        void* key = entries[i].key;
        
        uint32_t hash = entry->key_hash;
        uint32_t new_index = hash_to_bucket(hash, index->global_depth);
        
        hash_bucket_t* target_bucket;
        if (index->directory[new_index].bucket_offset == dir_entry->bucket_offset) {
            target_bucket = old_bucket;
        } else {
            target_bucket = new_bucket;
        }
        
        /* Calculate offset for entry in target bucket */
        uint32_t entry_size = sizeof(hash_entry_t) + entry->key_len;
        uint32_t entry_offset = sizeof(hash_bucket_t) + 
                               target_bucket->num_entries * sizeof(uint32_t);
        
        /* Add space for existing entries */
        for (uint32_t j = 0; j < target_bucket->num_entries; j++) {
            hash_entry_t* existing = (hash_entry_t*)
                ((char*)target_bucket + target_bucket->entry_offsets[j]);
            entry_offset += sizeof(hash_entry_t) + existing->key_len;
        }
        
        /* Copy entry */
        memcpy((char*)target_bucket + entry_offset, entry, sizeof(hash_entry_t));
        memcpy((char*)target_bucket + entry_offset + sizeof(hash_entry_t), 
               key, entry->key_len);
        
        target_bucket->entry_offsets[target_bucket->num_entries] = entry_offset;
        target_bucket->num_entries++;
    }
    
    free(entries);
    atomic_fetch_add(&index->splits, 1);
    
    pthread_rwlock_unlock(&index->dir_lock);
    return 0;
}

/* Create hash index */
hash_index_t* hash_index_create(const char* path, 
                               uint32_t (*hash_fn)(const void*, size_t)) {
    if (!path) return NULL;
    
    hash_index_t* index = calloc(1, sizeof(hash_index_t));
    if (!index) return NULL;
    
    /* Create storage */
    index->storage = mmap_storage_create(path, 256 * 1024 * 1024); /* 256MB initial */
    if (!index->storage) {
        free(index);
        return NULL;
    }
    
    index->hash_fn = hash_fn ? hash_fn : fnv1a_hash;
    pthread_rwlock_init(&index->dir_lock, NULL);
    
    /* Initialize directory */
    index->global_depth = HASH_INITIAL_DEPTH;
    index->directory_size = 1U << index->global_depth;
    index->directory = calloc(index->directory_size, sizeof(hash_dir_entry_t));
    
    if (!index->directory) {
        mmap_storage_destroy(index->storage);
        free(index);
        return NULL;
    }
    
    /* Create initial buckets */
    for (uint32_t i = 0; i < index->directory_size; i++) {
        index->directory[i].bucket_offset = allocate_bucket(index);
        index->directory[i].local_depth = index->global_depth;
    }
    
    /* Initialize statistics */
    atomic_init(&index->num_keys, 0);
    atomic_init(&index->num_buckets, index->directory_size);
    atomic_init(&index->splits, 0);
    atomic_init(&index->lookups, 0);
    atomic_init(&index->collisions, 0);
    
    LOG_INFO("Created hash index with %u buckets", index->directory_size);
    return index;
}

/* Destroy hash index */
void hash_index_destroy(hash_index_t* index) {
    if (!index) return;
    
    free(index->directory);
    pthread_rwlock_destroy(&index->dir_lock);
    mmap_storage_destroy(index->storage);
    free(index);
}

/* Insert a key-value pair */
int hash_index_insert(hash_index_t* index, const void* key, size_t key_len,
                     uint64_t value_offset) {
    if (!index || !key) return -1;
    
    uint32_t hash = index->hash_fn(key, key_len);
    uint32_t bucket_index = hash_to_bucket(hash, index->global_depth);
    
    pthread_rwlock_rdlock(&index->dir_lock);
    
retry:
    hash_dir_entry_t* dir_entry = &index->directory[bucket_index];
    hash_bucket_t* bucket = (hash_bucket_t*)
        ((char*)index->storage->base_addr + dir_entry->bucket_offset);
    
    /* Check if key already exists */
    for (uint32_t i = 0; i < bucket->num_entries; i++) {
        hash_entry_t* entry = (hash_entry_t*)
            ((char*)bucket + bucket->entry_offsets[i]);
        
        if (entry->key_hash == hash && entry->key_len == key_len) {
            void* entry_key = (char*)(entry + 1);
            if (memcmp(entry_key, key, key_len) == 0) {
                /* Update existing entry */
                entry->value_offset = value_offset;
                pthread_rwlock_unlock(&index->dir_lock);
                return 0;
            }
        }
    }
    
    /* Check if bucket has space */
    uint32_t entry_size = sizeof(hash_entry_t) + key_len;
    uint32_t used_space = sizeof(hash_bucket_t) + 
                         bucket->num_entries * sizeof(uint32_t);
    
    for (uint32_t i = 0; i < bucket->num_entries; i++) {
        hash_entry_t* entry = (hash_entry_t*)
            ((char*)bucket + bucket->entry_offsets[i]);
        used_space += sizeof(hash_entry_t) + entry->key_len;
    }
    
    if (used_space + entry_size > HASH_BUCKET_SIZE) {
        /* Bucket full, need to split */
        pthread_rwlock_unlock(&index->dir_lock);
        
        if (split_bucket(index, bucket_index) != 0) {
            return -1;
        }
        
        /* Recalculate bucket after split */
        bucket_index = hash_to_bucket(hash, index->global_depth);
        pthread_rwlock_rdlock(&index->dir_lock);
        goto retry;
    }
    
    /* Add new entry */
    uint32_t entry_offset = used_space;
    hash_entry_t* new_entry = (hash_entry_t*)((char*)bucket + entry_offset);
    
    new_entry->key_hash = hash;
    new_entry->key_len = key_len;
    new_entry->value_len = 0; /* Not used currently */
    new_entry->value_offset = value_offset;
    
    memcpy((char*)(new_entry + 1), key, key_len);
    
    bucket->entry_offsets[bucket->num_entries] = entry_offset;
    bucket->num_entries++;
    
    atomic_fetch_add(&index->num_keys, 1);
    
    pthread_rwlock_unlock(&index->dir_lock);
    return 0;
}

/* Search for a key */
int hash_index_search(hash_index_t* index, const void* key, size_t key_len,
                     uint64_t* value_offset) {
    if (!index || !key || !value_offset) return -1;
    
    atomic_fetch_add(&index->lookups, 1);
    
    uint32_t hash = index->hash_fn(key, key_len);
    uint32_t bucket_index = hash_to_bucket(hash, index->global_depth);
    
    pthread_rwlock_rdlock(&index->dir_lock);
    
    hash_dir_entry_t* dir_entry = &index->directory[bucket_index];
    hash_bucket_t* bucket = (hash_bucket_t*)
        ((char*)index->storage->base_addr + dir_entry->bucket_offset);
    
    /* Search in bucket */
    for (uint32_t i = 0; i < bucket->num_entries; i++) {
        hash_entry_t* entry = (hash_entry_t*)
            ((char*)bucket + bucket->entry_offsets[i]);
        
        if (entry->key_hash == hash && entry->key_len == key_len) {
            void* entry_key = (char*)(entry + 1);
            if (memcmp(entry_key, key, key_len) == 0) {
                *value_offset = entry->value_offset;
                pthread_rwlock_unlock(&index->dir_lock);
                return 0;
            }
            atomic_fetch_add(&index->collisions, 1);
        }
    }
    
    pthread_rwlock_unlock(&index->dir_lock);
    return -1; /* Not found */
}

/* Get statistics */
void hash_index_stats(hash_index_t* index, uint64_t* num_keys,
                     uint64_t* num_buckets, uint64_t* avg_chain_length) {
    if (!index) return;
    
    if (num_keys) *num_keys = atomic_load(&index->num_keys);
    if (num_buckets) *num_buckets = atomic_load(&index->num_buckets);
    
    if (avg_chain_length) {
        uint64_t keys = atomic_load(&index->num_keys);
        uint64_t buckets = atomic_load(&index->num_buckets);
        *avg_chain_length = buckets > 0 ? keys / buckets : 0;
    }
}