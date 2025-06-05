#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <execinfo.h>
#include "index/hash_index.h"

/* Signal handler for debugging */
void segfault_handler(int sig) {
    void *array[10];
    size_t size;
    
    fprintf(stderr, "\n=== SEGFAULT at signal %d ===\n", sig);
    size = backtrace(array, 10);
    fprintf(stderr, "Stack trace:\n");
    backtrace_symbols_fd(array, size, 2);
    exit(1);
}

int main(void) {
    /* Install signal handler */
    signal(SIGSEGV, segfault_handler);
    
    printf("=== Hash Index Stress Test ===\n\n");
    
    /* Remove old test file */
    remove("/tmp/hash_stress.idx");
    
    /* Create hash index */
    printf("Creating Hash Index...\n");
    hash_index_t* hash = hash_index_create("/tmp/hash_stress.idx", NULL);
    if (!hash) {
        fprintf(stderr, "Failed to create hash index\n");
        return 1;
    }
    
    /* Test 1: Insert 10,000 documents */
    printf("\n1. Inserting 10,000 documents...\n");
    clock_t start = clock();
    int fail_count = 0;
    
    for (int i = 0; i < 10000; i++) {
        char key[64];
        snprintf(key, sizeof(key), "document-%08d-test", i);
        uint64_t value = i * 1000;
        
        if (i % 1000 == 0) {
            printf("   Progress: %d documents inserted\n", i);
        }
        
        if (hash_index_insert(hash, key, strlen(key), value) != 0) {
            fprintf(stderr, "   FAIL: Could not insert %s (document #%d)\n", key, i);
            fail_count++;
            if (fail_count > 10) {
                fprintf(stderr, "   Too many failures, stopping test\n");
                break;
            }
        }
    }
    
    clock_t end = clock();
    double cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("   Insertion completed in %.2f seconds\n", cpu_time_used);
    printf("   Failed insertions: %d\n", fail_count);
    
    /* Test 2: Verify insertions with searches */
    printf("\n2. Verifying random insertions...\n");
    int verify_count = 100;
    int not_found = 0;
    
    for (int i = 0; i < verify_count; i++) {
        int idx = rand() % 10000;
        char key[64];
        snprintf(key, sizeof(key), "document-%08d-test", idx);
        uint64_t value;
        
        if (hash_index_search(hash, key, strlen(key), &value) != 0) {
            not_found++;
            fprintf(stderr, "   NOT FOUND: %s\n", key);
        } else if (value != (uint64_t)(idx * 1000)) {
            fprintf(stderr, "   VALUE MISMATCH: %s expected %llu got %llu\n", 
                   key, (unsigned long long)(idx * 1000), (unsigned long long)value);
        }
    }
    
    printf("   Verified %d random documents, %d not found\n", verify_count, not_found);
    
    /* Test 3: Statistics */
    printf("\n3. Hash Index Statistics:\n");
    uint64_t num_keys, num_buckets, avg_chain_length;
    hash_index_stats(hash, &num_keys, &num_buckets, &avg_chain_length);
    printf("   Keys: %llu\n", (unsigned long long)num_keys);
    printf("   Buckets: %llu\n", (unsigned long long)num_buckets);
    printf("   Avg chain length: %llu\n", (unsigned long long)avg_chain_length);
    
    /* Test 4: Delete some entries */
    printf("\n4. Deleting 1000 random entries...\n");
    int delete_count = 0;
    for (int i = 0; i < 1000; i++) {
        int idx = rand() % 10000;
        char key[64];
        snprintf(key, sizeof(key), "document-%08d-test", idx);
        
        if (hash_index_delete(hash, key, strlen(key)) == 0) {
            delete_count++;
        }
    }
    printf("   Successfully deleted %d entries\n", delete_count);
    
    /* Final statistics */
    hash_index_stats(hash, &num_keys, &num_buckets, &avg_chain_length);
    printf("\n5. Final Statistics:\n");
    printf("   Keys: %llu\n", (unsigned long long)num_keys);
    printf("   Buckets: %llu\n", (unsigned long long)num_buckets);
    printf("   Avg chain length: %llu\n", (unsigned long long)avg_chain_length);
    
    /* Clean up */
    printf("\n6. Cleaning up...\n");
    hash_index_destroy(hash);
    printf("   SUCCESS: Hash index destroyed\n");
    
    return 0;
}