#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Include all headers at the top */
#include "storage/mmap_storage.h"
#include "utils/generic_cache.h"
#include "utils/skiplist.h"
#include "utils/hazard_pointer.h"

/* Test mmap storage */
int test_storage() {
    printf("\n=== Testing mmap storage ===\n");
    
    mmap_storage_t* storage = mmap_storage_create("/tmp/test_storage.mmap", 10 * 1024 * 1024);
    if (!storage) {
        fprintf(stderr, "Failed to create storage\n");
        return -1;
    }
    
    /* Test basic operations */
    int num_tests = 100;
    int success = 0;
    
    /* Insert test */
    for (int i = 0; i < num_tests; i++) {
        char key[32], value[128];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "This is value number %d", i);
        
        if (mmap_storage_put(storage, key, strlen(key), value, strlen(value)) == 0) {
            success++;
        }
    }
    printf("Insert: %d/%d successful\n", success, num_tests);
    
    /* Retrieve test */
    success = 0;
    for (int i = 0; i < num_tests; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        
        void* value;
        size_t value_len;
        if (mmap_storage_get(storage, key, strlen(key), &value, &value_len) == 0) {
            success++;
            free(value);
        }
    }
    printf("Retrieve: %d/%d successful\n", success, num_tests);
    
    mmap_storage_destroy(storage);
    printf("Storage test completed\n");
    return 0;
}

/* Test generic cache */
int test_cache() {
    printf("\n=== Testing generic cache ===\n");
    
    generic_cache_t* cache = generic_cache_create(50);
    if (!cache) {
        fprintf(stderr, "Failed to create cache\n");
        return -1;
    }
    
    /* Test with integer keys */
    int success = 0;
    for (int i = 0; i < 100; i++) {
        int value = i * 100;
        if (generic_cache_put(cache, &i, sizeof(int), &value, sizeof(int)) == 0) {
            success++;
        }
    }
    printf("Cache put: %d/100 successful\n", success);
    
    /* Test retrieval and LRU eviction */
    success = 0;
    int evicted = 0;
    for (int i = 0; i < 100; i++) {
        int* value = (int*)generic_cache_get(cache, &i, sizeof(int));
        if (value) {
            if (*value == i * 100) {
                success++;
            }
            free(value);
        } else if (i < 50) {
            evicted++; /* First 50 should be evicted */
        }
    }
    printf("Cache get: %d successful, %d evicted (expected ~50)\n", success, evicted);
    
    uint64_t hits, misses, evictions;
    generic_cache_stats(cache, &hits, &misses, &evictions);
    printf("Cache stats: hits=%llu, misses=%llu, evictions=%llu\n",
           (unsigned long long)hits, (unsigned long long)misses, 
           (unsigned long long)evictions);
    
    generic_cache_destroy(cache);
    printf("Cache test completed\n");
    return 0;
}

/* Test skip list */
int test_skiplist() {
    printf("\n=== Testing skip list ===\n");
    
    skiplist_t* list = skiplist_create(NULL);
    if (!list) {
        fprintf(stderr, "Failed to create skiplist\n");
        return -1;
    }
    
    /* Insert test */
    int num_items = 1000;
    int success = 0;
    
    clock_t start = clock();
    for (int i = 0; i < num_items; i++) {
        char key[32], value[64];
        snprintf(key, sizeof(key), "item_%04d", i);
        snprintf(value, sizeof(value), "value_%04d", i);
        
        if (skiplist_insert(list, key, strlen(key), value, strlen(value))) {
            success++;
        }
    }
    double insert_time = (double)(clock() - start) / CLOCKS_PER_SEC;
    printf("Insert: %d/%d successful, time=%.3fs\n", success, num_items, insert_time);
    
    /* Search test */
    success = 0;
    start = clock();
    for (int i = 0; i < num_items; i++) {
        char key[32];
        snprintf(key, sizeof(key), "item_%04d", i);
        
        size_t value_len;
        void* value = skiplist_search(list, key, strlen(key), &value_len);
        if (value) {
            success++;
            free(value);
        }
    }
    double search_time = (double)(clock() - start) / CLOCKS_PER_SEC;
    printf("Search: %d/%d successful, time=%.3fs\n", success, num_items, search_time);
    
    printf("Size: %zu\n", skiplist_size(list));
    
    uint64_t inserts, deletes, searches;
    skiplist_stats(list, &inserts, &deletes, &searches);
    printf("Skiplist stats: inserts=%llu, deletes=%llu, searches=%llu\n",
           (unsigned long long)inserts, (unsigned long long)deletes, 
           (unsigned long long)searches);
    
    skiplist_destroy(list);
    printf("Skiplist test completed\n");
    return 0;
}

int main(void) {
    printf("=== Component Testing Suite ===\n");
    
    /* Test each component */
    if (test_storage() != 0) {
        fprintf(stderr, "Storage test failed\n");
    }
    
    if (test_cache() != 0) {
        fprintf(stderr, "Cache test failed\n");
    }
    
    if (test_skiplist() != 0) {
        fprintf(stderr, "Skiplist test failed\n");
    }
    
    printf("\n=== All component tests completed ===\n");
    return 0;
}