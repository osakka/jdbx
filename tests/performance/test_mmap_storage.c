#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "storage/mmap_storage.h"

#define NUM_DOCS 10000
#define KEY_SIZE 32
#define VALUE_SIZE 1024

/* Get current time in microseconds */
static double get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000.0 + tv.tv_usec;
}

/* Generate random key */
static void generate_key(char* key, int id) {
    snprintf(key, KEY_SIZE, "doc-%08d-%08x", id, rand());
}

/* Generate random value */
static void generate_value(char* value, size_t size) {
    for (size_t i = 0; i < size - 1; i++) {
        value[i] = 'A' + (rand() % 26);
    }
    value[size - 1] = '\0';
}

int main(void) {
    printf("=== JSONdb Memory-Mapped Storage Performance Test ===\n");
    printf("Testing with %d documents\n", NUM_DOCS);
    
    /* Create storage */
    const char* storage_path = "/tmp/jsondb_perf_test.mmap";
    mmap_storage_t* storage = mmap_storage_create(storage_path, 1ULL << 30); /* 1GB */
    if (!storage) {
        fprintf(stderr, "Failed to create storage\n");
        return 1;
    }
    
    /* Test 1: Sequential Writes */
    printf("\n1. Sequential Write Test\n");
    char key[KEY_SIZE];
    char value[VALUE_SIZE];
    
    double start_time = get_time_us();
    
    for (int i = 0; i < NUM_DOCS; i++) {
        generate_key(key, i);
        generate_value(value, VALUE_SIZE);
        
        if (mmap_storage_put(storage, key, strlen(key), value, strlen(value)) != 0) {
            fprintf(stderr, "Failed to insert document %d\n", i);
        }
        
        if ((i + 1) % 100000 == 0) {
            printf("  Inserted %d documents...\n", i + 1);
        }
    }
    
    double write_time = get_time_us() - start_time;
    double write_throughput = NUM_DOCS / (write_time / 1000000.0);
    printf("  Total write time: %.2f seconds\n", write_time / 1000000.0);
    printf("  Write throughput: %.0f docs/sec\n", write_throughput);
    printf("  Average write latency: %.2f μs\n", write_time / NUM_DOCS);
    
    /* Test 2: Random Reads */
    printf("\n2. Random Read Test\n");
    int num_reads = 100000;
    int hits = 0;
    
    start_time = get_time_us();
    
    for (int i = 0; i < num_reads; i++) {
        int doc_id = rand() % NUM_DOCS;
        generate_key(key, doc_id);
        
        void* read_value;
        size_t read_len;
        
        if (mmap_storage_get(storage, key, strlen(key), &read_value, &read_len) == 0) {
            hits++;
            free(read_value);
        }
    }
    
    double read_time = get_time_us() - start_time;
    double read_throughput = num_reads / (read_time / 1000000.0);
    printf("  Total read time: %.2f seconds\n", read_time / 1000000.0);
    printf("  Read throughput: %.0f reads/sec\n", read_throughput);
    printf("  Average read latency: %.2f μs\n", read_time / num_reads);
    printf("  Hit rate: %.1f%%\n", (hits * 100.0) / num_reads);
    
    /* Test 3: Batch Operations */
    printf("\n3. Batch Write Test\n");
    int batch_size = 1000;
    const void** batch_keys = malloc(batch_size * sizeof(void*));
    size_t* batch_key_lens = malloc(batch_size * sizeof(size_t));
    const void** batch_values = malloc(batch_size * sizeof(void*));
    size_t* batch_value_lens = malloc(batch_size * sizeof(size_t));
    
    /* Prepare batch data */
    for (int i = 0; i < batch_size; i++) {
        char* batch_key = malloc(KEY_SIZE);
        char* batch_value = malloc(VALUE_SIZE);
        
        generate_key(batch_key, NUM_DOCS + i);
        generate_value(batch_value, VALUE_SIZE);
        
        batch_keys[i] = batch_key;
        batch_key_lens[i] = strlen(batch_key);
        batch_values[i] = batch_value;
        batch_value_lens[i] = strlen(batch_value);
    }
    
    start_time = get_time_us();
    int batch_result = mmap_storage_put_batch(storage, batch_keys, batch_key_lens,
                                            batch_values, batch_value_lens, batch_size);
    double batch_time = get_time_us() - start_time;
    
    printf("  Batch size: %d documents\n", batch_size);
    printf("  Batch write time: %.2f ms\n", batch_time / 1000.0);
    printf("  Batch throughput: %.0f docs/sec\n", batch_size / (batch_time / 1000000.0));
    printf("  Documents written: %d\n", batch_result);
    
    /* Cleanup batch data */
    for (int i = 0; i < batch_size; i++) {
        free((void*)batch_keys[i]);
        free((void*)batch_values[i]);
    }
    free(batch_keys);
    free(batch_key_lens);
    free(batch_values);
    free(batch_value_lens);
    
    /* Test 4: Partitioned Collection */
    printf("\n4. Partitioned Collection Test\n");
    partitioned_collection_t* part_coll = partitioned_collection_create("test_collection", 16);
    if (!part_coll) {
        fprintf(stderr, "Failed to create partitioned collection\n");
    } else {
        int part_docs = 10000;
        start_time = get_time_us();
        
        for (int i = 0; i < part_docs; i++) {
            generate_key(key, i);
            generate_value(value, VALUE_SIZE);
            
            partitioned_put(part_coll, key, strlen(key), value, strlen(value));
        }
        
        double part_time = get_time_us() - start_time;
        printf("  Inserted %d documents into 16 partitions\n", part_docs);
        printf("  Total time: %.2f seconds\n", part_time / 1000000.0);
        printf("  Throughput: %.0f docs/sec\n", part_docs / (part_time / 1000000.0));
        
        partitioned_collection_destroy(part_coll);
    }
    
    /* Final Statistics */
    printf("\n5. Storage Statistics\n");
    printf("  Total documents: %llu\n", (unsigned long long)storage->header->doc_count);
    printf("  Deleted documents: %llu\n", (unsigned long long)storage->header->deleted_count);
    printf("  File size: %.2f MB\n", storage->file_size / (1024.0 * 1024.0));
    printf("  Read count: %llu\n", (unsigned long long)atomic_load(&storage->read_count));
    printf("  Write count: %llu\n", (unsigned long long)atomic_load(&storage->write_count));
    printf("  Cache hits: %llu\n", (unsigned long long)atomic_load(&storage->cache_hits));
    printf("  Cache misses: %llu\n", (unsigned long long)atomic_load(&storage->cache_misses));
    
    /* Cleanup */
    mmap_storage_destroy(storage);
    
    printf("\nTest completed successfully!\n");
    return 0;
}