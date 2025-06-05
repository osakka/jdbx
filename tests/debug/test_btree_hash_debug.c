#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>

/* Segfault handler to print stack trace */
void segfault_handler(int sig) {
    void *array[10];
    size_t size;
    
    fprintf(stderr, "\n=== SEGMENTATION FAULT DETECTED ===\n");
    fprintf(stderr, "Signal %d received\n", sig);
    
    /* Get void*'s for all entries on the stack */
    size = backtrace(array, 10);
    
    /* Print out all the frames to stderr */
    fprintf(stderr, "Stack trace:\n");
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    
    exit(1);
}

/* Test B+tree basic operations */
void test_btree_basic() {
    printf("\n=== Testing B+Tree Basic Operations ===\n");
    
    /* Minimal includes to test compilation */
    printf("1. Testing header inclusion...\n");
    
    #ifdef HAVE_BTREE_DISK_H
    #include "index/btree_disk.h"
    
    printf("2. Creating B+tree with order 10...\n");
    btree_disk_t* btree = btree_disk_create("/tmp/test_debug.btree", 10, NULL);
    if (!btree) {
        fprintf(stderr, "ERROR: Failed to create B+tree\n");
        return;
    }
    printf("   SUCCESS: B+tree created\n");
    
    printf("3. Inserting single key...\n");
    const char* key = "test_key";
    uint64_t value = 12345;
    int result = btree_disk_insert(btree, key, strlen(key), value);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to insert key\n");
    } else {
        printf("   SUCCESS: Key inserted\n");
    }
    
    printf("4. Searching for key...\n");
    uint64_t found_value;
    result = btree_disk_search(btree, key, strlen(key), &found_value);
    if (result == 0 && found_value == value) {
        printf("   SUCCESS: Key found with correct value\n");
    } else {
        fprintf(stderr, "ERROR: Key not found or wrong value\n");
    }
    
    printf("5. Destroying B+tree...\n");
    btree_disk_destroy(btree);
    printf("   SUCCESS: B+tree destroyed\n");
    
    #else
    printf("WARNING: btree_disk.h not found or not configured\n");
    #endif
}

/* Test hash index basic operations */
void test_hash_basic() {
    printf("\n=== Testing Hash Index Basic Operations ===\n");
    
    #ifdef HAVE_HASH_INDEX_H
    #include "index/hash_index.h"
    
    printf("1. Creating hash index...\n");
    hash_index_t* hash = hash_index_create("/tmp/test_debug.hash", NULL);
    if (!hash) {
        fprintf(stderr, "ERROR: Failed to create hash index\n");
        return;
    }
    printf("   SUCCESS: Hash index created\n");
    
    printf("2. Inserting single key...\n");
    const char* key = "test_key";
    uint64_t value = 12345;
    int result = hash_index_insert(hash, key, strlen(key), value);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to insert key\n");
    } else {
        printf("   SUCCESS: Key inserted\n");
    }
    
    printf("3. Searching for key...\n");
    uint64_t found_value;
    result = hash_index_search(hash, key, strlen(key), &found_value);
    if (result == 0 && found_value == value) {
        printf("   SUCCESS: Key found with correct value\n");
    } else {
        fprintf(stderr, "ERROR: Key not found or wrong value\n");
    }
    
    printf("4. Destroying hash index...\n");
    hash_index_destroy(hash);
    printf("   SUCCESS: Hash index destroyed\n");
    
    #else
    printf("WARNING: hash_index.h not found or not configured\n");
    #endif
}

/* Test with increasing stress */
void test_stress(int num_keys) {
    printf("\n=== Stress Test with %d keys ===\n", num_keys);
    
    #ifdef HAVE_BTREE_DISK_H
    btree_disk_t* btree = btree_disk_create("/tmp/stress.btree", 50, NULL);
    if (!btree) {
        fprintf(stderr, "Failed to create B+tree for stress test\n");
        return;
    }
    
    /* Insert keys */
    printf("Inserting %d keys...\n", num_keys);
    for (int i = 0; i < num_keys; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%08d", i);
        
        if (btree_disk_insert(btree, key, strlen(key), i * 100) != 0) {
            fprintf(stderr, "Failed at key %d\n", i);
            break;
        }
        
        if (i % 1000 == 0) {
            printf("  Inserted %d keys...\n", i);
        }
    }
    
    /* Verify some keys */
    printf("Verifying random keys...\n");
    for (int i = 0; i < 10; i++) {
        int idx = rand() % num_keys;
        char key[32];
        snprintf(key, sizeof(key), "key_%08d", idx);
        
        uint64_t value;
        if (btree_disk_search(btree, key, strlen(key), &value) == 0) {
            if (value != idx * 100) {
                fprintf(stderr, "Wrong value for key %s\n", key);
            }
        } else {
            fprintf(stderr, "Key %s not found\n", key);
        }
    }
    
    btree_disk_destroy(btree);
    printf("Stress test completed\n");
    #endif
}

int main(int argc, char* argv[]) {
    /* Install signal handler */
    signal(SIGSEGV, segfault_handler);
    signal(SIGABRT, segfault_handler);
    
    printf("=== B+Tree and Hash Index Debug Test ===\n");
    printf("This test will help identify crash points\n\n");
    
    /* Check if we can include the headers */
    #if __has_include("index/btree_disk.h")
        #define HAVE_BTREE_DISK_H
        printf("✓ btree_disk.h found\n");
    #else
        printf("✗ btree_disk.h NOT found\n");
    #endif
    
    #if __has_include("index/hash_index.h")
        #define HAVE_HASH_INDEX_H
        printf("✓ hash_index.h found\n");
    #else
        printf("✗ hash_index.h NOT found\n");
    #endif
    
    /* Run tests based on command line args */
    if (argc > 1) {
        if (strcmp(argv[1], "btree") == 0) {
            test_btree_basic();
        } else if (strcmp(argv[1], "hash") == 0) {
            test_hash_basic();
        } else if (strcmp(argv[1], "stress") == 0) {
            int num_keys = argc > 2 ? atoi(argv[2]) : 1000;
            test_stress(num_keys);
        } else {
            printf("Usage: %s [btree|hash|stress [num_keys]]\n", argv[0]);
        }
    } else {
        /* Run all tests */
        test_btree_basic();
        test_hash_basic();
        test_stress(100);
    }
    
    printf("\n=== All tests completed ===\n");
    return 0;
}

/* Makefile for this test:
 *
 * test_btree_hash_debug: test_btree_hash_debug.c
 *     gcc -g -O0 -I../../include -o test_btree_hash_debug test_btree_hash_debug.c \
 *         -L../../build/lib -ljsondb -lpthread -lm \
 *         -fsanitize=address -fno-omit-frame-pointer
 *
 * clean:
 *     rm -f test_btree_hash_debug
 *
 * Run with:
 *     ./test_btree_hash_debug btree    # Test only B+tree
 *     ./test_btree_hash_debug hash     # Test only hash
 *     ./test_btree_hash_debug stress 10000  # Stress test with 10000 keys
 */