#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include "storage/mmap_storage.h"
#include "index/btree_disk.h"
#include "index/hash_index.h"

#define NUM_DOCS 1000000
#define NUM_THREADS 8
#define BATCH_SIZE 1000

/* Test configuration */
typedef struct test_config {
    int num_docs;
    int doc_size;
    int num_threads;
    int batch_size;
    double write_ratio;
    double read_ratio;
    double scan_ratio;
} test_config_t;

/* Thread context */
typedef struct thread_context {
    int thread_id;
    test_config_t* config;
    mmap_storage_t* storage;
    btree_disk_t* btree;
    hash_index_t* hash;
    double total_time;
    uint64_t operations;
} thread_context_t;

/* Timing helpers */
static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

/* Generate test key */
static void generate_key(char* key, int id) {
    snprintf(key, 64, "user:%012d", id);
}

/* Generate test document */
static char* generate_document(int id, size_t size) {
    char* doc = malloc(size);
    if (!doc) return NULL;
    
    int written = snprintf(doc, size, 
        "{\"id\":%d,\"name\":\"User %d\",\"email\":\"user%d@example.com\","
        "\"created\":%ld,\"data\":\"", 
        id, id, id, time(NULL));
    
    /* Fill remaining space with data */
    for (size_t i = written; i < size - 3; i++) {
        doc[i] = 'A' + (rand() % 26);
    }
    
    doc[size-3] = '"';
    doc[size-2] = '}';
    doc[size-1] = '\0';
    
    return doc;
}

/* Benchmark write performance */
static void benchmark_writes(test_config_t* config, mmap_storage_t* storage,
                           btree_disk_t* btree, hash_index_t* hash) {
    printf("\n=== Write Performance Test ===\n");
    printf("Documents: %d, Size: %d bytes\n", config->num_docs, config->doc_size);
    
    double start = get_time_ms();
    
    for (int i = 0; i < config->num_docs; i++) {
        char key[64];
        generate_key(key, i);
        
        char* doc = generate_document(i, config->doc_size);
        if (!doc) continue;
        
        /* Insert into storage */
        uint64_t offset = storage->header->free_offset;
        if (mmap_storage_put(storage, key, strlen(key), doc, strlen(doc)) == 0) {
            /* Update indexes */
            if (btree) btree_disk_insert(btree, key, strlen(key), offset);
            if (hash) hash_index_insert(hash, key, strlen(key), offset);
        }
        
        free(doc);
        
        if ((i + 1) % 10000 == 0) {
            double elapsed = get_time_ms() - start;
            double rate = (i + 1) / (elapsed / 1000.0);
            printf("  Progress: %d docs, %.0f docs/sec\n", i + 1, rate);
        }
    }
    
    double elapsed = get_time_ms() - start;
    double rate = config->num_docs / (elapsed / 1000.0);
    
    printf("\nWrite Results:\n");
    printf("  Total time: %.2f seconds\n", elapsed / 1000.0);
    printf("  Throughput: %.0f docs/sec\n", rate);
    printf("  Latency: %.3f ms/doc\n", elapsed / config->num_docs);
}

