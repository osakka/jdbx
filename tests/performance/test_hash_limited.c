#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "index/hash_index.h"

int main(void) {
    printf("=== Testing Hash Index with Limited Storage ===\n\n");
    
    /* Remove old test file */
    remove("/tmp/hash_limited.idx");
    
    /* Create hash index with smaller initial size to trigger the issue faster */
    printf("1. Creating Hash Index...\n");
    hash_index_t* hash = hash_index_create("/tmp/hash_limited.idx", NULL);
    if (!hash) {
        fprintf(stderr, "Failed to create hash index\n");
        return 1;
    }
    printf("   SUCCESS: Hash index created\n");
    printf("   Initial storage size: %zu bytes\n", hash->storage->mapped_size);
    printf("   Bucket size: %d bytes\n", HASH_BUCKET_SIZE);
    printf("   Max buckets in storage: %zu\n", hash->storage->mapped_size / HASH_BUCKET_SIZE);
    
    /* Monitor free offset as we insert */
    printf("\n2. Inserting documents and monitoring storage usage...\n");
    
    int i;
    for (i = 0; i < 10000; i++) {
        char key[64];
        snprintf(key, sizeof(key), "document-%08d", i);
        uint64_t value = i * 1000;
        
        /* Check storage usage periodically */
        if (i % 100 == 0) {
            uint64_t used = hash->storage->header->free_offset;
            uint64_t total = hash->storage->mapped_size;
            double percent = (double)used / total * 100.0;
            printf("   Doc %d: Storage used: %llu / %llu bytes (%.1f%%)\n", 
                   i, (unsigned long long)used, (unsigned long long)total, percent);
        }
        
        if (hash_index_insert(hash, key, strlen(key), value) != 0) {
            fprintf(stderr, "\n   FAIL: Could not insert document #%d\n", i);
            
            /* Print final storage state */
            uint64_t used = hash->storage->header->free_offset;
            uint64_t total = hash->storage->mapped_size;
            double percent = (double)used / total * 100.0;
            fprintf(stderr, "   Final storage: %llu / %llu bytes (%.1f%%)\n", 
                   (unsigned long long)used, (unsigned long long)total, percent);
            fprintf(stderr, "   Number of buckets: %llu\n", 
                   (unsigned long long)atomic_load(&hash->num_buckets));
            break;
        }
    }
    
    printf("\n   Successfully inserted %d documents\n", i);
    
    /* Final statistics */
    printf("\n3. Final Hash Index Statistics:\n");
    uint64_t num_keys, num_buckets, avg_chain_length;
    hash_index_stats(hash, &num_keys, &num_buckets, &avg_chain_length);
    printf("   Keys: %llu\n", (unsigned long long)num_keys);
    printf("   Buckets: %llu\n", (unsigned long long)num_buckets);
    printf("   Avg chain length: %llu\n", (unsigned long long)avg_chain_length);
    
    /* Clean up */
    hash_index_destroy(hash);
    
    return 0;
}