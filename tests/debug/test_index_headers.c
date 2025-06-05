/* Simple test to check if index headers can be included without crashes */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Try to include the problematic headers */
#ifdef TEST_BTREE
    #include "index/btree_disk.h"
    void test_btree_struct_sizes() {
        printf("sizeof(btree_node_t) = %zu\n", sizeof(btree_node_t));
        printf("sizeof(btree_disk_t) = %zu\n", sizeof(btree_disk_t));
        printf("BTREE_PAGE_SIZE = %d\n", BTREE_PAGE_SIZE);
        printf("BTREE_DEFAULT_ORDER = %d\n", BTREE_DEFAULT_ORDER);
    }
#endif

#ifdef TEST_HASH
    #include "index/hash_index.h"
    void test_hash_struct_sizes() {
        printf("sizeof(hash_bucket_t) = %zu\n", sizeof(hash_bucket_t));
        printf("sizeof(hash_entry_t) = %zu\n", sizeof(hash_entry_t));
        printf("sizeof(hash_index_t) = %zu\n", sizeof(hash_index_t));
        printf("HASH_BUCKET_SIZE = %d\n", HASH_BUCKET_SIZE);
    }
#endif

int main(int argc, char* argv[]) {
    printf("=== Index Header Test ===\n");
    
    if (argc > 1 && strcmp(argv[1], "btree") == 0) {
        #ifdef TEST_BTREE
            printf("\nTesting B+tree structures:\n");
            test_btree_struct_sizes();
        #else
            printf("Compile with -DTEST_BTREE to test B+tree headers\n");
        #endif
    } else if (argc > 1 && strcmp(argv[1], "hash") == 0) {
        #ifdef TEST_HASH
            printf("\nTesting hash index structures:\n");
            test_hash_struct_sizes();
        #else
            printf("Compile with -DTEST_HASH to test hash headers\n");
        #endif
    } else {
        printf("Usage: %s [btree|hash]\n", argv[0]);
        printf("Compile with -DTEST_BTREE or -DTEST_HASH\n");
    }
    
    return 0;
}

/* Compile commands:
 * gcc -g -O0 -I../../src/include -DTEST_BTREE -o test_btree_headers test_index_headers.c
 * gcc -g -O0 -I../../src/include -DTEST_HASH -o test_hash_headers test_index_headers.c
 */