/* Benchmark read performance */
static void benchmark_reads(test_config_t* config, mmap_storage_t* storage,
                          btree_disk_t* btree, hash_index_t* hash) {
    printf("\n=== Read Performance Test ===\n");
    
    int num_reads = config->num_docs / 10; /* Read 10% of documents */
    double btree_time = 0, hash_time = 0, scan_time = 0;
    int hits = 0;
    
    /* Test point queries with B+tree */
    if (btree) {
        printf("\nB+tree point queries:\n");
        double start = get_time_ms();
        
        for (int i = 0; i < num_reads; i++) {
            char key[64];
            int id = rand() % config->num_docs;
            generate_key(key, id);
            
            uint64_t offset;
            if (btree_disk_search(btree, key, strlen(key), &offset) == 0) {
                hits++;
            }
        }
        
        btree_time = get_time_ms() - start;
        printf("  Time: %.2f seconds\n", btree_time / 1000.0);
        printf("  Throughput: %.0f queries/sec\n", num_reads / (btree_time / 1000.0));
        printf("  Latency: %.3f μs/query\n", (btree_time * 1000.0) / num_reads);
        printf("  Hit rate: %.1f%%\n", (hits * 100.0) / num_reads);
    }
    
    /* Test point queries with hash index */
    if (hash) {
        printf("\nHash index point queries:\n");
        hits = 0;
        double start = get_time_ms();
        
        for (int i = 0; i < num_reads; i++) {
            char key[64];
            int id = rand() % config->num_docs;
            generate_key(key, id);
            
            uint64_t offset;
            if (hash_index_search(hash, key, strlen(key), &offset) == 0) {
                hits++;
            }
        }
        
        hash_time = get_time_ms() - start;
        printf("  Time: %.2f seconds\n", hash_time / 1000.0);
        printf("  Throughput: %.0f queries/sec\n", num_reads / (hash_time / 1000.0));
        printf("  Latency: %.3f μs/query\n", (hash_time * 1000.0) / num_reads);
        printf("  Hit rate: %.1f%%\n", (hits * 100.0) / num_reads);
    }
    
    /* Compare linear scan (baseline) */
    printf("\nLinear scan (baseline):\n");
    int scan_count = 100; /* Only scan for 100 docs due to O(n) complexity */
    double start = get_time_ms();
    hits = 0;
    
    for (int i = 0; i < scan_count; i++) {
        char key[64];
        int id = rand() % config->num_docs;
        generate_key(key, id);
        
        void* value;
        size_t value_len;
        if (mmap_storage_get(storage, key, strlen(key), &value, &value_len) == 0) {
            hits++;
            free(value);
        }
    }
    
    scan_time = get_time_ms() - start;
    printf("  Time: %.2f seconds (for %d queries)\n", scan_time / 1000.0, scan_count);
    printf("  Latency: %.3f ms/query\n", scan_time / scan_count);
    printf("  Projected time for %d queries: %.1f minutes\n", 
           num_reads, (scan_time / scan_count * num_reads) / 60000.0);
    
    /* Performance comparison */
    printf("\n=== Performance Comparison ===\n");
    if (btree && hash) {
        printf("Hash index is %.1fx faster than B+tree\n", btree_time / hash_time);
        printf("Hash index is %.0fx faster than linear scan\n", 
               (scan_time / scan_count * num_reads) / hash_time);
        printf("B+tree is %.0fx faster than linear scan\n",
               (scan_time / scan_count * num_reads) / btree_time);
    }
}

/* Main benchmark */
int main(int argc, char* argv[]) {
    printf("=== JSONdb Indexed Storage Benchmark ===\n");
    
    test_config_t config = {
        .num_docs = (argc > 1) ? atoi(argv[1]) : 100000,
        .doc_size = 1024,
        .num_threads = 1,
        .batch_size = 1000,
        .write_ratio = 0.8,
        .read_ratio = 0.2,
        .scan_ratio = 0.0
    };
    
    /* Create storage */
    printf("\nCreating storage...\n");
    mmap_storage_t* storage = mmap_storage_create("/tmp/jsondb_bench.mmap", 1ULL << 30);
    if (!storage) {
        fprintf(stderr, "Failed to create storage\n");
        return 1;
    }
    
    /* Create B+tree index */
    printf("Creating B+tree index...\n");
    btree_disk_t* btree = btree_disk_create("/tmp/jsondb_bench.btree", 200, NULL);
    if (!btree) {
        fprintf(stderr, "Failed to create B+tree\n");
    }
    
    /* Create hash index */
    printf("Creating hash index...\n");
    hash_index_t* hash = hash_index_create("/tmp/jsondb_bench.hash", NULL);
    if (!hash) {
        fprintf(stderr, "Failed to create hash index\n");
    }
    
    /* Run benchmarks */
    benchmark_writes(&config, storage, btree, hash);
    
    /* Flush indexes */
    if (btree) {
        printf("\nFlushing B+tree buffer...\n");
        btree_disk_flush_buffer(btree);
    }
    
    benchmark_reads(&config, storage, btree, hash);
    
    /* Print final statistics */
    printf("\n=== Final Statistics ===\n");
    printf("Storage:\n");
    printf("  Documents: %llu\n", (unsigned long long)storage->header->doc_count);
    printf("  File size: %.2f MB\n", storage->file_size / (1024.0 * 1024.0));
    
    if (btree) {
        uint64_t height, num_keys, cache_hits, cache_misses;
        btree_disk_stats(btree, &height, &num_keys, &cache_hits, &cache_misses);
        printf("\nB+tree:\n");
        printf("  Height: %llu\n", (unsigned long long)height);
        printf("  Keys: %llu\n", (unsigned long long)num_keys);
        printf("  Cache hit rate: %.1f%%\n", 
               cache_hits * 100.0 / (cache_hits + cache_misses + 1));
    }
    
    /* Cleanup */
    if (hash) hash_index_destroy(hash);
    if (btree) btree_disk_destroy(btree);
    mmap_storage_destroy(storage);
    
    printf("\nBenchmark complete!\n");
    return 0;
}