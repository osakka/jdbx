/*
 * Test JDBX reopen functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "storage/jdbx.h"
#include "utils/logger.h"

#define TEST_DB_PATH "/tmp/test_reopen.jdbx"

int main(void) {
    /* Initialize logger for debug output */
    logger_init("test_jdbx_reopen.log", LOG_LEVEL_DEBUG);
    printf("=== JDBX Reopen Test ===\n\n");
    
    /* Remove any existing test file */
    unlink(TEST_DB_PATH);
    unlink(TEST_DB_PATH ".wal");
    
    /* Step 1: Create database */
    printf("Step 1: Create database\n");
    jdbx_page_manager_t* pm = jdbx_create(TEST_DB_PATH, 5 * 1024 * 1024);
    if (!pm) {
        printf("ERROR: Failed to create database\n");
        return 1;
    }
    printf("  - Created successfully\n");
    
    /* Create a B-tree and add some data */
    jdbx_btree_t* tree = jdbx_btree_create(pm, NULL);
    if (!tree) {
        printf("ERROR: Failed to create B-tree\n");
        jdbx_close(pm);
        return 1;
    }
    
    uint64_t root_page = tree->root_page;
    printf("  - B-tree created at page %llu\n", (unsigned long long)root_page);
    
    /* Insert test data */
    jdbx_btree_insert(tree, "test", 4, "data", 4);
    printf("  - Inserted test data\n");
    
    /* Close everything */
    jdbx_btree_close(tree);
    jdbx_close(pm);
    printf("  - Closed database\n");
    
    /* Step 2: Check files exist */
    printf("\nStep 2: Verify files\n");
    if (access(TEST_DB_PATH, F_OK) != 0) {
        printf("ERROR: Database file doesn't exist\n");
        return 1;
    }
    printf("  - Database file exists\n");
    
    if (access(TEST_DB_PATH ".wal", F_OK) != 0) {
        printf("ERROR: WAL file doesn't exist\n");
        return 1;
    }
    printf("  - WAL file exists\n");
    
    /* Step 3: Reopen database */
    printf("\nStep 3: Reopen database\n");
    pm = jdbx_open(TEST_DB_PATH);
    if (!pm) {
        printf("ERROR: Failed to reopen database: %s\n", strerror(errno));
        return 1;
    }
    printf("  - Database reopened successfully\n");
    printf("  - Total pages: %llu\n", (unsigned long long)pm->header->total_pages);
    printf("  - Free pages: %llu\n", (unsigned long long)pm->header->free_pages);
    
    /* Step 4: Reopen B-tree */
    printf("\nStep 4: Reopen B-tree\n");
    tree = jdbx_btree_open(pm, root_page, NULL);
    if (!tree) {
        printf("ERROR: Failed to reopen B-tree\n");
        jdbx_close(pm);
        return 1;
    }
    printf("  - B-tree reopened successfully\n");
    
    /* Step 5: Verify data */
    printf("\nStep 5: Verify data\n");
    void* value = NULL;
    size_t value_len = 0;
    int result = jdbx_btree_get(tree, "test", 4, &value, &value_len);
    if (result == 0 && value != NULL) {
        printf("  - Data retrieved: %.*s\n", (int)value_len, (char*)value);
        free(value);
    } else {
        printf("ERROR: Failed to retrieve data\n");
    }
    
    /* Cleanup */
    jdbx_btree_close(tree);
    jdbx_close(pm);
    unlink(TEST_DB_PATH);
    unlink(TEST_DB_PATH ".wal");
    
    printf("\n=== Test completed successfully! ===\n");
    
    return 0;
}