#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "storage/mmap_storage.h"

int main(void) {
    printf("Testing mmap storage creation...\n");
    
    mmap_storage_t* storage = mmap_storage_create("/tmp/debug.mmap", 1024 * 1024);
    if (!storage) {
        fprintf(stderr, "Failed to create storage\n");
        return 1;
    }
    
    printf("Storage created:\n");
    printf("  File size: %zu\n", storage->file_size);
    printf("  Mapped size: %zu\n", storage->mapped_size);
    printf("  Base addr: %p\n", storage->base_addr);
    
    if (storage->header) {
        printf("  Header magic: 0x%08X (expected 0x%08X)\n", 
               storage->header->magic, MMAP_MAGIC);
        printf("  Header version: %u\n", storage->header->version);
        printf("  Doc count: %llu\n", (unsigned long long)storage->header->doc_count);
    } else {
        printf("  Header is NULL!\n");
    }
    
    /* Try a simple put/get */
    printf("\nTesting put/get...\n");
    
    const char* key = "test_key";
    const char* value = "test_value";
    
    if (mmap_storage_put(storage, key, strlen(key), value, strlen(value)) == 0) {
        printf("Put successful\n");
        
        void* retrieved;
        size_t retrieved_len;
        
        if (mmap_storage_get(storage, key, strlen(key), &retrieved, &retrieved_len) == 0) {
            printf("Get successful: %.*s\n", (int)retrieved_len, (char*)retrieved);
            free(retrieved);
        } else {
            printf("Get failed\n");
        }
    } else {
        printf("Put failed\n");
    }
    
    mmap_storage_destroy(storage);
    printf("\nTest completed\n");
    
    return 0;
}