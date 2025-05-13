#include "src/include/utils/memory/ref_counter.h"
#include "src/include/utils/memory/ref_json.h"
#include "src/include/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Simple test for reference counting
 */

int main(int argc, char** argv) {
    printf("Simple Reference Counting Test with JavaScript Objects\n");
    printf("===================================================\n\n");
    
    /* Create a string and a wrapper object */
    printf("Creating a string for the 'name' property...\n");
    json_value_t* name = json_create_string("John Doe");
    printf("Created string at %p\n", (void*)name);
    
    /* Create a nested object with this string */
    printf("Creating a nested object that uses the string...\n");
    json_value_t* person = json_create_object();
    json_object_set(person, "name", name);  /* This should use the same string */
    json_object_set(person, "age", json_create_number(30));
    printf("Created person object at %p\n", (void*)person);
    
    /* Create a second object that uses the same string */
    printf("Creating a second object that uses the same string...\n");
    json_value_t* employee = json_create_object();
    json_object_set(employee, "name", name);  /* This should use the same string */
    json_object_set(employee, "department", json_create_string("Engineering"));
    printf("Created employee object at %p\n", (void*)employee);
    
    /* Create reference counted wrappers */
    printf("Creating reference counting wrappers...\n");
    ref_counted_t* rc_person = ref_json_create(person);
    printf("Created person wrapper at %p with count %zu\n", (void*)rc_person, ref_counter_count(rc_person));
    
    ref_counted_t* rc_employee = ref_json_create(employee);
    printf("Created employee wrapper at %p with count %zu\n", (void*)rc_employee, ref_counter_count(rc_employee));
    
    /* Release the reference to the person object */
    printf("\nReleasing person object reference...\n");
    ref_json_release(rc_person);
    
    /* Release the reference to the employee object */
    printf("Releasing employee object reference...\n");
    ref_json_release(rc_employee);
    
    printf("\nTest completed successfully\n");
    return 0;
}