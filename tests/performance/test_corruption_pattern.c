#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "index/hash_index.h"
#include "utils/logger.h"

// Function to calculate hash (copied from hash_index.c)
static uint32_t fnv1a_hash(const void* key, size_t len) {
    const uint8_t* data = (const uint8_t*)key;
    uint32_t hash = 2166136261U;
    
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    
    return hash;
}

// Function to check bucket memory
void check_bucket_memory(hash_index_t* index, uint32_t bucket_index, const char* phase) {
    hash_dir_entry_t* dir_entry = &index->directory[bucket_index];
    hash_bucket_t* bucket = (hash_bucket_t*)((char*)index->storage->base_addr + dir_entry->bucket_offset);
    
    printf("\n=== Bucket %u Memory Check (%s) ===\n", bucket_index, phase);
    printf("Bucket offset: %lu\n", dir_entry->bucket_offset);
    printf("num_entries: %u\n", bucket->num_entries);
    
    // Check first few bytes at offset 144
    if (bucket->num_entries > 0) {
        uint32_t offset = bucket->entry_offsets[0];
        if (offset == 144) {
            unsigned char* ptr = (unsigned char*)bucket + 144;
            printf("Memory at offset 144: ");
            for (int i = 0; i < 32; i++) {
                printf("%02x ", ptr[i]);
                if ((i + 1) % 16 == 0) printf("\n                      ");
            }
            printf("\n");
            
            // Interpret as hash_entry_t
            hash_entry_t* entry = (hash_entry_t*)ptr;
            printf("Interpreted as entry: key_hash=0x%x, key_len=%u, value_len=%u, value_offset=%lu\n",
                   entry->key_hash, entry->key_len, entry->value_len, entry->value_offset);
        }
    }
}

int main() {
    logger_init("/tmp/test_corruption_pattern.log", LOG_LEVEL_DEBUG);
    
    // Create a hash index
    hash_index_t* index = hash_index_create("/tmp/test_corruption.idx", NULL);
    assert(index != NULL);
    
    printf("Testing corruption pattern...\n");
    
    // First, find which document goes into bucket 11 first
    int first_bucket_11 = -1;
    for (int i = 0; i < 500; i++) {
        char doc_id[64];
        snprintf(doc_id, sizeof(doc_id), "doc-1749032608-%d-%04x", 100000000 + i, i);
        
        uint32_t hash = fnv1a_hash(doc_id, strlen(doc_id));
        uint32_t bucket = hash & 15;
        
        if (bucket == 11 && first_bucket_11 == -1) {
            first_bucket_11 = i;
            printf("First document for bucket 11: %s (i=%d)\n", doc_id, i);
            break;
        }
    }
    
    // Insert documents up to and including the first bucket 11 entry
    printf("\nInserting documents...\n");
    for (int i = 0; i <= first_bucket_11 + 10; i++) {
        char doc_id[64];
        snprintf(doc_id, sizeof(doc_id), "doc-1749032608-%d-%04x", 100000000 + i, i);
        
        uint32_t hash = fnv1a_hash(doc_id, strlen(doc_id));
        uint32_t bucket = hash & 15;
        
        int rc = hash_index_insert(index, doc_id, strlen(doc_id), 4096 + i * 100);
        if (rc != 0) {
            printf("ERROR at i=%d\n", i);
            break;
        }
        
        if (bucket == 11) {
            printf("Inserted into bucket 11: %s (i=%d)\n", doc_id, i);
            check_bucket_memory(index, 11, "after insertion");
        }
    }
    
    // Now continue inserting more documents and check bucket 11 periodically
    printf("\nContinuing insertions and monitoring bucket 11...\n");
    for (int i = first_bucket_11 + 11; i < 500; i++) {
        char doc_id[64];
        snprintf(doc_id, sizeof(doc_id), "doc-1749032608-%d-%04x", 100000000 + i, i);
        
        // Check bucket 11 before insertion
        if (i % 50 == 0) {
            check_bucket_memory(index, 11, "periodic check");
        }
        
        int rc = hash_index_insert(index, doc_id, strlen(doc_id), 4096 + i * 100);
        if (rc != 0) {
            printf("ERROR at i=%d (doc_id=%s)\n", i, doc_id);
            check_bucket_memory(index, 11, "after error");
            
            // Check all buckets to see if any have suspicious data
            printf("\nChecking all buckets for anomalies...\n");
            for (uint32_t b = 0; b < 16; b++) {
                hash_dir_entry_t* de = &index->directory[b];
                hash_bucket_t* bkt = (hash_bucket_t*)((char*)index->storage->base_addr + de->bucket_offset);
                if (bkt->num_entries > 20) {
                    printf("Bucket %u has %u entries (offset=%lu)\n", b, bkt->num_entries, de->bucket_offset);
                }
            }
            break;
        }
    }
    
    hash_index_destroy(index);
    logger_close();
    return 0;
}