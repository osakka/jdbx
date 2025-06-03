#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "index/btree_disk.h"
#include "index/hash_index.h"

int main(void) {
    printf("=== Testing B+Tree and Hash Indexes ===\n\n");
    
    /* Test B+Tree */
    printf("1. Testing B+Tree Index:\n");
    btree_disk_t* btree = btree_disk_create("/tmp/test.btree", 50, NULL);
    if (!btree) {
        fprintf(stderr, "Failed to create B+tree\n");
        return 1;
    }
    
    /* Insert some keys */
    for (int i = 0; i < 100; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key%03d", i);
        
        if (btree_disk_insert(btree, key, strlen(key), i * 1000) != 0) {
            fprintf(stderr, "Failed to insert %s\n", key);
        }
    }
    
    /* Flush buffer */
    printf("   Flushing write buffer...\n");
    btree_disk_flush_buffer(btree);
    
    /* Search for keys */
    printf("   Searching for keys...\n");
    int found = 0;
    for (int i = 0; i < 100; i += 10) {
        char key[32];
        snprintf(key, sizeof(key), "key%03d", i);
        
        uint64_t value;
        if (btree_disk_search(btree, key, strlen(key), &value) == 0) {
            printf("   Found %s = %llu\n", key, (unsigned long long)value);
            found++;
        }
    }
    printf("   Found %d/10 keys\n\n", found);
    
    /* Test Hash Index */
    printf("2. Testing Hash Index:\n");
    hash_index_t* hash = hash_index_create("/tmp/test.hash", NULL);
    if (!hash) {
        fprintf(stderr, "Failed to create hash index\n");
        btree_disk_destroy(btree);
        return 1;
    }
    
    /* Insert same keys */
    for (int i = 0; i < 100; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key%03d", i);
        
        if (hash_index_insert(hash, key, strlen(key), i * 1000) != 0) {
            fprintf(stderr, "Failed to insert %s\n", key);
        }
    }
    
    /* Search for keys */
    printf("   Searching for keys...\n");
    found = 0;
    for (int i = 0; i < 100; i += 10) {
        char key[32];
        snprintf(key, sizeof(key), "key%03d", i);
        
        uint64_t value;
        if (hash_index_search(hash, key, strlen(key), &value) == 0) {
            printf("   Found %s = %llu\n", key, (unsigned long long)value);
            found++;
        }
    }
    printf("   Found %d/10 keys\n\n", found);
    
    /* Performance comparison */
    printf("3. Performance Test (1000 lookups):\n");
    
    /* B+Tree lookups */
    clock_t start = clock();
    for (int i = 0; i < 1000; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key%03d", rand() % 100);
        uint64_t value;
        btree_disk_search(btree, key, strlen(key), &value);
    }
    double btree_time = (double)(clock() - start) / CLOCKS_PER_SEC;
    
    /* Hash lookups */
    start = clock();
    for (int i = 0; i < 1000; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key%03d", rand() % 100);
        uint64_t value;
        hash_index_search(hash, key, strlen(key), &value);
    }
    double hash_time = (double)(clock() - start) / CLOCKS_PER_SEC;
    
    printf("   B+Tree: %.3f seconds\n", btree_time);
    printf("   Hash:   %.3f seconds\n", hash_time);
    printf("   Hash is %.1fx faster\n\n", btree_time / hash_time);
    
    /* Cleanup */
    btree_disk_destroy(btree);
    hash_index_destroy(hash);
    
    printf("Test completed successfully!\n");
    return 0;
}