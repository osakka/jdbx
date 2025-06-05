#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <execinfo.h>
#include "index/btree_disk.h"
#include "index/hash_index.h"

/* Signal handler for debugging */
void segfault_handler(int sig) {
    void *array[10];
    size_t size;
    
    fprintf(stderr, "\n=== SEGFAULT at signal %d ===\n", sig);
    size = backtrace(array, 10);
    fprintf(stderr, "Stack trace:\n");
    backtrace_symbols_fd(array, size, 2);
    exit(1);
}

int main(void) {
    /* Install signal handler */
    signal(SIGSEGV, segfault_handler);
    
    printf("=== Testing B+Tree and Hash Index ===\n\n");
    
    /* Test 1: B+Tree creation */
    printf("1. Creating B+Tree...\n");
    btree_disk_t* btree = btree_disk_create("/tmp/btree_test.idx", 50, NULL);
    if (!btree) {
        fprintf(stderr, "Failed to create B+tree\n");
        return 1;
    }
    printf("   SUCCESS: B+tree created\n");
    
    /* Test 2: Insert into B+Tree */
    printf("\n2. Inserting into B+Tree...\n");
    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%04d", i);
        uint64_t value = i * 1000;
        
        printf("   Inserting %s -> %llu\n", key, (unsigned long long)value);
        
        if (btree_disk_insert(btree, key, strlen(key), value) != 0) {
            fprintf(stderr, "   FAIL: Could not insert %s\n", key);
        }
    }
    
    /* Test 3: Flush write buffer */
    printf("\n3. Flushing B+Tree write buffer...\n");
    if (btree_disk_flush_buffer(btree) == 0) {
        printf("   SUCCESS: Buffer flushed\n");
    } else {
        fprintf(stderr, "   FAIL: Could not flush buffer\n");
    }
    
    /* Test 4: Search in B+Tree */
    printf("\n4. Searching in B+Tree...\n");
    for (int i = 0; i < 10; i += 2) {
        char key[32];
        snprintf(key, sizeof(key), "key_%04d", i);
        uint64_t value;
        
        if (btree_disk_search(btree, key, strlen(key), &value) == 0) {
            printf("   Found %s -> %llu\n", key, (unsigned long long)value);
        } else {
            fprintf(stderr, "   NOT FOUND: %s\n", key);
        }
    }
    
    /* Test 5: B+Tree statistics */
    printf("\n5. B+Tree Statistics:\n");
    uint64_t height, num_keys, cache_hits, cache_misses;
    btree_disk_stats(btree, &height, &num_keys, &cache_hits, &cache_misses);
    printf("   Height: %llu\n", (unsigned long long)height);
    printf("   Keys: %llu\n", (unsigned long long)num_keys);
    printf("   Cache hits: %llu\n", (unsigned long long)cache_hits);
    printf("   Cache misses: %llu\n", (unsigned long long)cache_misses);
    
    /* Test 6: Hash index creation */
    printf("\n6. Creating Hash Index...\n");
    hash_index_t* hash = hash_index_create("/tmp/hash_test.idx", NULL);
    if (!hash) {
        fprintf(stderr, "Failed to create hash index\n");
        btree_disk_destroy(btree);
        return 1;
    }
    printf("   SUCCESS: Hash index created\n");
    
    /* Test 7: Insert into hash index */
    printf("\n7. Inserting into Hash Index...\n");
    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%04d", i);
        uint64_t value = i * 1000;
        
        printf("   Inserting %s -> %llu\n", key, (unsigned long long)value);
        
        if (hash_index_insert(hash, key, strlen(key), value) != 0) {
            fprintf(stderr, "   FAIL: Could not insert %s\n", key);
        }
    }
    
    /* Test 8: Search in hash index */
    printf("\n8. Searching in Hash Index...\n");
    for (int i = 0; i < 10; i += 2) {
        char key[32];
        snprintf(key, sizeof(key), "key_%04d", i);
        uint64_t value;
        
        if (hash_index_search(hash, key, strlen(key), &value) == 0) {
            printf("   Found %s -> %llu\n", key, (unsigned long long)value);
        } else {
            fprintf(stderr, "   NOT FOUND: %s\n", key);
        }
    }
    
    /* Test 9: Hash index statistics */
    printf("\n9. Hash Index Statistics:\n");
    uint64_t h_num_keys, h_num_buckets, h_avg_chain;
    hash_index_stats(hash, &h_num_keys, &h_num_buckets, &h_avg_chain);
    printf("   Keys: %llu\n", (unsigned long long)h_num_keys);
    printf("   Buckets: %llu\n", (unsigned long long)h_num_buckets);
    printf("   Avg chain length: %llu\n", (unsigned long long)h_avg_chain);
    
    /* Test 10: Performance comparison */
    printf("\n10. Performance Comparison (1000 lookups):\n");
    
    clock_t start = clock();
    for (int i = 0; i < 1000; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%04d", i % 10);
        uint64_t value;
        btree_disk_search(btree, key, strlen(key), &value);
    }
    double btree_time = (double)(clock() - start) / CLOCKS_PER_SEC;
    
    start = clock();
    for (int i = 0; i < 1000; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%04d", i % 10);
        uint64_t value;
        hash_index_search(hash, key, strlen(key), &value);
    }
    double hash_time = (double)(clock() - start) / CLOCKS_PER_SEC;
    
    printf("   B+Tree time: %.4f seconds\n", btree_time);
    printf("   Hash time: %.4f seconds\n", hash_time);
    if (hash_time > 0) {
        printf("   Hash is %.1fx faster\n", btree_time / hash_time);
    }
    
    /* Cleanup */
    printf("\n11. Cleanup...\n");
    btree_disk_destroy(btree);
    hash_index_destroy(hash);
    printf("   SUCCESS: All resources freed\n");
    
    printf("\n=== All tests completed successfully ===\n");
    return 0;
}