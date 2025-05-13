#include "src/include/utils/memory/ref_counter.h"
#include "src/include/utils/memory/ref_json.h"
#include "src/include/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Test nested JSON objects */
void test_nested_json() {
    printf("\nTesting nested JSON objects...\n");
    
    /* Create nested objects */
    json_value_t* inner = json_create_object();
    json_object_set(inner, "inner_key", json_create_string("Inner Value"));
    printf("Created inner object at %p\n", (void*)inner);
    
    /* Create outer object with a reference to inner */
    json_value_t* outer = json_create_object();
    json_object_set(outer, "name", json_create_string("Outer Object"));
    json_object_set(outer, "inner", inner);  /* This doesn't clone inner, just stores a reference */
    printf("Created outer object at %p with reference to inner object\n", (void*)outer);
    
    /* Wrap the outer object with reference counting */
    ref_counted_t* rc_outer = ref_json_create(outer);
    printf("Created reference counted wrapper for outer at %p\n", (void*)rc_outer);
    
    /* Release the outer reference (which should free both outer and inner) */
    printf("Releasing outer reference\n");
    ref_json_release(rc_outer);
    
    printf("Nested JSON objects test completed\n\n");
}

/* Main function */
int main() {
    printf("Nested Reference Counting Test\n");
    printf("=============================\n");
    
    test_nested_json();
    
    printf("Test completed!\n");
    return 0;
}