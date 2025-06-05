#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "index/hash_index.h"
#include "utils/logger.h"

int main() {
    logger_init("/tmp/test_hash_420.log", LOG_LEVEL_TRACE);
    
    // Create a hash index
    hash_index_t* index = hash_index_create("/tmp/test_hash_420.idx", NULL);
    assert(index != NULL);
    
    printf("Testing insertion pattern that fails at 420...\n");
    
    // Test with the exact pattern from the failing test
    // Insert documents 418, 419, 420 to see what happens
    for (int i = 418; i <= 422; i++) {
        char doc_id[64];
        snprintf(doc_id, sizeof(doc_id), "doc-1749032608-100000%d-%04x", i, i);
        
        printf("\nInserting document %d: %s\n", i, doc_id);
        
        int rc = hash_index_insert(index, doc_id, strlen(doc_id), 4096 + i * 100);
        if (rc != 0) {
            printf("ERROR: Failed to insert doc_id='%s' at iteration %d\n", doc_id, i);
            
            // Let's check what's happening in the buckets
            printf("\nChecking bucket states...\n");
            for (uint32_t b = 0; b < index->directory_size; b++) {
                hash_dir_entry_t* dir_entry = &index->directory[b];
                hash_bucket_t* bucket = (hash_bucket_t*)
                    ((char*)index->storage->base_addr + dir_entry->bucket_offset);
                
                if (bucket->num_entries > 0) {
                    printf("Bucket %u: %u entries\n", b, bucket->num_entries);
                }
            }
            break;
        } else {
            printf("Success!\n");
        }
    }
    
    hash_index_destroy(index);
    logger_close();
    return 0;
}