#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "index/hash_index.h"
#include "utils/logger.h"

int main() {
    logger_init("/tmp/test_hash_simple.log", LOG_LEVEL_DEBUG);
    
    // Create a hash index
    hash_index_t* index = hash_index_create("/tmp/test_hash_simple.idx", NULL);
    assert(index != NULL);
    
    printf("Testing hash index with sequential document IDs...\n");
    
    // Insert documents with IDs similar to what the database generates
    // Stop at 500 to focus on the error
    for (int i = 0; i < 500; i++) {
        char doc_id[64];
        snprintf(doc_id, sizeof(doc_id), "doc-%d-%d-%04x", 
                 1749032608, 100000000 + i, i);
        
        int rc = hash_index_insert(index, doc_id, strlen(doc_id), 4096 + i * 100);
        if (rc != 0) {
            printf("ERROR: Failed to insert doc_id='%s' at iteration %d\n", doc_id, i);
            break;
        }
        
        if ((i + 1) % 100 == 0) {
            printf("Inserted %d documents\n", i + 1);
        }
    }
    
    // Test retrieval
    printf("\nTesting retrieval...\n");
    for (int i = 0; i < 10; i++) {
        char doc_id[64];
        snprintf(doc_id, sizeof(doc_id), "doc-%d-%d-%04x", 
                 1749032608, 100000000 + i, i);
        
        uint64_t offset;
        int rc = hash_index_search(index, doc_id, strlen(doc_id), &offset);
        if (rc == 0) {
            printf("Found doc_id='%s' at offset %lu\n", doc_id, offset);
        } else {
            printf("ERROR: Failed to find doc_id='%s'\n", doc_id);
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