#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Function to calculate hash (same as in hash_index.c)
static uint32_t fnv1a_hash(const void* key, size_t len) {
    const uint8_t* data = (const uint8_t*)key;
    uint32_t hash = 2166136261U;
    
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    
    return hash;
}

typedef struct {
    uint32_t key_hash;
    uint32_t key_len;
    uint64_t value_offset;
} hash_entry_t;

int main() {
    // Test bucket calculations for specific documents
    printf("Testing bucket calculations...\n\n");
    
    // Document 2 that is failing
    char doc2[64];
    snprintf(doc2, sizeof(doc2), "doc-1749032608-%d-%04x", 100000002, 2);
    
    uint32_t hash2 = fnv1a_hash(doc2, strlen(doc2));
    printf("Document 2: %s\n", doc2);
    printf("  Hash: 0x%x\n", hash2);
    printf("  Bucket at depth 4: %u\n", hash2 & 15);
    printf("  Bucket at depth 5: %u\n", hash2 & 31);
    
    // Check bucket 11 capacity
    printf("\nBucket capacity calculation:\n");
    uint32_t header_size = 8 + 32 * 4;  // local_depth + num_entries + 32 offsets
    printf("  Header size: %u bytes\n", header_size);
    printf("  Available for entries: %u bytes\n", 4096 - header_size);
    
    uint32_t entry_size = sizeof(hash_entry_t) + 29;  // typical doc ID length
    printf("  Typical entry size: %lu bytes\n", (unsigned long)entry_size);
    printf("  Max entries per bucket: %u\n", (4096 - header_size) / entry_size);
    
    // Calculate at what point bucket 11 would be full
    printf("\nBucket 11 fill analysis:\n");
    int count = 0;
    for (int i = 0; i < 500; i++) {
        char doc_id[64];
        snprintf(doc_id, sizeof(doc_id), "doc-1749032608-%d-%04x", 100000000 + i, i);
        uint32_t hash = fnv1a_hash(doc_id, strlen(doc_id));
        uint32_t bucket = hash & 15;  // depth 4
        
        if (bucket == 11) {
            count++;
            if (count <= 5 || count > 100) {
                printf("  Doc %d -> bucket 11 (count: %d)\n", i, count);
            } else if (count == 6) {
                printf("  ...\n");
            }
        }
    }
    
    return 0;
}
EOF < /dev/null
