#include "index/hash_index.h"
#include "utils/logger.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>  /* For offsetof */

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
    
    /* Check if allocation would exceed the data region (must stay below index_offset) */
    if (offset + HASH_BUCKET_SIZE > index->storage->header->index_offset) {
        LOG_ERROR("Bucket allocation exceeds data region: offset=%lu, index_offset=%lu",
                  offset, index->storage->header->index_offset);
        return 0;
    }
    
    /* Update free_offset only after validation */
    index->storage->header->free_offset += HASH_BUCKET_SIZE;
    hash_bucket_t* bucket = (hash_bucket_t*)((char*)index->storage->base_addr + offset);
    memset(bucket, 0, HASH_BUCKET_SIZE);
    bucket->local_depth = index->global_depth;
    bucket->num_entries = 0;
    bucket->next_bucket = 0;
    
    /* Explicitly clear entry_offsets array to ensure no garbage */
    /* Clear more slots than minimum to be extra safe against corruption */
    uint32_t slots_to_clear = 64;  /* Increased from 32 for safety */
    uint32_t max_slots = (HASH_BUCKET_SIZE - offsetof(hash_bucket_t, entry_offsets)) / sizeof(uint32_t);
    if (slots_to_clear > max_slots) {
        slots_to_clear = max_slots;
    }
    for (uint32_t i = 0; i < slots_to_clear; i++) {
        bucket->entry_offsets[i] = 0;
    }
    
    atomic_fetch_add(&index->num_buckets, 1);
    return offset;
}

