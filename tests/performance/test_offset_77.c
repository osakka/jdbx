#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "index/hash_index.h"
#include "utils/logger.h"

void check_bucket_at_offset(hash_index_t* index, uint32_t bucket_idx, uint32_t offset) {
    hash_dir_entry_t* dir_entry = &index->directory[bucket_idx];
    hash_bucket_t* bucket = (hash_bucket_t*)
        ((char*)index->storage->base_addr + dir_entry->bucket_offset);
    
    printf("\nChecking bucket %u at offset %u:\n", bucket_idx, offset);
    
    // Print raw bytes at that offset
    unsigned char* p = (unsigned char*)bucket + offset;
    printf("Raw bytes at offset %u: ", offset);
    for (int i = 0; i < 32; i++) {
        printf("%02x ", p[i]);
    }
    printf("\n");
    
    // Try to interpret as hash_entry_t
    if (offset < HASH_BUCKET_SIZE - sizeof(hash_entry_t)) {
        hash_entry_t* entry = (hash_entry_t*)((char*)bucket + offset);
        printf("As hash_entry_t: key_hash=0x%x, key_len=%u, value_len=%u, value_offset=%lu\n",
               entry->key_hash, entry->key_len, entry->value_len, entry->value_offset);
    }
}

int main() {
    logger_init("/tmp/test_offset_77.log", LOG_LEVEL_DEBUG);
    
    // Create index and insert some data
    hash_index_t* index = hash_index_create("/tmp/test_offset.idx", NULL);
    assert(index != NULL);
    
    printf("Testing offset 77 issue...\n");
    
    // Insert enough entries to get some data in various buckets
    for (int i = 0; i < 200; i++) {
        char key[64];
        snprintf(key, sizeof(key), "doc-%d-%d-%04x", 1749033571, 900000000 + i, i);
        
        int rc = hash_index_insert(index, key, strlen(key), 4096 + i * 100);
        if (rc != 0) {
            printf("ERROR: Failed at iteration %d\n", i);
            break;
        }
    }
    
    // Check offset 77 in various buckets
    for (uint32_t b = 0; b < 4; b++) {
        check_bucket_at_offset(index, b, 77);
    }
    
    // Also check the structure of bucket 2
    printf("\nBucket 2 structure:\n");
    hash_dir_entry_t* dir_entry = &index->directory[2];
    hash_bucket_t* bucket = (hash_bucket_t*)
        ((char*)index->storage->base_addr + dir_entry->bucket_offset);
    
    printf("num_entries: %u\n", bucket->num_entries);
    printf("Entry offsets:");
    for (uint32_t i = 0; i < bucket->num_entries && i < 10; i++) {
        printf(" %u", bucket->entry_offsets[i]);
    }
    printf("\n");
    
    hash_index_destroy(index);
    logger_close();
    return 0;
}