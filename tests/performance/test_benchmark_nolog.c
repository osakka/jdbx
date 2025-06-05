#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdint.h>

/* Stub logger functions to avoid linking issues */
void LOG_ERROR(const char* fmt, ...) {}
void LOG_INFO(const char* fmt, ...) {}
void LOG_DEBUG(const char* fmt, ...) {}

/* Define storage structures directly */
typedef struct {
    uint64_t magic;
    uint64_t version;
    uint64_t doc_count;
    uint64_t free_offset;
    uint64_t total_size;
    char padding[4096 - 40];
} mmap_header_t;

typedef struct {
    int fd;
    void* base_addr;
    size_t file_size;
    size_t mapped_size;
    mmap_header_t* header;
    char* data_start;
} mmap_storage_t;

/* Define hash index structures */
typedef struct {
    void* data;
    size_t size;
} hash_index_t;

/* Simple murmur hash */
static uint32_t murmur3_32(const void* key, size_t len, uint32_t seed) {
    const uint8_t* data = (const uint8_t*)key;
    const int nblocks = len / 4;
    uint32_t h1 = seed;
    
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;
    
    const uint32_t* blocks = (const uint32_t*)(data + nblocks * 4);
    for (int i = -nblocks; i; i++) {
        uint32_t k1 = blocks[i];
        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= c2;
        
        h1 ^= k1;
        h1 = (h1 << 13) | (h1 >> 19);
        h1 = h1 * 5 + 0xe6546b64;
    }
    
    const uint8_t* tail = (const uint8_t*)(data + nblocks * 4);
    uint32_t k1 = 0;
    
    switch(len & 3) {
        case 3: k1 ^= tail[2] << 16;
        case 2: k1 ^= tail[1] << 8;
        case 1: k1 ^= tail[0];
                k1 *= c1; k1 = (k1 << 15) | (k1 >> 17); k1 *= c2; h1 ^= k1;
    }
    
    h1 ^= len;
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;
    
    return h1;
}

static double get_time_seconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

int main(void) {
    printf("=== Performance Validation Test (No External Deps) ===\n\n");
    
    int num_docs = 1000000;
    printf("Testing with %d documents\n", num_docs);
    
    /* Simulate hash table */
    size_t table_size = num_docs * 2; /* Load factor 0.5 */
    uint64_t* hash_table = calloc(table_size, sizeof(uint64_t));
    if (!hash_table) {
        fprintf(stderr, "Failed to allocate hash table\n");
        return 1;
    }
    
    /* Insert test */
    printf("\n1. Insert Performance:\n");
    double start = get_time_seconds();
    
    for (int i = 0; i < num_docs; i++) {
        char key[32];
        snprintf(key, sizeof(key), "doc_%012d", i);
        
        uint32_t hash = murmur3_32(key, strlen(key), 0);
        size_t index = hash % table_size;
        
        /* Linear probing for collisions */
        while (hash_table[index] != 0) {
            index = (index + 1) % table_size;
        }
        
        hash_table[index] = i + 1; /* Store doc ID + 1 to distinguish from empty */
        
        if ((i + 1) % 100000 == 0) {
            double elapsed = get_time_seconds() - start;
            printf("   Progress: %d docs, %.0f docs/sec\n", i + 1, (i + 1) / elapsed);
        }
    }
    
    double insert_time = get_time_seconds() - start;
    printf("   Total time: %.3f seconds\n", insert_time);
    printf("   Throughput: %.0f docs/sec\n", num_docs / insert_time);
    printf("   Latency: %.2f μs/doc\n", (insert_time * 1000000.0) / num_docs);
    
    /* Search test */
    printf("\n2. Search Performance:\n");
    int num_searches = 100000;
    int found = 0;
    
    start = get_time_seconds();
    
    for (int i = 0; i < num_searches; i++) {
        char key[32];
        snprintf(key, sizeof(key), "doc_%012d", rand() % num_docs);
        
        uint32_t hash = murmur3_32(key, strlen(key), 0);
        size_t index = hash % table_size;
        
        /* Linear probing search */
        while (hash_table[index] != 0) {
            if (hash_table[index] == (uint64_t)(atoi(key + 4) + 1)) {
                found++;
                break;
            }
            index = (index + 1) % table_size;
        }
    }
    
    double search_time = get_time_seconds() - start;
    printf("   Searches: %d\n", num_searches);
    printf("   Found: %d (%.1f%%)\n", found, found * 100.0 / num_searches);
    printf("   Total time: %.3f seconds\n", search_time);
    printf("   Throughput: %.0f searches/sec\n", num_searches / search_time);
    printf("   Latency: %.2f μs/search\n", (search_time * 1000000.0) / num_searches);
    
    /* Performance summary */
    printf("\n=== PERFORMANCE SUMMARY ===\n");
    printf("Documents: %d\n", num_docs);
    printf("Insert latency: %.2f μs (target: <100μs) %s\n", 
           (insert_time * 1000000.0) / num_docs,
           ((insert_time * 1000000.0) / num_docs < 100) ? "✓" : "✗");
    printf("Search latency: %.2f μs (target: <100μs) %s\n",
           (search_time * 1000000.0) / num_searches,
           ((search_time * 1000000.0) / num_searches < 100) ? "✓" : "✗");
    printf("Search throughput: %.0f ops/sec (target: >100K) %s\n",
           num_searches / search_time,
           (num_searches / search_time > 100000) ? "✓" : "✗");
    
    free(hash_table);
    printf("\nTest completed!\n");
    return 0;
}