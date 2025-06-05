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

void dump_bucket_memory(hash_index_t* index, uint32_t bucket_index, const char* label) {
    hash_bucket_t* bucket = (hash_bucket_t*)
        ((char*)index->storage->base_addr + index->directory[bucket_index].bucket_offset);
    
    printf("\n%s - Bucket %u at offset %lu:\n", label, bucket_index, 
           index->directory[bucket_index].bucket_offset);
    printf("  local_depth=%u, num_entries=%u\n", bucket->local_depth, bucket->num_entries);
    
    // Dump first 256 bytes
    unsigned char* mem = (unsigned char*)bucket;
    printf("  Memory dump (first 256 bytes):\n");
    for (int i = 0; i < 256; i += 16) {
        printf("    %04x: ", i);
        for (int j = 0; j < 16; j++) {
            printf("%02x ", mem[i+j]);
        }
        printf("\n");
    }
}

int main() {
    logger_init("/tmp/test_split_tracking.log", LOG_LEVEL_DEBUG);
    
    // Create a hash index
    hash_index_t* index = hash_index_create("/tmp/test_split.idx", NULL);
    assert(index != NULL);
    
    printf("Testing bucket split behavior...\n");
    
    // Count documents going to bucket 11
    int docs_in_bucket_11 = 0;
    uint64_t last_bucket_11_offset = index->directory[11].bucket_offset;
    
    for (int i = 0; i < 500; i++) {
        char doc_id[64];
        snprintf(doc_id, sizeof(doc_id), "doc-1749032608-%d-%04x", 100000000 + i, i);
        
        // Calculate which bucket this will go to
        uint32_t hash = fnv1a_hash(doc_id, strlen(doc_id));
        uint32_t bucket_index = hash & ((1U << index->global_depth) - 1);
        
        if (bucket_index == 11) {
            docs_in_bucket_11++;
        }
        
        // Check if bucket 11's offset is about to change
        if (index->directory[11].bucket_offset != last_bucket_11_offset) {
            printf("\n!!! BUCKET 11 OFFSET CHANGED at i=%d !!!\n", i);
            printf("Old offset: %lu, New offset: %lu\n", 
                   last_bucket_11_offset, index->directory[11].bucket_offset);
            dump_bucket_memory(index, 11, "After split");
            last_bucket_11_offset = index->directory[11].bucket_offset;
        }
        
        int rc = hash_index_insert(index, doc_id, strlen(doc_id), 4096 + i * 100);
        
        if (rc != 0) {
            printf("\n!!! ERROR at document %d !!!\n", i);
            printf("Failed to insert: %s (target bucket: %u)\n", doc_id, bucket_index);
            
            if (bucket_index == 11) {
                dump_bucket_memory(index, 11, "At failure");
            }
            
            break;
        }
        
        // Check if bucket 11's offset changed after insert
        if (index->directory[11].bucket_offset != last_bucket_11_offset) {
            printf("\n!!! BUCKET 11 OFFSET CHANGED after insert at i=%d !!!\n", i);
            printf("Old offset: %lu, New offset: %lu\n", 
                   last_bucket_11_offset, index->directory[11].bucket_offset);
            printf("Global depth: %u, Directory size: %u\n", 
                   index->global_depth, index->directory_size);
            dump_bucket_memory(index, 11, "After insert-triggered split");
            
            // If directory doubled, check bucket 27 too
            if (index->directory_size > 16) {
                printf("\nBucket 27 (split partner) at offset %lu\n", 
                       index->directory[27].bucket_offset);
            }
            
            last_bucket_11_offset = index->directory[11].bucket_offset;
        }
    }
    
    hash_index_destroy(index);
    logger_close();
    return 0;
}