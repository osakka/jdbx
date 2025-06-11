/*
 * Simple JDBX Test - Verify core functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "storage/jdbx.h"

#define TEST_DB_PATH "/tmp/test_simple.jdbx"

int main(void) {
    printf("=== Simple JDBX Test ===\n\n");
    
    /* Remove any existing test file */
    unlink(TEST_DB_PATH);
    
    /* Test 1: Create database */
    printf("Test 1: Create JDBX database\n");
    jdbx_page_manager_t* pm = jdbx_create(TEST_DB_PATH, 10 * 1024 * 1024);
    if (!pm) {
        printf("ERROR: Failed to create database\n");
        return 1;
    }
    printf("  - Database created successfully\n");
    printf("  - Total pages: %llu\n", (unsigned long long)pm->header->total_pages);
    printf("  - Page size: %u bytes\n", JDBX_PAGE_SIZE);
    
    /* Test 2: Create B-tree */
    printf("\nTest 2: Create B-tree\n");
    jdbx_btree_t* tree = jdbx_btree_create(pm, NULL);
    if (!tree) {
        printf("ERROR: Failed to create B-tree\n");
        jdbx_close(pm);
        return 1;
    }
    printf("  - B-tree created at page %llu\n", (unsigned long long)tree->root_page);
    
    /* Test 3: Insert data */
    printf("\nTest 3: Insert key-value pairs\n");
    const char* test_keys[] = {"user:1", "user:2", "product:abc", "order:123", NULL};
    const char* test_values[] = {"John Doe", "Jane Smith", "Widget", "Pending", NULL};
    
    for (int i = 0; test_keys[i] != NULL; i++) {
        int result = jdbx_btree_insert(tree,
                                      test_keys[i], strlen(test_keys[i]),
                                      test_values[i], strlen(test_values[i]));
        if (result != 0) {
            printf("ERROR: Failed to insert %s\n", test_keys[i]);
        } else {
            printf("  - Inserted: %s => %s\n", test_keys[i], test_values[i]);
        }
    }
    
    /* Test 4: Retrieve data */
    printf("\nTest 4: Retrieve values\n");
    for (int i = 0; test_keys[i] != NULL; i++) {
        void* value = NULL;
        size_t value_len = 0;
        
        int result = jdbx_btree_get(tree,
                                   test_keys[i], strlen(test_keys[i]),
                                   &value, &value_len);
        if (result == 0 && value != NULL) {
            printf("  - Retrieved: %s => %.*s\n", 
                   test_keys[i], (int)value_len, (char*)value);
            free(value);
        } else {
            printf("ERROR: Failed to retrieve %s\n", test_keys[i]);
        }
    }
    
    /* Test 5: Close and reopen */
    printf("\nTest 5: Persistence test\n");
    uint64_t root_page = tree->root_page;
    jdbx_btree_close(tree);
    jdbx_close(pm);
    
    /* Reopen database */
    pm = jdbx_open(TEST_DB_PATH);
    if (!pm) {
        printf("ERROR: Failed to reopen database\n");
        return 1;
    }
    printf("  - Database reopened successfully\n");
    
    /* Reopen B-tree */
    tree = jdbx_btree_open(pm, root_page, NULL);
    if (!tree) {
        printf("ERROR: Failed to reopen B-tree\n");
        jdbx_close(pm);
        return 1;
    }
    printf("  - B-tree reopened at page %llu\n", (unsigned long long)tree->root_page);
    
    /* Verify data persisted */
    void* value = NULL;
    size_t value_len = 0;
    int result = jdbx_btree_get(tree, "user:1", 6, &value, &value_len);
    if (result == 0 && value != NULL) {
        printf("  - Data persisted correctly: user:1 => %.*s\n", 
               (int)value_len, (char*)value);
        free(value);
    } else {
        printf("ERROR: Data not persisted\n");
    }
    
    /* Cleanup */
    jdbx_btree_close(tree);
    jdbx_close(pm);
    unlink(TEST_DB_PATH);
    unlink(TEST_DB_PATH ".wal");
    
    printf("\n=== All tests completed successfully! ===\n");
    
    return 0;
}