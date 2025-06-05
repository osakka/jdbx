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

// Function to examine bucket state
void examine_bucket(hash_index_t* index, uint32_t bucket_index) {
    hash_bucket_t* bucket = (hash_bucket_t*)
        ((char*)index->storage->base_addr + index->directory[bucket_index].bucket_offset);
    
    printf("\nBucket %u state:\n", bucket_index);
    printf("  Offset: %lu\n", index->directory[bucket_index].bucket_offset);
    printf("  Local depth: %u\n", bucket->local_depth);
    printf("  Num entries: %u\n", bucket->num_entries);
    
    // Check each entry
    for (uint32_t i = 0; i < bucket->num_entries; i++) {
        uint32_t offset = bucket->entry_offsets[i];
        printf("  Entry %u: offset=%u", i, offset);
        
        if (offset < 144) {  // 144 is the minimum safe offset (header + 32 slots)
            printf(" [TOO LOW!]");
        }
        
        if (offset < 4096) {  // Within reasonable bounds
            hash_entry_t* entry = (hash_entry_t*)((char*)bucket + offset);
            printf(", key_len=%u", entry->key_len);
            
            if (entry->key_len > 100) {
                printf(" [SUSPICIOUS!]");
            }
        }
        printf("\n");
    }
}

int main() {
    logger_init("/tmp/test_bucket_state.log", LOG_LEVEL_DEBUG);
    
    // Create a hash index
    hash_index_t* index = hash_index_create("/tmp/test_bucket_state.idx", NULL);
    assert(index != NULL);
    
    printf("Testing bucket state tracking...\n");
    printf("Initial: global_depth=%u, directory_size=%u\n", 
           index->global_depth, index->directory_size);
    
    // Track which documents go to bucket 11
    int docs_in_bucket_11 = 0;
    
    for (int i = 0; i < 500; i++) {
        char doc_id[64];
        snprintf(doc_id, sizeof(doc_id), "doc-1749032608-%d-%04x", 100000000 + i, i);
        
        // Calculate which bucket this will go to
        uint32_t hash = fnv1a_hash(doc_id, strlen(doc_id));
        uint32_t bucket_index = hash & ((1U << index->global_depth) - 1);
        
        if (bucket_index == 11) {
            docs_in_bucket_11++;
            printf("\n=== Document %d goes to bucket 11 (total: %d) ===\n", 
                   i, docs_in_bucket_11);
            printf("Doc ID: %s\n", doc_id);
            printf("Hash: 0x%x\n", hash);
            
            // Examine bucket 11 before insertion
            examine_bucket(index, 11);
        }
        
        int rc = hash_index_insert(index, doc_id, strlen(doc_id), 4096 + i * 100);
        
        if (rc != 0) {
            printf("\n!!! ERROR at document %d !!!\n", i);
            printf("Failed to insert: %s\n", doc_id);
            printf("Target bucket was: %u\n", bucket_index);
            
            // Examine the problematic bucket
            if (bucket_index == 11) {
                printf("\nDetailed bucket 11 state at failure:\n");
                examine_bucket(index, 11);
                
                // Also check the raw memory around offset 144
                hash_bucket_t* bucket = (hash_bucket_t*)
                    ((char*)index->storage->base_addr + index->directory[11].bucket_offset);
                
                printf("\nMemory dump around offset 144:\n");
                unsigned char* mem = (unsigned char*)bucket + 140;
                for (int j = 0; j < 16; j++) {
                    printf("  [%3d] = 0x%02x", 140 + j, mem[j]);
                    if (j == 4) printf(" <- offset 144");
                    printf("\n");
                }
            }
            
            break;
        }
        
        if (bucket_index == 11 && rc == 0) {
            printf("Successfully inserted into bucket 11\n");
            // Check bucket state after insertion
            examine_bucket(index, 11);
        }
    }
    
    // Print final statistics
    printf("\n\nFinal statistics:\n");
    printf("Total keys: %lu\n", (unsigned long)index->num_keys);
    printf("Total buckets: %lu\n", (unsigned long)index->num_buckets);
    printf("Total splits: %lu\n", (unsigned long)index->splits);
    printf("Documents that went to bucket 11: %d\n", docs_in_bucket_11);
    
    hash_index_destroy(index);
    logger_close();
    return 0;
}