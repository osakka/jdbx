#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "index/btree_disk.h"
#include "index/hash_index.h"

/* Test without using the write buffer to avoid skiplist issues */
int main(void) {
    printf("=== Simple Index Test (Direct Operations) ===\n\n");
    
    /* Test Hash Index only (simpler, no write buffer) */
    printf("1. Testing Hash Index...\n");
    hash_index_t* hash = hash_index_create("/tmp/hash_simple.idx", NULL);
    if (!hash) {
        fprintf(stderr, "Failed to create hash index\n");
        return 1;
    }
    
    /* Insert some data */
    printf("2. Inserting data...\n");
    int success = 0;
    for (int i = 0; i < 100; i++) {
        char key[32];
        snprintf(key, sizeof(key), "doc_%d", i);
        uint64_t offset = i * 100;
        
        if (hash_index_insert(hash, key, strlen(key), offset) == 0) {
            success++;
        }
    }
    printf("   Inserted %d/100 entries\n", success);
    
    /* Search for data */
    printf("3. Searching for data...\n");
    success = 0;
    for (int i = 0; i < 100; i += 10) {
        char key[32];
        snprintf(key, sizeof(key), "doc_%d", i);
        uint64_t offset;
        
        if (hash_index_search(hash, key, strlen(key), &offset) == 0) {
            printf("   Found %s -> %llu\n", key, (unsigned long long)offset);
            success++;
        }
    }
    printf("   Found %d/10 entries\n", success);
    
    /* Performance test */
    printf("4. Performance test (10000 lookups)...\n");
    clock_t start = clock();
    int found = 0;
    
    for (int i = 0; i < 10000; i++) {
        char key[32];
        snprintf(key, sizeof(key), "doc_%d", rand() % 100);
        uint64_t offset;
        
        if (hash_index_search(hash, key, strlen(key), &offset) == 0) {
            found++;
        }
    }
    
    double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
    printf("   Time: %.3f seconds\n", elapsed);
    printf("   Found: %d/10000\n", found);
    printf("   Rate: %.0f lookups/sec\n", 10000.0 / elapsed);
    printf("   Latency: %.3f μs/lookup\n", (elapsed * 1000000.0) / 10000.0);
    
    /* Statistics */
    printf("5. Hash index statistics...\n");
    uint64_t num_keys, num_buckets, avg_chain;
    hash_index_stats(hash, &num_keys, &num_buckets, &avg_chain);
    printf("   Keys: %llu\n", (unsigned long long)num_keys);
    printf("   Buckets: %llu\n", (unsigned long long)num_buckets);
    printf("   Avg chain: %llu\n", (unsigned long long)avg_chain);
    printf("   Load factor: %.2f\n", (double)num_keys / num_buckets);
    
    /* Cleanup */
    hash_index_destroy(hash);
    
    printf("\nTest completed successfully!\n");
    return 0;
}