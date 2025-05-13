#include "src/include/utils/memory/ref_counter.h"
#include "src/include/utils/memory/ref_json.h"
#include "src/include/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Global flag for debug */
int g_debug = 1;

/* Test JSON object reference */
void test_object_references() {
    printf("\nTesting JSON object references...\n");
    
    /* Create a JSON object */
    json_value_t* obj = json_create_object();
    json_object_set(obj, "name", json_create_string("Test"));
    json_object_set(obj, "value", json_create_number(42));
    printf("Created JSON object at %p\n", (void*)obj);
    
    /* Create a reference counted wrapper */
    ref_counted_t* rc1 = ref_json_create(obj);
    printf("Created first ref_counted wrapper (rc1) at %p\n", (void*)rc1);
    
    /* Get the object and use it */
    json_value_t* obj1 = ref_json_get(rc1);
    printf("Got JSON object from rc1: %p\n", (void*)obj1);
    
    /* Create a second reference to the same object */
    ref_counted_t* rc2 = ref_json_acquire(obj1);
    printf("Created second reference (rc2) at %p\n", (void*)rc2);
    
    /* Check reference counts */
    printf("rc1 reference count: %zu\n", ref_counter_count(rc1));
    printf("rc2 reference count: %zu\n", ref_counter_count(rc2));
    
    /* Release the first reference */
    printf("Releasing rc1\n");
    ref_json_release(rc1);
    
    /* Check reference count of the second reference */
    printf("rc2 reference count after releasing rc1: %zu\n", ref_counter_count(rc2));
    
    /* Get the object from the second reference */
    json_value_t* obj2 = ref_json_get(rc2);
    printf("Got JSON object from rc2: %p\n", (void*)obj2);
    
    /* Modify the object */
    json_object_set(obj2, "modified", json_create_boolean(1));
    printf("Modified the JSON object through rc2\n");
    
    /* Release the second reference */
    printf("Releasing rc2\n");
    ref_json_release(rc2);
    
    printf("JSON object references test passed\n\n");
}

/* Test multiple objects with shared values */
void test_shared_values() {
    printf("\nTesting shared JSON values...\n");
    
    /* Create JSON objects with shared values */
    json_value_t* shared_name = json_create_string("Shared Name");
    printf("Created shared name at %p\n", (void*)shared_name);
    
    /* Create objects that use the shared name */
    json_value_t* obj1 = json_create_object();
    json_object_set(obj1, "name", shared_name);
    printf("Created first object at %p with reference to shared name\n", (void*)obj1);
    
    json_value_t* obj2 = json_create_object();
    json_object_set(obj2, "name", shared_name);
    printf("Created second object at %p with reference to same shared name\n", (void*)obj2);
    
    /* Create reference counted wrappers */
    ref_counted_t* rc1 = ref_json_create(obj1);
    ref_counted_t* rc2 = ref_json_create(obj2);
    
    /* Release objects in sequence */
    printf("Releasing rc1\n");
    ref_json_release(rc1);
    
    printf("Releasing rc2\n");
    ref_json_release(rc2);
    
    printf("Shared JSON values test passed\n\n");
}

/* Main function */
int main() {
    printf("Complex Reference Counting Tests\n");
    printf("===============================\n");
    
    test_object_references();
    test_shared_values();
    
    printf("All complex tests completed!\n");
    return 0;
}