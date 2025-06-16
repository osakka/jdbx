#include <stdio.h>
#include <string.h>
#include "utils/memory_manager.h"

int main() {
    printf("Test 1: Simple allocation without checkpoint\n");
    memory_manager_init();
    
    void* ptr1 = memory_alloc(100);
    printf("Allocated: %p\n", ptr1);
    strcpy(ptr1, "Hello World");
    printf("Data: %s\n", (char*)ptr1);
    
    memory_free(ptr1);
    printf("Freed successfully\n\n");
    
    printf("Test 2: Allocation with checkpoint\n");
    memory_checkpoint_t* cp = memory_checkpoint_create();
    printf("Created checkpoint: %p\n", cp);
    
    void* ptr2 = memory_alloc(200);
    printf("Allocated: %p\n", ptr2);
    strcpy(ptr2, "Checkpoint allocation");
    printf("Data: %s\n", (char*)ptr2);
    
    memory_checkpoint_commit(cp);
    printf("Committed checkpoint\n");
    
    memory_free(ptr2);
    printf("Freed successfully\n");
    
    return 0;
}