#include "src/include/utils/memory/ref_counter.h"
#include "src/include/utils/memory/ref_json.h"
#include "src/include/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Test shared JSON values */
void test_shared_value() {
    printf("\nTesting shared JSON value...\n");
    
    /* Create a shared string that will be used in multiple objects */
    json_value_t* shared_string = json_create_string("Shared String");
    printf("Created shared string at %p\n", (void*)shared_string);
    
    /* Create first object that uses shared string */
    json_value_t* obj1 = json_create_object();
    json_object_set(obj1, "shared", shared_string); /* This reuses the shared_string */
    printf("Created first object at %p with reference to shared string\n", (void*)obj1);
    
    /* Create reference counted wrapper for first object */
    ref_counted_t* rc1 = ref_json_create(obj1);
    printf("Created reference counted wrapper for first object at %p\n", (void*)rc1);
    
    /* Create second object that uses the same shared string */
    json_value_t* obj2 = json_create_object();
    json_object_set(obj2, "shared", shared_string); /* This reuses the shared_string */
    printf("Created second object at %p with reference to shared string\n", (void*)obj2);
    
    /* Create reference counted wrapper for second object */
    ref_counted_t* rc2 = ref_json_create(obj2);
    printf("Created reference counted wrapper for second object at %p\n", (void*)rc2);
    
    /* Release the first reference */
    printf("Releasing first reference\n");
    ref_json_release(rc1);
    
    printf("First reference released\n");
    
    /* Release the second reference */
    printf("Releasing second reference\n");
    ref_json_release(rc2);
    
    printf("Shared JSON value test completed\n\n");
}

/* Main function */
int main() {
    printf("Shared Reference Counting Test\n");
    printf("============================\n");
    
    test_shared_value();
    
    printf("Test completed!\n");
    return 0;
}