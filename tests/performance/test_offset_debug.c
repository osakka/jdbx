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

void check_bucket_11(hash_index_t* index, const char* when) {
    hash_bucket_t* bucket = (hash_bucket_t*)
        ((char*)index->storage->base_addr + index->directory[11].bucket_offset);
    
    printf("\n%s - Bucket 11:\n", when);
    printf("  num_entries=%u\n", bucket->num_entries);
    
    // Check first few entries and last entry
    for (uint32_t i = 0; i < bucket->num_entries && i < 3; i++) {
        printf("  entry_offsets[%u]=%u\n", i, bucket->entry_offsets[i]);
    }
    if (bucket->num_entries > 3) {
        printf("  ...\n");
        printf("  entry_offsets[%u]=%u\n", 
               bucket->num_entries-1, bucket->entry_offsets[bucket->num_entries-1]);
    }
    
    // Check if offset 144 has the expected data
    if (bucket->num_entries > 0 && bucket->entry_offsets[0] == 144) {
        hash_entry_t* entry = (hash_entry_t*)((char*)bucket + 144);
        printf("  Entry at offset 144: key_len=%u\n", entry->key_len);
        if (entry->key_len == 1901) {
            printf("  \!\!\! CORRUPTION DETECTED \!\!\!\n");
            // Show what is at offset 1901
            if (bucket->num_entries >= 34) {
                printf("  entry_offsets[33]=%u (this is being read as key_len\!)\n", 
                       bucket->entry_offsets[33]);
            }
        }
    }
}

int main() {
    logger_init("/tmp/test_offset_debug.log", LOG_LEVEL_DEBUG);
    
    // Create a hash index
    hash_index_t* index = hash_index_create("/tmp/test_offset.idx", NULL);
    assert(index \!= NULL);
    
    printf("Testing offset corruption in bucket 11...\n");
    
    // Insert all documents up to 420
    for (int i = 0; i < 421; i++) {
        char doc_id[64];
        snprintf(doc_id, sizeof(doc_id), "doc-1749032608-%d-%04x", 100000000 + i, i);
        
        // Check state before document 420
        if (i == 419) {
            check_bucket_11(index, "Before document 420");
        }
        
        int rc = hash_index_insert(index, doc_id, strlen(doc_id), 4096 + i * 100);
        
        if (rc \!= 0) {
            printf("\nERROR at document %d\n", i);
            check_bucket_11(index, "After error");
            break;
        }
        
        // Progress indicator
        if (i > 0 && i % 100 == 0) {
            printf("Inserted %d documents\n", i);
        }
    }
    
    hash_index_destroy(index);
    logger_close();
    return 0;
}
ENDOFFILE < /dev/null
