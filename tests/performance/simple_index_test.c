#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("=== Simple Index Component Test ===\n\n");
    
    /* First, test just the storage layer which we know works */
    printf("1. Testing mmap storage (known working)...\n");
    
#ifdef TEST_STORAGE
    #include "storage/mmap_storage.h"
    
    mmap_storage_t* storage = mmap_storage_create("/tmp/simple_test.mmap", 1024 * 1024);
    if (!storage) {
        fprintf(stderr, "FAIL: Could not create storage\n");
        return 1;
    }
    printf("   SUCCESS: Storage created\n");
    
    const char* key = "test";
    const char* value = "value";
    
    if (mmap_storage_put(storage, key, strlen(key), value, strlen(value)) == 0) {
        printf("   SUCCESS: Put operation\n");
    } else {
        printf("   FAIL: Put operation\n");
    }
    
    void* retrieved;
    size_t len;
    if (mmap_storage_get(storage, key, strlen(key), &retrieved, &len) == 0) {
        printf("   SUCCESS: Get operation - retrieved: %.*s\n", (int)len, (char*)retrieved);
        free(retrieved);
    } else {
        printf("   FAIL: Get operation\n");
    }
    
    mmap_storage_destroy(storage);
    printf("   SUCCESS: Storage destroyed\n\n");
#endif

    /* Now test generic cache independently */
    printf("2. Testing generic cache...\n");
    
#ifdef TEST_CACHE
    #include "utils/generic_cache.h"
    
    generic_cache_t* cache = generic_cache_create(10);
    if (!cache) {
        fprintf(stderr, "FAIL: Could not create cache\n");
        return 1;
    }
    printf("   SUCCESS: Cache created with capacity 10\n");
    
    int key_int = 42;
    int value_int = 100;
    
    if (generic_cache_put(cache, &key_int, sizeof(int), &value_int, sizeof(int)) == 0) {
        printf("   SUCCESS: Cache put\n");
    } else {
        printf("   FAIL: Cache put\n");
    }
    
    int* cached = (int*)generic_cache_get(cache, &key_int, sizeof(int));
    if (cached && *cached == value_int) {
        printf("   SUCCESS: Cache get - value: %d\n", *cached);
        free(cached);
    } else {
        printf("   FAIL: Cache get\n");
    }
    
    generic_cache_destroy(cache);
    printf("   SUCCESS: Cache destroyed\n\n");
#endif

    /* Test skip list independently */
    printf("3. Testing skip list...\n");
    
#ifdef TEST_SKIPLIST
    #include "utils/skiplist.h"
    #include "utils/hazard_pointer.h"
    
    skiplist_t* skiplist = skiplist_create(NULL);
    if (!skiplist) {
        fprintf(stderr, "FAIL: Could not create skiplist\n");
        return 1;
    }
    printf("   SUCCESS: Skiplist created\n");
    
    const char* sl_key = "skipkey";
    const char* sl_value = "skipvalue";
    
    if (skiplist_insert(skiplist, sl_key, strlen(sl_key), sl_value, strlen(sl_value))) {
        printf("   SUCCESS: Skiplist insert\n");
    } else {
        printf("   FAIL: Skiplist insert\n");
    }
    
    size_t sl_len;
    void* sl_result = skiplist_search(skiplist, sl_key, strlen(sl_key), &sl_len);
    if (sl_result) {
        printf("   SUCCESS: Skiplist search - value: %.*s\n", (int)sl_len, (char*)sl_result);
        free(sl_result);
    } else {
        printf("   FAIL: Skiplist search\n");
    }
    
    skiplist_destroy(skiplist);
    printf("   SUCCESS: Skiplist destroyed\n\n");
#endif

    printf("All component tests completed.\n");
    printf("\nTo test individual components, compile with:\n");
    printf("  -DTEST_STORAGE   for mmap storage\n");
    printf("  -DTEST_CACHE     for generic cache\n");
    printf("  -DTEST_SKIPLIST  for skip list\n");
    
    return 0;
}