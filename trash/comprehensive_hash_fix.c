/* Comprehensive fix for hash index corruption issue
 * 
 * Key changes:
 * 1. Properly clear ALL entry_offset slots during split
 * 2. Add validation to ensure we don't read garbage offsets
 * 3. Improve bounds checking
 * 4. Add defensive programming against offset array corruption
 */

/* In split_bucket function, replace the clearing section with: */

    /* CRITICAL FIX: Clear ALL entry offset slots to prevent corruption
     * The bug was that we only cleared old_count slots, but the bucket
     * reserves space for at least 32 slots. Any uncleared slots could
     * contain garbage data that gets misinterpreted as valid offsets.
     */
    
    /* Calculate the maximum number of slots that could have been used */
    uint32_t min_slots = 32;  /* Minimum slots we reserve */
    uint32_t max_possible_slots = (HASH_BUCKET_SIZE - offsetof(hash_bucket_t, entry_offsets)) / sizeof(uint32_t);
    
    /* Be conservative and clear a reasonable number of slots */
    uint32_t slots_to_clear = min_slots;
    if (old_count > min_slots) {
        slots_to_clear = old_count;
    }
    
    /* Ensure we don't exceed bucket bounds */
    if (slots_to_clear * sizeof(uint32_t) + offsetof(hash_bucket_t, entry_offsets) > HASH_BUCKET_SIZE) {
        slots_to_clear = max_possible_slots;
    }
    
    LOG_DEBUG("Clearing %u entry offset slots in old bucket (had %u entries)", 
              slots_to_clear, old_count);
    
    /* Clear the slots - this prevents any garbage data from being interpreted as offsets */
    memset(old_bucket->entry_offsets, 0, sizeof(uint32_t) * slots_to_clear);
    old_bucket->num_entries = 0;

/* Additional validation in hash_index_insert when reading existing entries: */

    /* Validate each offset before using it */
    for (uint32_t i = 0; i < bucket->num_entries; i++) {
        uint32_t offset = bucket->entry_offsets[i];
        
        /* Comprehensive validation */
        if (offset == 0) {
            LOG_ERROR("Zero offset for entry %u in bucket %u", i, bucket_index);
            pthread_rwlock_unlock(&index->dir_lock);
            return -1;
        }
        
        /* Ensure offset is beyond the header area */
        uint32_t min_data_offset = offsetof(hash_bucket_t, entry_offsets) + 32 * sizeof(uint32_t);
        if (offset < min_data_offset) {
            LOG_ERROR("Entry offset %u is in header area (min %u) in bucket %u", 
                     offset, min_data_offset, bucket_index);
            pthread_rwlock_unlock(&index->dir_lock);
            return -1;
        }
        
        /* Ensure offset doesn't exceed bucket size */
        if (offset >= HASH_BUCKET_SIZE) {
            LOG_ERROR("Entry offset %u exceeds bucket size in bucket %u", 
                     offset, bucket_index);
            pthread_rwlock_unlock(&index->dir_lock);
            return -1;
        }
        
        /* Validate the entry at this offset */
        if (offset + sizeof(hash_entry_t) > HASH_BUCKET_SIZE) {
            LOG_ERROR("Entry at offset %u would exceed bucket bounds", offset);
            pthread_rwlock_unlock(&index->dir_lock);
            return -1;
        }
        
        hash_entry_t* entry = (hash_entry_t*)((char*)bucket + offset);
        
        /* Sanity check the key length */
        if (entry->key_len == 0 || entry->key_len > 1024) {
            LOG_ERROR("Invalid key length %u at offset %u in bucket %u", 
                     entry->key_len, offset, bucket_index);
            pthread_rwlock_unlock(&index->dir_lock);
            return -1;
        }
        
        /* Ensure the full entry fits in the bucket */
        if (offset + sizeof(hash_entry_t) + entry->key_len > HASH_BUCKET_SIZE) {
            LOG_ERROR("Entry at offset %u with key_len %u exceeds bucket bounds", 
                     offset, entry->key_len);
            pthread_rwlock_unlock(&index->dir_lock);
            return -1;
        }
        
        data_size += sizeof(hash_entry_t) + entry->key_len;
    }

/* In allocate_bucket, ensure we clear more slots: */

    /* Explicitly clear entry_offsets array to ensure no garbage */
    /* Clear enough slots to cover any reasonable usage */
    uint32_t slots_to_clear = 64;  /* Increase from 32 to be extra safe */
    if (slots_to_clear * sizeof(uint32_t) + offsetof(hash_bucket_t, entry_offsets) <= HASH_BUCKET_SIZE) {
        for (uint32_t i = 0; i < slots_to_clear; i++) {
            bucket->entry_offsets[i] = 0;
        }
    }