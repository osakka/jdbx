#include "src/include/utils/memory/ref_counter.h"
#include "src/include/utils/memory/ref_json.h"
#include "src/include/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Test basic reference counting */
void test_ref_counter() {
    printf("\nTesting basic reference counting...\n");
    
    /* Create a test object */
    char* test_str = strdup("Test object");
    printf("Created test string at %p\n", (void*)test_str);
    
    /* Create a reference counted wrapper */
    ref_counted_t* rc = ref_counter_create(test_str, free);
    
    /* Check reference count */
    printf("Initial reference count: %zu\n", ref_counter_count(rc));
    
    /* Acquire a reference */
    ref_counter_acquire(rc);
    printf("Reference count after acquire: %zu\n", ref_counter_count(rc));
    
    /* Release a reference */
    ref_counter_release(rc);
    printf("Reference count after release: %zu\n", ref_counter_count(rc));
    
    /* Get the object */
    char* str = (char*)ref_counter_get(rc);
    printf("Object value: %s\n", str);
    
    /* Release the final reference */
    printf("Releasing final reference\n");
    ref_counter_release(rc);
    printf("Basic reference counting test passed\n\n");
}

/* Test a single JSON object with reference counting */
void test_simple_json() {
    printf("\nTesting simple JSON reference counting...\n");
    
    /* Create a test JSON string */
    json_value_t* obj = json_create_string("Test");
    printf("Created JSON string at %p\n", (void*)obj);
    
    /* Create a reference counted wrapper */
    ref_counted_t* rc = ref_json_create(obj);
    printf("Created ref_counted wrapper at %p\n", (void*)rc);
    
    /* Check reference count */
    printf("Initial reference count: %zu\n", ref_counter_count(rc));
    
    /* Release the reference */
    printf("Releasing reference\n");
    ref_json_release(rc);
    printf("Simple JSON reference counting test passed\n\n");
}

/* Main function */
int main() {
    printf("Reference Counting Debug Tests\n");
    printf("==============================\n");
    
    test_ref_counter();
    test_simple_json();
    
    printf("All tests completed!\n");
    return 0;
}