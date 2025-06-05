#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "index/hash_index.h"
#include "utils/logger.h"

// Function to calculate hash (same as in hash_index.c)
static uint32_t fnv1a_hash(const void* key, size_t len) {
    const uint8_t* data = (const uint8_t*)key;
    uint32_t hash = 2166136261U;
    
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    
    return hash;
}

// Function to dump bucket entries
void dump_bucket_entries(hash_index_t* index, uint32_t bucket_index, const char* label) {
    hash_bucket_t* bucket = (hash_bucket_t*)
        ((char*)index->storage->base_addr + index->directory[bucket_index].bucket_offset);
    
    printf("\n%s - Bucket %u at offset %lu:\n", label, bucket_index, 
           index->directory[bucket_index].bucket_offset);
    printf("  local_depth=%u, num_entries=%u\n", bucket->local_depth, bucket->num_entries);
    
    for (uint32_t i = 0; i < bucket->num_entries; i++) {
        uint32_t offset = bucket->entry_offsets[i];
        if (offset >= 144 && offset < 4096) {  // Valid range
            hash_entry_t* entry = (hash_entry_t*)((char*)bucket + offset);
            if (entry->key_len < 100) {  // Reasonable key length
                char* key = (char*)(entry + 1);
                // Safely print first few chars of key
                printf("  Entry %u: offset=%u, key_len=%u, hash=0x%x, key=", 
                       i, offset, entry->key_len, entry->key_hash);
                for (int j = 0; j < 10 && j < entry->key_len; j++) {
                    printf("%c", key[j]);
                }
                printf("...\n");
            } else {
                printf("  Entry %u: offset=%u, CORRUPT (key_len=%u)\n", 
                       i, offset, entry->key_len);
            }
        } else {
            printf("  Entry %u: INVALID OFFSET %u\n", i, offset);
        }
    }
}

int main() {
    logger_init("/tmp/test_split_logic.log", LOG_LEVEL_DEBUG);
    
    // Create a hash index
    hash_index_t* index = hash_index_create("/tmp/test_split.idx", NULL);
    assert(index != NULL);
    
    printf("Testing split logic in detail...\n");
    printf("Initial: global_depth=%u, directory_size=%u\n", 
           index->global_depth, index->directory_size);
    
    // Track bucket 11
    uint64_t bucket_11_offset = index->directory[11].bucket_offset;
    uint32_t last_splits = index->splits;
    
    for (int i = 0; i < 500; i++) {
        char doc_id[64];
        snprintf(doc_id, sizeof(doc_id), "doc-1749032608-%d-%04x", 100000000 + i, i);
        
        // Check if a split occurred
        if (index->splits > last_splits) {
            printf("\n!!! SPLIT OCCURRED after document %d !!!\n", i-1);
            printf("Global depth: %u -> %u\n", 
                   (index->global_depth > 4) ? index->global_depth - 1 : 4, 
                   index->global_depth);
            printf("Directory size: %u\n", index->directory_size);
            
            // Check bucket 11
            if (index->directory[11].bucket_offset != bucket_11_offset) {
                printf("\nBucket 11 offset changed: %lu -> %lu\n", 
                       bucket_11_offset, index->directory[11].bucket_offset);
                dump_bucket_entries(index, 11, "Bucket 11 after split");
                bucket_11_offset = index->directory[11].bucket_offset;
            }
            
            // Check bucket 27 if it exists
            if (index->directory_size > 27) {
                printf("\nBucket 27 offset: %lu\n", index->directory[27].bucket_offset);
                dump_bucket_entries(index, 27, "Bucket 27 after split");
            }
            
            last_splits = index->splits;
        }
        
        int rc = hash_index_insert(index, doc_id, strlen(doc_id), 4096 + i * 100);
        
        if (rc != 0) {
            printf("\n!!! ERROR at document %d !!!\n", i);
            printf("Failed to insert: %s\n", doc_id);
            
            // Calculate which bucket it should go to
            uint32_t hash = fnv1a_hash(doc_id, strlen(doc_id));
            uint32_t bucket = hash & ((1U << index->global_depth) - 1);
            printf("Should go to bucket: %u\n", bucket);
            
            if (bucket == 11 || bucket == 27) {
                dump_bucket_entries(index, bucket, "Failed bucket");
            }
            
            break;
        }
    }
    
    hash_index_destroy(index);
    logger_close();
    return 0;
}