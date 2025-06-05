#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "index/hash_index.h"
#include "storage/mmap_storage.h"
#include "utils/logger.h"

void hexdump(const char* label, void* ptr, size_t len) {
    printf("\n%s:\n", label);
    unsigned char* p = (unsigned char*)ptr;
    for (size_t i = 0; i < len; i++) {
        if (i % 16 == 0) printf("%04zx: ", i);
        printf("%02x ", p[i]);
        if (i % 16 == 15) printf("\n");
    }
    if (len % 16 != 0) printf("\n");
}

int main() {
    logger_init("/tmp/test_bucket_debug.log", LOG_LEVEL_DEBUG);
    
    // Create a hash index
    hash_index_t* index = hash_index_create("/tmp/test_bucket.idx", NULL);
    assert(index != NULL);
    
    printf("Testing bucket structure...\n");
    printf("sizeof(hash_bucket_t) = %zu\n", sizeof(hash_bucket_t));
    printf("offsetof(hash_bucket_t, entry_offsets) = %zu\n", 
           offsetof(hash_bucket_t, entry_offsets));
    
    // Get bucket 0
    hash_dir_entry_t* dir_entry = &index->directory[0];
    hash_bucket_t* bucket = (hash_bucket_t*)
        ((char*)index->storage->base_addr + dir_entry->bucket_offset);
    
    printf("\nInitial bucket 0 state:\n");
    printf("local_depth: %u\n", bucket->local_depth);
    printf("num_entries: %u\n", bucket->num_entries);
    hexdump("First 256 bytes of bucket", bucket, 256);
    
    // Insert one entry
    const char* key = "test-key-1";
    uint32_t hash = index->hash_fn(key, strlen(key));
    uint32_t expected_bucket = hash & ((1U << index->global_depth) - 1);
    printf("\nKey '%s' hash=0x%x, expected bucket=%u\n", key, hash, expected_bucket);
    
    int rc = hash_index_insert(index, key, strlen(key), 12345);
    printf("Inserted '%s', rc=%d\n", key, rc);
    
    // Check the correct bucket (13)
    dir_entry = &index->directory[expected_bucket];
    bucket = (hash_bucket_t*)((char*)index->storage->base_addr + dir_entry->bucket_offset);
    
    printf("\nBucket %u after first insert:\n", expected_bucket);
    printf("num_entries: %u\n", bucket->num_entries);
    if (bucket->num_entries > 0) {
        printf("entry_offsets[0]: %u\n", bucket->entry_offsets[0]);
    }
    
    // Calculate expected offset
    uint32_t expected_offset = sizeof(hash_bucket_t) + 32 * sizeof(uint32_t);
    printf("Expected first entry offset: %u\n", expected_offset);
    
    if (bucket->entry_offsets[0] != expected_offset) {
        printf("ERROR: First entry offset mismatch!\n");
    }
    
    // Check the entry
    if (bucket->entry_offsets[0] < 256) {
        hash_entry_t* entry = (hash_entry_t*)((char*)bucket + bucket->entry_offsets[0]);
        printf("\nFirst entry:\n");
        printf("  key_hash: 0x%x\n", entry->key_hash);
        printf("  key_len: %u\n", entry->key_len);
        printf("  value_offset: %lu\n", entry->value_offset);
    }
    
    hash_index_destroy(index);
    logger_close();
    return 0;
}