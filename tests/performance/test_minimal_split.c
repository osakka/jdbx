#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "index/hash_index.h"
#include "utils/logger.h"

// Function to print raw memory
void dump_memory(void* ptr, size_t size, const char* label) {
    unsigned char* p = (unsigned char*)ptr;
    printf("\n%s:\n", label);
    for (size_t i = 0; i < size; i += 16) {
        printf("  %04zx: ", i);
        for (size_t j = 0; j < 16 && i+j < size; j++) {
            printf("%02x ", p[i+j]);
        }
        printf("\n");
    }
}

int main() {
    logger_init("/tmp/test_minimal_split.log", LOG_LEVEL_DEBUG);
    
    // Create a hash index
    hash_index_t* index = hash_index_create("/tmp/test_minimal.idx", NULL);
    assert(index != NULL);
    
    printf("Testing minimal split scenario...\n");
    
    // Find which bucket gets split first
    int split_count = 0;
    
    for (int i = 0; i < 500 && split_count < 2; i++) {
        char doc_id[64];
        snprintf(doc_id, sizeof(doc_id), "doc-1749032608-%d-%04x", 100000000 + i, i);
        
        uint32_t old_splits = index->splits;
        
        int rc = hash_index_insert(index, doc_id, strlen(doc_id), 4096 + i * 100);
        
        if (index->splits > old_splits) {
            split_count++;
            printf("\n=== SPLIT %d occurred after document %d ===\n", split_count, i-1);
            printf("Global depth: %u, Directory size: %u\n", 
                   index->global_depth, index->directory_size);
            
            // Check all buckets to see which one split
            for (uint32_t b = 0; b < index->directory_size; b++) {
                hash_bucket_t* bucket = (hash_bucket_t*)
                    ((char*)index->storage->base_addr + index->directory[b].bucket_offset);
                if (bucket->num_entries > 0) {
                    printf("Bucket %u: offset=%lu, local_depth=%u, entries=%u\n",
                           b, index->directory[b].bucket_offset, 
                           bucket->local_depth, bucket->num_entries);
                }
            }
            
            // Dump bucket 11 if it exists
            if (index->directory_size > 11) {
                hash_bucket_t* bucket11 = (hash_bucket_t*)
                    ((char*)index->storage->base_addr + index->directory[11].bucket_offset);
                dump_memory(bucket11, 256, "Bucket 11 after split");
            }
        }
        
        if (rc != 0) {
            printf("\nERROR at document %d\n", i);
            break;
        }
    }
    
    hash_index_destroy(index);
    logger_close();
    return 0;
}