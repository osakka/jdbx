#include "src/include/utils/json.h"
#include "src/include/utils/buffer_pool.h"
#include "src/include/utils/logger.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("Testing JSON memory operations with buffer pool...\n");
    
    // Initialize buffer pool
    buffer_pool_init();
    
    // Test 1: Simple object creation and deletion
    printf("Test 1: Simple object creation and deletion\n");
    json_value_t* obj = json_create_object();
    json_object_set(obj, "test_key", json_create_string("test_value"));
    json_object_set(obj, "number", json_create_number(42));
    printf("Created object with 2 fields\n");
    
    json_free(obj);
    printf("Freed object successfully\n");
    
    // Test 2: Nested object creation and deletion
    printf("Test 2: Nested object deletion\n");
    json_value_t* parent = json_create_object();
    json_value_t* child = json_create_object();
    json_object_set(child, "nested_key", json_create_string("nested_value"));
    json_object_set(parent, "child", child);
    printf("Created nested object\n");
    
    json_free(parent);
    printf("Freed nested object successfully\n");
    
    // Test 3: Array creation and deletion
    printf("Test 3: Array creation and deletion\n");
    json_value_t* arr = json_create_array();
    json_array_add(arr, json_create_string("item1"));
    json_array_add(arr, json_create_string("item2"));
    json_array_add(arr, json_create_number(123));
    printf("Created array with 3 items\n");
    
    json_free(arr);
    printf("Freed array successfully\n");
    
    // Test 4: Complex document-like structure
    printf("Test 4: Complex document structure\n");
    json_value_t* doc = json_create_object();
    json_object_set(doc, "_id", json_create_string("doc-1748630209-8335"));
    json_object_set(doc, "username", json_create_string("admin"));
    json_object_set(doc, "role", json_create_string("administrator"));
    
    json_value_t* metadata = json_create_object();
    json_object_set(metadata, "created", json_create_number(1748630209));
    json_object_set(metadata, "active", json_create_boolean(1));
    json_object_set(doc, "metadata", metadata);
    printf("Created complex document\n");
    
    json_free(doc);
    printf("Freed complex document successfully\n");
    
    printf("All tests passed!\n");
    return 0;
}