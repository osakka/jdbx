#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "index/hash_index.h"

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

int main() {
    // Simulate directory growth from depth 4 to 5
    printf("Testing bucket reassignment during split...\n\n");
    
    // At depth 4, bucket_index 11 is just 11
    // At depth 5, bucket 11 could stay at 11 or become 27 (11 + 16)
    
    printf("When depth increases from 4 to 5:\n");
    printf("Old mask (depth 4): 0x%x (15)\n", (1U << 4) - 1);
    printf("New mask (depth 5): 0x%x (31)\n", (1U << 5) - 1);
    printf("New bit (bit 4): 0x%x (16)\n\n", 1U << 4);
    
    // Check which buckets get reassigned
    printf("Directory entry reassignments:\n");
    uint32_t old_mask = (1U << 4) - 1;  // 15
    uint32_t new_bit = 1U << 4;         // 16
    uint32_t bucket_index = 11;
    
    for (uint32_t i = 0; i < 32; i++) {
        if ((i & old_mask) == (bucket_index & old_mask)) {
            if (i & new_bit) {
                printf("  Entry %2u: gets NEW bucket (had bit 4 set)\n", i);
            } else {
                printf("  Entry %2u: keeps OLD bucket (bit 4 clear)\n", i);
            }
        }
    }
    
    printf("\nSpecifically:\n");
    printf("  Bucket 11: %s\n", (11 & new_bit) ? "gets NEW bucket" : "keeps OLD bucket");
    printf("  Bucket 27: %s\n", (27 & new_bit) ? "gets NEW bucket" : "keeps OLD bucket");
    
    // Now test with actual documents
    printf("\n\nDocument distribution check:\n");
    for (int i = 410; i <= 430; i++) {
        char doc_id[64];
        snprintf(doc_id, sizeof(doc_id), "doc-1749032608-%d-%04x", 100000000 + i, i);
        uint32_t hash = fnv1a_hash(doc_id, strlen(doc_id));
        
        uint32_t bucket_at_depth_4 = hash & 15;
        uint32_t bucket_at_depth_5 = hash & 31;
        
        if (bucket_at_depth_4 == 11) {
            printf("Doc %d: hash=0x%x, depth4=%u, depth5=%u %s\n", 
                   i, hash, bucket_at_depth_4, bucket_at_depth_5,
                   (bucket_at_depth_5 == 11) ? "[stays in 11]" : "[moves to 27]");
        }
    }
    
    return 0;
}