#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "storage/mmap_storage.h"

int main(void) {
    printf("Creating mmap storage...\n");
    
    mmap_storage_t* storage = mmap_storage_create("/tmp/test.mmap", 1024 * 1024);
    if (!storage) {
        fprintf(stderr, "Failed to create storage\n");
        return 1;
    }
    
    printf("Storage created successfully\n");
    
    /* Insert a few documents */
    for (int i = 0; i < 10; i++) {
        char key[32];
        char value[128];
        
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "This is value %d", i);
        
        printf("Inserting %s...\n", key);
        
        if (mmap_storage_put(storage, key, strlen(key), value, strlen(value)) != 0) {
            fprintf(stderr, "Failed to insert %s\n", key);
        }
    }
    
    printf("\nReading back values:\n");
    
    /* Read them back */
    for (int i = 0; i < 10; i++) {
        char key[32];
        void* value;
        size_t value_len;
        
        snprintf(key, sizeof(key), "key_%d", i);
        
        if (mmap_storage_get(storage, key, strlen(key), &value, &value_len) == 0) {
            printf("%s = %.*s\n", key, (int)value_len, (char*)value);
            free(value);
        } else {
            printf("%s = NOT FOUND\n", key);
        }
    }
    
    printf("\nStorage stats:\n");
    printf("Documents: %llu\n", (unsigned long long)storage->header->doc_count);
    printf("File size: %zu bytes\n", storage->file_size);
    
    mmap_storage_destroy(storage);
    
    return 0;
}