#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "index/hash_index.h"
#include "utils/logger.h"

// Simulate the document ID generation pattern from database.c
void generate_doc_id(char* buffer, size_t size) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    
    // Using similar pattern to database.c
    snprintf(buffer, size, "doc-%ld-%09ld-%04x",
             ts.tv_sec, ts.tv_nsec, rand() & 0xFFFF);
}

int main() {
    logger_init("/tmp/test_database_pattern.log", LOG_LEVEL_DEBUG);
    srand(time(NULL));
    
    // Create a hash index
    hash_index_t* index = hash_index_create("/tmp/test_db_pattern.idx", NULL);
    assert(index != NULL);
    
    printf("Testing with database-style document IDs...\n");
    
    // Insert documents with database-style IDs
    for (int i = 0; i < 250; i++) {
        char doc_id[64];
        generate_doc_id(doc_id, sizeof(doc_id));
        
        // Storage offset would be allocated by mmap_storage
        uint64_t offset = 4096 + i * 1024;
        
        int rc = hash_index_insert(index, doc_id, strlen(doc_id), offset);
        if (rc != 0) {
            printf("ERROR: Failed to insert doc_id='%s' at iteration %d\n", doc_id, i);
            
            // Dump debug info
            uint32_t hash = index->hash_fn(doc_id, strlen(doc_id));
            uint32_t bucket_idx = hash & ((1U << index->global_depth) - 1);
            printf("Hash: 0x%x, Bucket: %u\n", hash, bucket_idx);
            
            // Get bucket info
            hash_dir_entry_t* dir_entry = &index->directory[bucket_idx];
            hash_bucket_t* bucket = (hash_bucket_t*)
                ((char*)index->storage->base_addr + dir_entry->bucket_offset);
            
            printf("Bucket has %u entries\n", bucket->num_entries);
            printf("First few entry offsets:");
            for (uint32_t j = 0; j < bucket->num_entries && j < 10; j++) {
                printf(" %u", bucket->entry_offsets[j]);
            }
            printf("\n");
            
            break;
        }
        
        if ((i + 1) % 50 == 0) {
            printf("Inserted %d documents\n", i + 1);
        }
    }
    
    // Print stats
    uint64_t num_keys, num_buckets, avg_chain;
    hash_index_stats(index, &num_keys, &num_buckets, &avg_chain);
    printf("\nFinal stats: keys=%lu, buckets=%lu, avg_chain=%lu\n",
           num_keys, num_buckets, avg_chain);
    
    hash_index_destroy(index);
    logger_close();
    return 0;
}