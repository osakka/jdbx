#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "index/hash_index.h"
#include "utils/logger.h"

void dump_bucket_detailed(hash_bucket_t* bucket, const char* label) {
    printf("\n=== %s ===\n", label);
    printf("local_depth: %u\n", bucket->local_depth);
    printf("num_entries: %u\n", bucket->num_entries);
    printf("next_bucket: %lu\n", bucket->next_bucket);
    
    // Dump raw bytes at start of bucket
    printf("First 256 bytes of bucket:\n");
    unsigned char* p = (unsigned char*)bucket;
    for (int i = 0; i < 256; i++) {
        if (i % 16 == 0) printf("%04x: ", i);
        printf("%02x ", p[i]);
        if (i % 16 == 15) printf("\n");
    }
    printf("\n");
}

int main() {
    logger_init("/tmp/test_split_debug.log", LOG_LEVEL_DEBUG);
    
    // Create a hash index
    hash_index_t* index = hash_index_create("/tmp/test_split.idx", NULL);
    assert(index != NULL);
    
    printf("Created hash index with %u buckets\n", index->directory_size);
    
    // Insert enough entries into one bucket to trigger a split
    // Use keys that hash to the same bucket
    int target_bucket = 3;
    
    for (int i = 0; i < 200; i++) {
        // Generate keys that will hash to bucket 3 with initial depth of 4
        char key[64];
        int attempt = 0;
        while (1) {
            snprintf(key, sizeof(key), "test-key-%d-%d", i, attempt);
            uint32_t hash = index->hash_fn(key, strlen(key));
            uint32_t bucket_idx = hash & ((1U << index->global_depth) - 1);
            
            if (bucket_idx == target_bucket) {
                printf("Inserting key '%s' (hash=0x%x) into bucket %u\n", 
                       key, hash, bucket_idx);
                
                int rc = hash_index_insert(index, key, strlen(key), 4096 + i * 100);
                if (rc != 0) {
                    printf("ERROR: Failed to insert at iteration %d\n", i);
                    
                    // Dump bucket state
                    hash_dir_entry_t* dir_entry = &index->directory[target_bucket];
                    hash_bucket_t* bucket = (hash_bucket_t*)
                        ((char*)index->storage->base_addr + dir_entry->bucket_offset);
                    dump_bucket_detailed(bucket, "Failed bucket");
                    
                    hash_index_destroy(index);
                    logger_close();
                    return 1;
                }
                break;
            }
            attempt++;
        }
        
        // Check if a split occurred
        static uint32_t last_dir_size = 0;
        if (index->directory_size > last_dir_size) {
            printf("\n*** DIRECTORY DOUBLED: %u -> %u ***\n", 
                   last_dir_size, index->directory_size);
            last_dir_size = index->directory_size;
        }
    }
    
    printf("\nTest completed successfully!\n");
    hash_index_destroy(index);
    logger_close();
    return 0;
}