#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "index/hash_index.h"
#include "utils/logger.h"

// Function to calculate hash
static uint32_t fnv1a_hash(const void* key, size_t len) {
    const uint8_t* data = (const uint8_t*)key;
    uint32_t hash = 2166136261U;
    
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    
    return hash;
}

void dump_directory_state(hash_index_t* index, const char* phase) {
    printf("\n=== Directory State (%s) ===\n", phase);
    printf("global_depth: %u, directory_size: %u\n", index->global_depth, index->directory_size);
    printf("Directory entries:\n");
    for (uint32_t i = 0; i < index->directory_size; i++) {
        printf("  [%2u] bucket_offset=%8lu, local_depth=%u\n", 
               i, index->directory[i].bucket_offset, index->directory[i].local_depth);
    }
}

int main() {
    logger_init("/tmp/test_directory_tracking.log", LOG_LEVEL_INFO);
    
    // Create a hash index
    hash_index_t* index = hash_index_create("/tmp/test_directory.idx", NULL);
    assert(index != NULL);
    
    printf("Testing directory changes...\n");
    
    // Initial state
    dump_directory_state(index, "initial");
    
    // Store initial bucket 11 offset
    uint64_t initial_bucket_11_offset = index->directory[11].bucket_offset;
    printf("\nInitial bucket 11 offset: %lu\n", initial_bucket_11_offset);
    
    // Insert documents and monitor directory changes
    for (int i = 0; i < 500; i++) {
        char doc_id[64];
        snprintf(doc_id, sizeof(doc_id), "doc-1749032608-%d-%04x", 100000000 + i, i);
        
        // Check if bucket 11 offset changed
        if (index->directory[11].bucket_offset != initial_bucket_11_offset) {
            printf("\n!!! BUCKET 11 OFFSET CHANGED at i=%d !!!\n", i);
            printf("Old offset: %lu, New offset: %lu\n", 
                   initial_bucket_11_offset, index->directory[11].bucket_offset);
            dump_directory_state(index, "after change");
            
            // Check if directory size changed
            static uint32_t last_dir_size = 16;
            if (index->directory_size != last_dir_size) {
                printf("\nDirectory size changed from %u to %u\n", 
                       last_dir_size, index->directory_size);
                last_dir_size = index->directory_size;
            }
            break;
        }
        
        int rc = hash_index_insert(index, doc_id, strlen(doc_id), 4096 + i * 100);
        if (rc != 0) {
            printf("ERROR at i=%d\n", i);
            break;
        }
        
        // Periodic checks
        if (i > 0 && i % 100 == 0) {
            printf("Inserted %d documents, bucket 11 still at offset %lu\n", 
                   i, index->directory[11].bucket_offset);
        }
    }
    
    hash_index_destroy(index);
    logger_close();
    return 0;
}