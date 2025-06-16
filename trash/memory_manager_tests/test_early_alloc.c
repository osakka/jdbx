#include <stdio.h>
#include "utils/memory_manager.h"
#include "utils/buffer_pool.h"

int main() {
    printf("Test 1: Allocation before memory manager init\n");
    void* ptr1 = BUFFER_ALLOC(100);
    printf("Allocated: %p\n", ptr1);
    
    printf("\nInitializing memory manager\n");
    memory_manager_init();
    
    printf("\nTest 2: Allocation after memory manager init\n");
    void* ptr2 = BUFFER_ALLOC(100);
    printf("Allocated: %p\n", ptr2);
    
    printf("\nFreeing allocations\n");
    BUFFER_FREE(ptr1);
    printf("Freed ptr1\n");
    
    BUFFER_FREE(ptr2);
    printf("Freed ptr2\n");
    
    return 0;
}