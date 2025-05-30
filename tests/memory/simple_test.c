#include <stdio.h>
#include <stdlib.h>
#include "utils/json.h"
#include "utils/buffer_pool.h"

int main() {
    printf("Testing basic JSON functionality...\n");
    
    // Test 1: Simple JSON creation
    json_value_t* obj = json_create_object();
    if (!obj) {
        printf("Failed to create JSON object\n");
        return 1;
    }
    
    json_object_set(obj, "test", json_create_string("hello"));
    printf("Created JSON object with test field\n");
    
    // Test 2: Deep copy
    json_value_t* copy = json_deep_copy(obj);
    if (!copy) {
        printf("Failed to deep copy JSON object\n");
        json_free(obj);
        return 1;
    }
    printf("Successfully deep copied JSON object\n");
    
    // Test 3: Buffer pool
    void* ptr = buffer_pool_alloc(1024);
    if (!ptr) {
        printf("Failed to allocate from buffer pool\n");
        json_free(obj);
        json_free(copy);
        return 1;
    }
    printf("Successfully allocated from buffer pool\n");
    
    // Cleanup
    buffer_pool_free(ptr);
    json_free(obj);
    json_free(copy);
    
    printf("All tests passed!\n");
    return 0;
}