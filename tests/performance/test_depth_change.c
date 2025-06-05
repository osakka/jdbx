#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "index/hash_index.h"
#include "utils/logger.h"

int main() {
    logger_init("/tmp/test_depth_change.log", LOG_LEVEL_INFO);
    
    // Create a hash index
    hash_index_t* index = hash_index_create("/tmp/test_depth.idx", NULL);
    assert(index != NULL);
    
    printf("Testing depth changes...\n");
    printf("Initial: global_depth=%u, directory_size=%u\n", 
           index->global_depth, index->directory_size);
    
    // Insert documents and monitor depth changes
    uint32_t last_depth = index->global_depth;
    uint32_t last_dir_size = index->directory_size;
    
    for (int i = 0; i < 500; i++) {
        char doc_id[64];
        snprintf(doc_id, sizeof(doc_id), "doc-1749032608-%d-%04x", 100000000 + i, i);
        
        int rc = hash_index_insert(index, doc_id, strlen(doc_id), 4096 + i * 100);
        
        // Check for changes
        if (index->global_depth != last_depth || index->directory_size != last_dir_size) {
            printf("\n!!! DIRECTORY CHANGE at i=%d !!!\n", i);
            printf("global_depth: %u -> %u\n", last_depth, index->global_depth);
            printf("directory_size: %u -> %u\n", last_dir_size, index->directory_size);
            
            // Check bucket 11's new location
            printf("Bucket 11 offset: %lu\n", index->directory[11].bucket_offset);
            if (index->directory_size > 27) {
                printf("Bucket 27 offset: %lu\n", index->directory[27].bucket_offset);
            }
            
            last_depth = index->global_depth;
            last_dir_size = index->directory_size;
        }
        
        if (rc != 0) {
            printf("\nERROR at i=%d (after %u insertions)\n", i, i);
            printf("Current: global_depth=%u, directory_size=%u\n", 
                   index->global_depth, index->directory_size);
            
            // If directory grew, check both bucket 11 and 27
            if (index->directory_size > 16) {
                printf("Bucket 11 points to offset: %lu\n", index->directory[11].bucket_offset);
                printf("Bucket 27 points to offset: %lu\n", index->directory[27].bucket_offset);
                
                // The document that failed should now go to bucket 27
                printf("\nDocument %d should now go to bucket %u\n", i, i & ((1U << index->global_depth) - 1));
            }
            break;
        }
        
        if (i > 0 && i % 100 == 0) {
            printf("Progress: %d documents inserted\n", i);
        }
    }
    
    // Print final statistics
    printf("\nFinal statistics:\n");
    printf("Total keys: %lu\n", (unsigned long)index->num_keys);
    printf("Total buckets: %lu\n", (unsigned long)index->num_buckets);
    printf("Total splits: %lu\n", (unsigned long)index->splits);
    
    hash_index_destroy(index);
    logger_close();
    return 0;
}