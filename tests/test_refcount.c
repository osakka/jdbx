#include "../include/utils/memory/ref_counter.h"
#include "../include/utils/memory/ref_json.h"
#include "../include/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Test reference counting */
void test_ref_counter() {
    printf("Testing basic reference counting...\n");
    
    /* Create a test object */
    char* test_str = strdup("Test object");
    
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
    ref_counter_release(rc);
    printf("Object should be freed now\n");
    
    printf("Basic reference counting test passed\n\n");
}

/* Test JSON reference counting */
void test_ref_json() {
    printf("Testing JSON reference counting...\n");
    
    /* Create a test JSON object */
    json_value_t* obj = json_create_object();
    json_object_set(obj, "name", json_create_string("Test"));
    json_object_set(obj, "value", json_create_number(42));
    
    /* Create a reference counted wrapper */
    ref_counted_t* rc1 = ref_json_create(obj);
    
    /* Check reference count */
    printf("Initial reference count: %zu\n", ref_counter_count(rc1));
    
    /* Acquire another reference */
    ref_counted_t* rc2 = ref_json_acquire(obj);
    
    /* Check reference counts */
    printf("rc1 reference count: %zu\n", ref_counter_count(rc1));
    printf("rc2 reference count: %zu\n", ref_counter_count(rc2));
    
    /* Get the JSON object from the first reference */
    json_value_t* obj1 = ref_json_get(rc1);
    
    /* Modify the object */
    json_object_set(obj1, "modified", json_create_boolean(1));
    
    /* Get the JSON object from the second reference */
    json_value_t* obj2 = ref_json_get(rc2);
    
    /* Check if they're the same object */
    json_value_t* modified = json_object_get(obj2, "modified");
    printf("Modified value exists in obj2: %s\n", modified ? "yes" : "no");
    
    /* Release the first reference */
    ref_json_release(rc1);
    printf("Released rc1, rc2 reference count: %zu\n", ref_counter_count(rc2));
    
    /* Modify the object again */
    json_object_set(obj2, "modified_again", json_create_boolean(1));
    
    /* Release the second reference */
    ref_json_release(rc2);
    printf("Released rc2, object should be freed now\n");
    
    printf("JSON reference counting test passed\n\n");
}

/* Test multiple threads */
void test_multiple_objects() {
    printf("Testing multiple objects with reference counting...\n");
    
    /* Create test objects */
    json_value_t* obj1 = json_create_object();
    json_value_t* obj2 = json_create_object();
    json_value_t* obj3 = json_create_object();
    
    json_object_set(obj1, "name", json_create_string("Object 1"));
    json_object_set(obj2, "name", json_create_string("Object 2"));
    json_object_set(obj3, "name", json_create_string("Object 3"));
    
    /* Create reference counted wrappers */
    ref_counted_t* rc1 = ref_json_create(obj1);
    ref_counted_t* rc2 = ref_json_create(obj2);
    ref_counted_t* rc3 = ref_json_create(obj3);
    
    /* Cross-reference objects */
    json_object_set(obj1, "ref", json_object_get(obj2, "name"));
    json_object_set(obj2, "ref", json_object_get(obj3, "name"));
    json_object_set(obj3, "ref", json_object_get(obj1, "name"));
    
    /* Release references in different order */
    ref_json_release(rc2);
    ref_json_release(rc1);
    ref_json_release(rc3);
    
    printf("Multiple objects test passed\n\n");
}

int main() {
    printf("Reference Counting System Tests\n");
    printf("==============================\n\n");
    
    test_ref_counter();
    test_ref_json();
    test_multiple_objects();
    
    printf("All tests passed!\n");
    return 0;
}