#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "utils/logger.h"
#include "storage/mmap_storage.h"
#include "index/hash_index.h"

static double get_time_seconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

int main(void) {
    printf("=== Simple Benchmark Test ===\n");
    
    /* Initialize logger */
    if (logger_init("/tmp/benchmark.log", LOG_LEVEL_INFO) != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }
    
    /* Create storage */
    printf("Creating storage...\n");
    mmap_storage_t* storage = mmap_storage_create("/tmp/test_bench.mmap", 10 * 1024 * 1024);
    if (!storage) {
        fprintf(stderr, "Failed to create storage\n");
        logger_close();
        return 1;
    }
    printf("Storage created successfully\n");
    
    /* Create hash index */
    printf("Creating hash index...\n");
    hash_index_t* hash = hash_index_create("/tmp/test_bench.hash", NULL);
    if (!hash) {
        fprintf(stderr, "Failed to create hash index\n");
        mmap_storage_destroy(storage);
        logger_close();
        return 1;
    }
    printf("Hash index created successfully\n");
    
    /* Test inserting 1000 documents */
    int num_docs = 1000;
    printf("\nInserting %d documents...\n", num_docs);
    
    double start = get_time_seconds();
    
    for (int i = 0; i < num_docs; i++) {
        char key[64];
        char value[256];
        
        snprintf(key, sizeof(key), "doc_%06d", i);
        snprintf(value, sizeof(value), "{\"id\": %d, \"value\": \"test data %d\"}", i, i);
        
        /* Store in mmap storage */
        uint64_t offset = storage->header->free_offset;
        if (mmap_storage_put(storage, key, strlen(key), value, strlen(value)) == 0) {
            /* Index the document */
            hash_index_insert(hash, key, strlen(key), offset);
        }
        
        if ((i + 1) % 100 == 0) {
            printf("Progress: %d/%d\n", i + 1, num_docs);
        }
    }
    
    double elapsed = get_time_seconds() - start;
    printf("\nInsert completed:\n");
    printf("  Time: %.3f seconds\n", elapsed);
    printf("  Rate: %.0f docs/sec\n", num_docs / elapsed);
    
    /* Test searching */
    printf("\nSearching for 100 random documents...\n");
    int found = 0;
    
    start = get_time_seconds();
    
    for (int i = 0; i < 100; i++) {
        char key[64];
        snprintf(key, sizeof(key), "doc_%06d", rand() % num_docs);
        
        uint64_t offset;
        if (hash_index_search(hash, key, strlen(key), &offset) == 0) {
            void* value;
            size_t value_len;
            if (mmap_storage_get(storage, key, strlen(key), &value, &value_len) == 0) {
                found++;
                free(value);
            }
        }
    }
    
    elapsed = get_time_seconds() - start;
    printf("\nSearch completed:\n");
    printf("  Found: %d/100\n", found);
    printf("  Time: %.3f seconds\n", elapsed);
    printf("  Rate: %.0f searches/sec\n", 100.0 / elapsed);
    
    /* Cleanup */
    hash_index_destroy(hash);
    mmap_storage_destroy(storage);
    logger_close();
    
    printf("\nTest completed successfully!\n");
    return 0;
}