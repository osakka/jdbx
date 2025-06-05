#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "index/hash_index.h"

int main(void) {
    printf("=== Hash Index Debug Test ===\n\n");
    
    /* Create hash index */
    hash_index_t* hash = hash_index_create("/tmp/hash_debug.idx", NULL);
    if (!hash) {
        fprintf(stderr, "Failed to create hash index\n");
        return 1;
    }
    
    printf("Hash index created:\n");
    printf("  Directory size: %u\n", hash->directory_size);
    printf("  Global depth: %u\n", hash->global_depth);
    printf("  Initial buckets: %llu\n", (unsigned long long)atomic_load(&hash->num_buckets));
    printf("\n");
    
    printf("Storage info:\n");
    printf("  Free offset: %llu\n", (unsigned long long)hash->storage->header->free_offset);
    printf("  Index offset: %llu\n", (unsigned long long)hash->storage->header->index_offset);
    printf("  Mapped size: %zu\n", hash->storage->mapped_size);
    printf("\n");
    
    /* Insert documents until failure */
    int i;
    for (i = 0; i < 1000; i++) {
        char key[32];
        snprintf(key, sizeof(key), "doc-%04d", i);
        uint64_t value = i * 1000;
        
        if (i == 527 || i == 528) {
            printf("\nBefore inserting doc %d:\n", i);
            printf("  Free offset: %llu\n", (unsigned long long)hash->storage->header->free_offset);
            printf("  Num buckets: %llu\n", (unsigned long long)atomic_load(&hash->num_buckets));
            printf("  Num keys: %llu\n", (unsigned long long)atomic_load(&hash->num_keys));
        }
        
        int result = hash_index_insert(hash, key, strlen(key), value);
        
        if (i == 527 || i == 528) {
            printf("After inserting doc %d (result=%d):\n", i, result);
            printf("  Free offset: %llu\n", (unsigned long long)hash->storage->header->free_offset);
            printf("  Num buckets: %llu\n", (unsigned long long)atomic_load(&hash->num_buckets));
            printf("  Num keys: %llu\n", (unsigned long long)atomic_load(&hash->num_keys));
        }
        
        if (result != 0) {
            fprintf(stderr, "\nFailed at document %d\n", i);
            break;
        }
    }
    
    printf("\nFinal stats:\n");
    printf("  Inserted: %d documents\n", i);
    printf("  Free offset: %llu\n", (unsigned long long)hash->storage->header->free_offset);
    printf("  Num buckets: %llu\n", (unsigned long long)atomic_load(&hash->num_buckets));
    
    hash_index_destroy(hash);
    return 0;
}