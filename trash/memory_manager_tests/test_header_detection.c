#include <stdio.h>
#include <stdint.h>
#include "utils/memory_manager.h"
#include "utils/buffer_pool.h"

int main() {
    printf("Initializing memory manager\n");
    memory_manager_init();
    
    printf("\nAllocating 100 bytes\n");
    void* ptr = BUFFER_ALLOC(100);
    printf("Allocated at: %p\n", ptr);
    
    // Try to manually check what's before the pointer
    uint32_t* magic_ptr = (uint32_t*)((char*)ptr - sizeof(void*) * 5 - sizeof(uint32_t));
    printf("Checking for magic at %p: 0x%x (expected 0xDEADBEEF)\n", magic_ptr, *magic_ptr);
    
    printf("\nFreeing allocation\n");
    BUFFER_FREE(ptr);
    printf("Freed successfully\n");
    
    return 0;
}