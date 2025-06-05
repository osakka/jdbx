#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "index/hash_index.h"
#include "utils/logger.h"

// Function to calculate hash (copied from hash_index.c since it's static)
static uint32_t fnv1a_hash(const void* key, size_t len) {
    const uint8_t* data = (const uint8_t*)key;
    uint32_t hash = 2166136261U;
    
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    
    return hash;
}

int main() {
    logger_init("/tmp/test_bucket_11.log", LOG_LEVEL_DEBUG);
    
    // Create a hash index
    hash_index_t* index = hash_index_create("/tmp/test_bucket_11.idx", NULL);
    assert(index != NULL);
    
    printf("Testing insertions to find what goes into bucket 11...\n");
    
    // Try different keys to find ones that hash to bucket 11
    int found_count = 0;
    for (int i = 0; i < 10000 && found_count < 10; i++) {
        char key[64];
        snprintf(key, sizeof(key), "test-key-%d", i);
        
        uint32_t hash = fnv1a_hash(key, strlen(key));
        uint32_t bucket = hash & 15; // With 16 buckets (depth 4)
        
        if (bucket == 11) {
            printf("Key '%s' hashes to bucket 11 (hash=0x%x)\n", key, hash);
            
            int rc = hash_index_insert(index, key, strlen(key), 4096 + found_count * 100);
            if (rc != 0) {
                printf("ERROR: Failed to insert key='%s'\n", key);
                break;
            }
            found_count++;
        }
    }
    
    printf("\nInserted %d keys into bucket 11\n", found_count);
    
    // Now try to insert a document ID that would be #420 in the original test
    char doc_id[64];
    snprintf(doc_id, sizeof(doc_id), "doc-1749032608-100000420-01a4");
    uint32_t hash = fnv1a_hash(doc_id, strlen(doc_id));
    uint32_t bucket = hash & 15;
    
    printf("\nDocument ID '%s' hashes to bucket %u (hash=0x%x)\n", doc_id, bucket, hash);
    
    if (bucket == 11) {
        printf("This is the problematic document!\n");
        int rc = hash_index_insert(index, doc_id, strlen(doc_id), 42000);
        if (rc != 0) {
            printf("ERROR: Failed to insert the problematic document\n");
        } else {
            printf("Successfully inserted the problematic document\n");
        }
    }
    
    hash_index_destroy(index);
    logger_close();
    return 0;
}