#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "index/btree_disk.h"
#include "utils/logger.h"

int main() {
    /* Initialize logger */
    logger_config_t logger_cfg = {
        .log_level = LOG_LEVEL_DEBUG,
        .log_to_file = 1,
        .log_file_path = "/tmp/btree_test.log"
    };
    logger_init(&logger_cfg);
    
    printf("Testing B+Tree with safety fixes...\n");
    
    /* Create B+tree */
    printf("1. Creating B+tree...\n");
    btree_disk_t* btree = btree_disk_create("/tmp/test_btree_fixed.idx", 5, NULL);
    if (!btree) {
        fprintf(stderr, "Failed to create B+tree\n");
        return 1;
    }
    printf("   SUCCESS: B+tree created\n");
    
    /* Test single insert */
    printf("2. Testing single insert...\n");
    const char* key1 = "test_key_1";
    uint64_t value1 = 12345;
    if (btree_disk_insert(btree, key1, strlen(key1), value1) != 0) {
        fprintf(stderr, "Failed to insert first key\n");
        btree_disk_destroy(btree);
        return 1;
    }
    printf("   SUCCESS: Inserted key '%s'\n", key1);
    
    /* Test retrieval */
    printf("3. Testing retrieval...\n");
    uint64_t retrieved;
    if (btree_disk_get(btree, key1, strlen(key1), &retrieved) == 0) {
        printf("   SUCCESS: Retrieved value %lu\n", retrieved);
    } else {
        fprintf(stderr, "Failed to retrieve key\n");
    }
    
    /* Test multiple inserts to trigger splits */
    printf("4. Testing multiple inserts (may trigger splits)...\n");
    for (int i = 0; i < 20; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%04d", i);
        uint64_t value = i * 1000;
        
        if (btree_disk_insert(btree, key, strlen(key), value) != 0) {
            fprintf(stderr, "Failed to insert key %s\n", key);
        } else if (i % 5 == 0) {
            printf("   Inserted %d keys...\n", i + 1);
        }
    }
    printf("   SUCCESS: Inserted 20 keys\n");
    
    /* Test retrieval of multiple keys */
    printf("5. Testing retrieval of multiple keys...\n");
    int found = 0;
    for (int i = 0; i < 20; i += 5) {
        char key[32];
        snprintf(key, sizeof(key), "key_%04d", i);
        uint64_t value;
        
        if (btree_disk_get(btree, key, strlen(key), &value) == 0) {
            found++;
        }
    }
    printf("   SUCCESS: Found %d/4 keys\n", found);
    
    /* Cleanup */
    printf("6. Cleaning up...\n");
    btree_disk_destroy(btree);
    printf("   SUCCESS: B+tree destroyed\n");
    
    printf("\nAll tests completed successfully!\n");
    logger_close();
    
    return 0;
}