/* Split a bucket */
static int split_bucket(hash_index_t* index, uint32_t bucket_index) {
    LOG_DEBUG("split_bucket: Starting split for bucket %u", bucket_index);
    pthread_rwlock_wrlock(&index->dir_lock);
    
    /* Debug: Log initial state */
    LOG_INFO("SPLIT DEBUG: Bucket %u, global_depth=%u, directory_size=%u", 
             bucket_index, index->global_depth, index->directory_size);
    
    /* Store bucket offset before potential realloc */
    uint64_t bucket_offset = index->directory[bucket_index].bucket_offset;
    
    /* Validate bucket offset */
    if (bucket_offset + HASH_BUCKET_SIZE > index->storage->mapped_size) {
        LOG_ERROR("Invalid bucket offset: %lu", bucket_offset);
        pthread_rwlock_unlock(&index->dir_lock);
        return -1;
    }
    
    hash_bucket_t* old_bucket = (hash_bucket_t*)
        ((char*)index->storage->base_addr + bucket_offset);
    
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
    if (new_bucket_offset == 0) {
        LOG_ERROR("Cannot allocate new bucket during split.");
        pthread_rwlock_unlock(&index->dir_lock);
        return -1;
    }
    hash_bucket_t* new_bucket = (hash_bucket_t*)
        ((char*)index->storage->base_addr + new_bucket_offset);
    
    /* Increase local depth */
    old_bucket->local_depth++;
    new_bucket->local_depth = old_bucket->local_depth;
    
    /* Update directory entries */
    uint32_t old_mask = (1U << (old_bucket->local_depth - 1)) - 1;
    uint32_t new_bit = 1U << (old_bucket->local_depth - 1);
    
    /* Remember which directories point to which bucket for redistribution */
    uint32_t dirs_to_old = 0;
    uint32_t dirs_to_new = 0;
    
    for (uint32_t i = 0; i < index->directory_size; i++) {
        if ((i & old_mask) == (bucket_index & old_mask)) {
            if (i & new_bit) {
                index->directory[i].bucket_offset = new_bucket_offset;
                index->directory[i].local_depth = new_bucket->local_depth;
                dirs_to_new++;
            } else {
                /* Update the local depth for the old bucket entries too */
                index->directory[i].local_depth = old_bucket->local_depth;
                dirs_to_old++;
            }
        }
    }
    
    LOG_DEBUG("Split complete: %u dirs -> old bucket, %u dirs -> new bucket", 
              dirs_to_old, dirs_to_new);
    
    /* Rehash entries */
    uint32_t old_count = old_bucket->num_entries;
    old_bucket->num_entries = 0;
    new_bucket->num_entries = 0;
    
    /* Temporary storage for entries - we need to copy the data */
    struct {
        hash_entry_t entry;  /* Copy of entry, not pointer */
        char key[256];       /* Buffer for key data */
    } *entries = malloc(old_count * sizeof(*entries));
    
    if (!entries) {
        pthread_rwlock_unlock(&index->dir_lock);
        return -1;
    }
    
    /* Collect all entries - copy the data before we clear the bucket */
    uint32_t entry_count = 0;
    LOG_DEBUG("SPLIT DEBUG: Collecting %u entries from bucket %u", old_count, bucket_index);
    
    for (uint32_t i = 0; i < old_count; i++) {
        uint32_t offset = old_bucket->entry_offsets[i];
        if (offset < 144 || offset >= HASH_BUCKET_SIZE) {
            LOG_ERROR("SPLIT DEBUG: Invalid offset %u at position %u in bucket %u", 
                      offset, i, bucket_index);
            free(entries);
            pthread_rwlock_unlock(&index->dir_lock);
            return -1;
        }
        hash_entry_t* entry = (hash_entry_t*)((char*)old_bucket + offset);
        
        /* Copy entry header */
        entries[entry_count].entry = *entry;
        
        /* Copy key data */
        if (entry->key_len > sizeof(entries[entry_count].key)) {
            LOG_ERROR("Key too large: %u bytes", entry->key_len);
            free(entries);
            pthread_rwlock_unlock(&index->dir_lock);
            return -1;
        }
        memcpy(entries[entry_count].key, (char*)(entry + 1), entry->key_len);
        entry_count++;
    }
    
    /* CRITICAL FIX: Clear ALL entry offset slots to prevent corruption
     * The bug was that we only cleared old_count slots, but the bucket
     * reserves space for at least 128 slots. Any uncleared slots could
     * contain garbage data that gets misinterpreted as valid offsets.
     */
    uint32_t min_slots = 128;  /* Minimum slots we reserve - increased to prevent high entry count overlap */
    uint32_t slots_to_clear = old_count > min_slots ? old_count : min_slots;
    
    /* Ensure we don't exceed bucket bounds */
    uint32_t max_possible_slots = (HASH_BUCKET_SIZE - offsetof(hash_bucket_t, entry_offsets)) / sizeof(uint32_t);
    if (slots_to_clear > max_possible_slots) {
        slots_to_clear = max_possible_slots;
    }
    
    LOG_DEBUG("Clearing %u entry offset slots in old bucket (had %u entries)", 
              slots_to_clear, old_count);
    
    memset(old_bucket->entry_offsets, 0, sizeof(uint32_t) * slots_to_clear);
    old_bucket->num_entries = 0;  /* Reset count since we're redistributing */
    
    /* New bucket is already zeroed from allocation */
    
    /* Redistribute entries */
    for (uint32_t i = 0; i < entry_count; i++) {
        hash_entry_t* entry = &entries[i].entry;  /* Use address of copied entry */
        void* key = entries[i].key;               /* Use copied key data */
        
        uint32_t hash = entry->key_hash;
        uint32_t new_index = hash_to_bucket(hash, index->global_depth);
        
        hash_bucket_t* target_bucket;
        /* Determine target based on the split bit, not current directory state */
        /* The old bucket keeps entries where the new bit is 0 */
        /* The new bucket gets entries where the new bit is 1 */
        if ((new_index & new_bit) == 0) {
            target_bucket = old_bucket;
        } else {
            target_bucket = new_bucket;
        }
        
        /* Calculate offset for entry in target bucket */
        /* Reserve space for at least 128 entry offsets */
        uint32_t min_offset_slots = 128;
        uint32_t offset_slots = target_bucket->num_entries + 1;
        if (offset_slots < min_offset_slots) {
            offset_slots = min_offset_slots;
        }
        /* Use offsetof to get the correct header size */
        uint32_t fixed_header_size = offsetof(hash_bucket_t, entry_offsets);
        uint32_t header_size = fixed_header_size + offset_slots * sizeof(uint32_t);
        
        /* Calculate data size based on entries already placed in target bucket */
        /* IMPORTANT: We cannot read from entry_offsets if this is the old bucket
         * because we just cleared them! We need to track the running offset. */
        uint32_t data_size = 0;
        
        /* If this is a fresh start (num_entries == 0), data_size is 0 */
        /* Otherwise, we need to calculate based on what we've already placed */
        if (target_bucket->num_entries > 0) {
            /* Find the last entry's offset and size */
            uint32_t last_offset = target_bucket->entry_offsets[target_bucket->num_entries - 1];
            if (last_offset > 0) {
                hash_entry_t* last_entry = (hash_entry_t*)((char*)target_bucket + last_offset);
                data_size = last_offset + sizeof(hash_entry_t) + last_entry->key_len - header_size;
            }
        }
        
        uint32_t entry_offset = header_size + data_size;
        
        /* Copy entry */
        memcpy((char*)target_bucket + entry_offset, entry, sizeof(hash_entry_t));
        memcpy((char*)target_bucket + entry_offset + sizeof(hash_entry_t), 
               key, entry->key_len);
        
        /* Debug logging for original bucket */
        if (target_bucket == old_bucket && bucket_index == 11) {
            LOG_DEBUG("Redistributing to original bucket (was 11): entry %u at offset %u, key_len=%u",
                      target_bucket->num_entries, entry_offset, entry->key_len);
        }
        
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
        uint64_t bucket_offset = allocate_bucket(index);
        if (bucket_offset == 0) {
            LOG_ERROR("Cannot allocate initial bucket %u", i);
            /* Clean up already allocated buckets */
            free(index->directory);
            mmap_storage_destroy(index->storage);
            free(index);
            return NULL;
        }
        index->directory[i].bucket_offset = bucket_offset;
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
    
    pthread_rwlock_wrlock(&index->dir_lock);
    
retry:
    /* Calculate bucket index inside the lock and after retry label */
    /* This ensures we use the current global_depth if it changed during a split */
    uint32_t bucket_index = hash_to_bucket(hash, index->global_depth);
    LOG_DEBUG("hash_index_insert: key_len=%zu, hash=0x%x, bucket_index=%u, value_offset=%lu", 
              key_len, hash, bucket_index, value_offset);
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
    
    /* Debug: Log bucket state for bucket 11 */
    if (bucket_index == 11 && bucket->num_entries == 0) {
        LOG_DEBUG("hash_index_insert: First insertion into bucket 11, key_len=%zu", key_len);
    }
    
    /* Calculate the actual used space in the bucket */
    /* Reserve space for entry offsets to avoid overlapping with data */
    /* CRITICAL: The first entry is placed AFTER all reserved slots */
    /* If we reserve 64 slots (indices 0-63), the first entry goes at offset 12+64*4=268 */
    /* But if we have 64 entries, we'll write to slot 64, which IS at offset 268! */
    /* And if we have 65 entries, we'll write to slot 65 at offset 272, overlapping data! */
    /* So we must reserve MORE slots than we'll ever use for safety */
    uint32_t min_offset_slots = 128;  /* Increased from 64 to prevent overlap at higher entry counts */
    uint32_t offset_slots = bucket->num_entries + 1;
    if (offset_slots < min_offset_slots) {
        offset_slots = min_offset_slots;
    }
    
    /* Debug check for bucket 11 */
    if (bucket_index == 11 && bucket->num_entries >= 31) {
        LOG_DEBUG("Bucket 11: num_entries=%u, offset_slots=%u", 
                  bucket->num_entries, offset_slots);
    }
    /* Use offsetof to get the correct header size without the flexible array member */
    uint32_t fixed_header_size = offsetof(hash_bucket_t, entry_offsets);
    uint32_t header_size = fixed_header_size + offset_slots * sizeof(uint32_t);
    
    uint32_t data_size = 0;
    for (uint32_t i = 0; i < bucket->num_entries; i++) {
        uint32_t offset = bucket->entry_offsets[i];
        if (offset == 0) {
            LOG_ERROR("Zero offset for entry %u in bucket %u", i, bucket_index);
            pthread_rwlock_unlock(&index->dir_lock);
            return -1;
        }
        if (offset >= HASH_BUCKET_SIZE) {
            LOG_ERROR("Invalid entry offset %u in bucket %u", offset, bucket_index);
            pthread_rwlock_unlock(&index->dir_lock);
            return -1;
        }
        
        /* Ensure offset is past the header */
        uint32_t min_offset = offsetof(hash_bucket_t, entry_offsets) + 32 * sizeof(uint32_t);
        if (offset < min_offset) {
            LOG_ERROR("Entry offset %u is in header area (min %u) in bucket %u", 
                     offset, min_offset, bucket_index);
            pthread_rwlock_unlock(&index->dir_lock);
            return -1;
        }
        
        hash_entry_t* entry = (hash_entry_t*)((char*)bucket + offset);
        if (entry->key_len > 1024) {  /* Sanity check */
            LOG_ERROR("Invalid key length %u at offset %u in bucket %u", 
                     entry->key_len, offset, bucket_index);
            pthread_rwlock_unlock(&index->dir_lock);
            return -1;
        }
        
        data_size += sizeof(hash_entry_t) + entry->key_len;
    }
    
    uint32_t used_space = header_size + data_size;
    
    if (used_space + entry_size > HASH_BUCKET_SIZE) {
        /* Bucket full, need to split */
        LOG_INFO("Bucket %u is full (used=%u, entry_size=%u), splitting...", 
                 bucket_index, used_space, entry_size);
        pthread_rwlock_unlock(&index->dir_lock);
        
        if (split_bucket(index, bucket_index) != 0) {
            LOG_ERROR("Cannot split bucket %u", bucket_index);
            return -1;
        }
        
        /* Reacquire lock and retry - bucket_index will be recalculated after retry label */
        pthread_rwlock_wrlock(&index->dir_lock);
        goto retry;
    }
    
    /* Add new entry */
    /* The entry should be placed after the header and all existing data */
    uint32_t entry_offset = header_size + data_size;
    
    /* Ensure we don't write into the entry_offsets array */
    uint32_t min_required_offset = offsetof(hash_bucket_t, entry_offsets) + 
                                  (bucket->num_entries + 1) * sizeof(uint32_t);
    if (entry_offset < min_required_offset) {
        LOG_ERROR("Entry offset %u would overlap with header (min required: %u)", 
                  entry_offset, min_required_offset);
        pthread_rwlock_unlock(&index->dir_lock);
        return -1;
    }
    
    hash_entry_t* new_entry = (hash_entry_t*)((char*)bucket + entry_offset);
    
    new_entry->key_hash = hash;
    new_entry->key_len = key_len;
    new_entry->value_len = 0; /* Not used currently */
    new_entry->value_offset = value_offset;
    
    memcpy((char*)(new_entry + 1), key, key_len);
    
    /* Double-check that we're not exceeding bucket bounds */
    if (entry_offset + entry_size > HASH_BUCKET_SIZE) {
        LOG_ERROR("Entry would exceed bucket bounds: offset=%u, size=%u", 
                  entry_offset, entry_size);
        pthread_rwlock_unlock(&index->dir_lock);
        return -1;
    }
    
    /* CRITICAL DEBUG: Check if we're about to corrupt bucket 11 */
    if (bucket_index == 11 && bucket->num_entries == 32) {
        LOG_ERROR("About to add 33rd entry to bucket 11.");
        LOG_INFO("  entry_offset=%u, should be > 144", entry_offset);
        LOG_INFO("  header_size=%u, data_size=%u", header_size, data_size);
        
        /* Check what's currently at offset 144 */
        hash_entry_t* first_entry = (hash_entry_t*)((char*)bucket + 144);
        LOG_INFO("  Current entry at offset 144: key_len=%u", first_entry->key_len);
    }
    
    bucket->entry_offsets[bucket->num_entries] = entry_offset;
    bucket->num_entries++;
    
    LOG_DEBUG("hash_index_insert: Added entry at offset %u in bucket %u (now has %u entries)", 
              entry_offset, bucket_index, bucket->num_entries);
    LOG_DEBUG("hash_index_insert: Entry details - key_hash=0x%x, key_len=%u, value_offset=%lu", 
              new_entry->key_hash, new_entry->key_len, new_entry->value_offset);
    
    /* Post-insert check for bucket 11 */
    if (bucket_index == 11 && bucket->num_entries == 33) {
        LOG_ERROR("After adding 33rd entry to bucket 11.");
        /* Check if we corrupted offset 144 */
        hash_entry_t* first_entry = (hash_entry_t*)((char*)bucket + 144);
        LOG_INFO("  Entry at offset 144 now has key_len=%u", first_entry->key_len);
        if (first_entry->key_len > 1000) {
            LOG_ERROR("  CORRUPTION DETECTED!");
        }
    }
    
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
    LOG_DEBUG("hash_index_search: key_len=%zu, hash=0x%x, bucket_index=%u", 
              key_len, hash, bucket_index);
    
    pthread_rwlock_rdlock(&index->dir_lock);
    
    hash_dir_entry_t* dir_entry = &index->directory[bucket_index];
    hash_bucket_t* bucket = (hash_bucket_t*)
        ((char*)index->storage->base_addr + dir_entry->bucket_offset);
    
    /* Search in bucket */
    LOG_DEBUG("hash_index_search: Searching bucket at offset %lu with %u entries", 
              dir_entry->bucket_offset, bucket->num_entries);
    
    for (uint32_t i = 0; i < bucket->num_entries; i++) {
        hash_entry_t* entry = (hash_entry_t*)
            ((char*)bucket + bucket->entry_offsets[i]);
        
        LOG_DEBUG("hash_index_search: Checking entry %u at offset %u, hash=0x%x", 
                  i, bucket->entry_offsets[i], entry->key_hash);
        
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

/* Delete a key */
int hash_index_delete(hash_index_t* index, const void* key, size_t key_len) {
    if (!index || !key) return -1;
    
    uint32_t hash = index->hash_fn(key, key_len);
    uint32_t bucket_index = hash_to_bucket(hash, index->global_depth);
    
    pthread_rwlock_wrlock(&index->dir_lock);
    
    hash_dir_entry_t* dir_entry = &index->directory[bucket_index];
    hash_bucket_t* bucket = (hash_bucket_t*)
        ((char*)index->storage->base_addr + dir_entry->bucket_offset);
    
    /* Search for key in bucket */
    for (uint32_t i = 0; i < bucket->num_entries; i++) {
        hash_entry_t* entry = (hash_entry_t*)
            ((char*)bucket + bucket->entry_offsets[i]);
        
        if (entry->key_hash == hash && entry->key_len == key_len) {
            void* entry_key = (char*)(entry + 1);
            if (memcmp(entry_key, key, key_len) == 0) {
                /* Found the entry, remove it */
                
                /* Shift remaining entries down */
                for (uint32_t j = i; j < bucket->num_entries - 1; j++) {
                    bucket->entry_offsets[j] = bucket->entry_offsets[j + 1];
                }
                
                bucket->num_entries--;
                atomic_fetch_sub(&index->num_keys, 1);
                
                pthread_rwlock_unlock(&index->dir_lock);
                return 0; /* Success */
            }
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