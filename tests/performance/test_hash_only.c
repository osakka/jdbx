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
    
    printf("=== Testing Hash Index Only ===\n\n");
    
    /* Remove old test file */
    remove("/tmp/hash_test.idx");
    
    /* Test 1: Hash index creation */
    printf("1. Creating Hash Index...\n");
    hash_index_t* hash = hash_index_create("/tmp/hash_test.idx", NULL);
    if (!hash) {
        fprintf(stderr, "Failed to create hash index\n");
        return 1;
    }
    printf("   SUCCESS: Hash index created\n");
    
    /* Test 2: Basic insertions (10 docs) */
    printf("\n2. Basic insertions (10 documents)...\n");
    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, sizeof(key), "doc-%04d", i);
        uint64_t value = i * 1000;
        
        printf("   Inserting %s -> %llu\n", key, (unsigned long long)value);
        
        if (hash_index_insert(hash, key, strlen(key), value) != 0) {
            fprintf(stderr, "   FAIL: Could not insert %s\n", key);
            hash_index_destroy(hash);
            return 1;
        }
    }
    printf("   SUCCESS: All 10 documents inserted\n");
    
    /* Test 3: Search the insertions */
    printf("\n3. Searching for inserted documents...\n");
    for (int i = 0; i < 10; i += 2) {
        char key[32];
        snprintf(key, sizeof(key), "doc-%04d", i);
        uint64_t value;
        
        if (hash_index_search(hash, key, strlen(key), &value) == 0) {
            printf("   Found %s -> %llu\n", key, (unsigned long long)value);
        } else {
            fprintf(stderr, "   NOT FOUND: %s\n", key);
        }
    }
    
    /* Test 4: Large scale insertion (500 docs) */
    printf("\n4. Large scale insertion (500 documents)...\n");
    int success_count = 0;
    for (int i = 10; i < 500; i++) {
        char key[32];
        snprintf(key, sizeof(key), "doc-%04d", i);
        uint64_t value = i * 1000;
        
        if (i % 50 == 0) {
            printf("   Progress: %d documents inserted\n", i);
        }
        
        if (hash_index_insert(hash, key, strlen(key), value) != 0) {
            fprintf(stderr, "   FAIL: Could not insert %s (document #%d)\n", key, i);
            break;
        }
        success_count++;
    }
    printf("   Inserted %d documents successfully\n", success_count + 10);
    
    /* Test 5: Hash index statistics */
    printf("\n5. Hash Index Statistics:\n");
    uint64_t num_keys, num_buckets, avg_chain_length;
    hash_index_stats(hash, &num_keys, &num_buckets, &avg_chain_length);
    printf("   Keys: %llu\n", (unsigned long long)num_keys);
    printf("   Buckets: %llu\n", (unsigned long long)num_buckets);
    printf("   Avg chain length: %llu\n", (unsigned long long)avg_chain_length);
    
    /* Clean up */
    printf("\n6. Cleaning up...\n");
    hash_index_destroy(hash);
    printf("   SUCCESS: Hash index destroyed\n");
    
    return 0;
}