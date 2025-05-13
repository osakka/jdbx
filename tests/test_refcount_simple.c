#include "src/include/utils/memory/ref_counter.h"
#include "src/include/utils/memory/ref_json.h"
#include "src/include/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Test JSON object with simple structure */
void test_json_object() {
    printf("\nTesting single JSON object...\n");
    
    /* Create a JSON object */
    json_value_t* obj = json_create_object();
    printf("Created JSON object at %p\n", (void*)obj);
    
    /* Add a string property */
    json_object_set(obj, "name", json_create_string("Test"));
    
    /* Create a reference counted wrapper */
    ref_counted_t* rc = ref_json_create(obj);
    printf("Created reference counted wrapper at %p\n", (void*)rc);
    
    /* Release the reference to free it */
    printf("Releasing reference\n");
    ref_json_release(rc);
    
    printf("Single JSON object test completed\n\n");
}

/* Main function */
int main() {
    printf("Simple Reference Counting Test\n");
    printf("=============================\n");
    
    test_json_object();
    
    printf("Test completed!\n");
    return 0;
}