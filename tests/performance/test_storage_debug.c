#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "storage/mmap_storage.h"

int main(void) {
    printf("=== Debugging Storage Layout ===\n\n");
    
    /* Create storage */
    mmap_storage_t* storage = mmap_storage_create("/tmp/storage_debug.idx", 256 * 1024 * 1024);
    if (!storage) {
        fprintf(stderr, "Failed to create storage\n");
        return 1;
    }
    
    printf("Storage created:\n");
    printf("  File size: %zu bytes (%.1f MB)\n", storage->file_size, storage->file_size / (1024.0 * 1024.0));
    printf("  Mapped size: %zu bytes (%.1f MB)\n", storage->mapped_size, storage->mapped_size / (1024.0 * 1024.0));
    printf("\n");
    
    printf("Header info:\n");
    printf("  Magic: 0x%X\n", storage->header->magic);
    printf("  Version: %u\n", storage->header->version);
    printf("  Data offset: %llu\n", (unsigned long long)storage->header->data_offset);
    printf("  Index offset: %llu (%.1f MB)\n", 
           (unsigned long long)storage->header->index_offset,
           storage->header->index_offset / (1024.0 * 1024.0));
    printf("  Free offset: %llu\n", (unsigned long long)storage->header->free_offset);
    printf("  Partition count: %u\n", storage->header->partition_count);
    printf("  Partition size: %u bytes\n", storage->header->partition_size);
    printf("\n");
    
    printf("Available space:\n");
    printf("  Data region: %llu - %llu (%llu bytes, %.1f MB)\n",
           (unsigned long long)storage->header->data_offset,
           (unsigned long long)storage->header->index_offset,
           (unsigned long long)(storage->header->index_offset - storage->header->data_offset),
           (storage->header->index_offset - storage->header->data_offset) / (1024.0 * 1024.0));
    printf("\n");
    
    /* Calculate how many 4KB buckets fit in data region */
    uint64_t data_region_size = storage->header->index_offset - storage->header->data_offset;
    uint64_t max_buckets = data_region_size / 4096;
    printf("Hash index buckets:\n");
    printf("  Bucket size: 4096 bytes\n");
    printf("  Max buckets in data region: %llu\n", (unsigned long long)max_buckets);
    printf("  Max documents (at 33 per bucket): %llu\n", (unsigned long long)(max_buckets * 33));
    printf("\n");
    
    /* Simulate bucket allocation */
    printf("Simulating bucket allocation:\n");
    uint64_t offset = storage->header->free_offset;
    int count = 0;
    while (offset + 4096 <= storage->header->index_offset) {
        count++;
        offset += 4096;
    }
    printf("  Can allocate %d buckets before hitting index region\n", count);
    printf("  Final offset would be: %llu\n", (unsigned long long)offset);
    printf("  Index offset is at: %llu\n", (unsigned long long)storage->header->index_offset);
    
    mmap_storage_destroy(storage);
    return 0;
}