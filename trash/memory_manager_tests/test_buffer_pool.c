#include <stdio.h>
#include "utils/buffer_pool.h"
#include "utils/memory_manager.h"

int main() {
    printf("Initializing memory manager...\n");
    memory_manager_init();
    
    printf("\nTest 1: Simple buffer pool allocation\n");
    void* ptr1 = BUFFER_ALLOC(100);
    printf("BUFFER_ALLOC(100) returned: %p\n", ptr1);
    
    if (ptr1) {
        printf("SUCCESS: Allocation successful\n");
        BUFFER_FREE(ptr1);
        printf("SUCCESS: Free successful\n");
    } else {
        printf("ERROR: Allocation failed!\n");
    }
    
    printf("\nTest 2: BUFFER_CALLOC\n");
    void* ptr2 = BUFFER_CALLOC(1, sizeof(int));
    printf("BUFFER_CALLOC(1, sizeof(int)) returned: %p\n", ptr2);
    
    if (ptr2) {
        printf("SUCCESS: Calloc successful\n");
        BUFFER_FREE(ptr2);
        printf("SUCCESS: Free successful\n");
    } else {
        printf("ERROR: Calloc failed!\n");
    }
    
    return 0;
}