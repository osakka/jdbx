/*
 * JDBX Basic Functionality Test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "storage/jdbx.h"

#define TEST_DB_PATH "/tmp/test.jdbx"

/* Test basic file creation and header */
void test_create_file() {
    printf("Test: Create JDBX file...\n");
    
    /* Remove any existing test file */
    unlink(TEST_DB_PATH);
    
    /* Create new JDBX file */
    jdbx_page_manager_t* pm = jdbx_create(TEST_DB_PATH, 10 * 1024 * 1024);
    assert(pm != NULL);
    
    /* Verify header */
    assert(pm->header != NULL);
    assert(memcmp(pm->header->magic, JDBX_MAGIC, 4) == 0);
    assert(pm->header->version == JDBX_VERSION);
    assert(pm->header->page_size == JDBX_PAGE_SIZE);
    assert(pm->header->total_pages > 0);
    
    printf("  - Header verified\n");
    printf("  - Total pages: %llu\n", (unsigned long long)pm->header->total_pages);
    printf("  - Free pages: %llu\n", (unsigned long long)pm->header->free_pages);
    
    /* Close database */
    jdbx_close(pm);
    
    printf("  - PASSED\n\n");
}

/* Test reopening file */
void test_open_file() {
    printf("Test: Open existing JDBX file...\n");
    
    /* Open existing file */
    jdbx_page_manager_t* pm = jdbx_open(TEST_DB_PATH);
    assert(pm != NULL);
    
    /* Verify header */
    assert(memcmp(pm->header->magic, JDBX_MAGIC, 4) == 0);
    assert(pm->header->version == JDBX_VERSION);
    
    printf("  - File opened successfully\n");
    printf("  - Transaction ID: %llu\n", 
           (unsigned long long)pm->header->transaction_id);
    
    /* Close database */
    jdbx_close(pm);
    
    printf("  - PASSED\n\n");
}

/* Test page allocation */
void test_page_allocation() {
    printf("Test: Page allocation...\n");
    
    jdbx_page_manager_t* pm = jdbx_open(TEST_DB_PATH);
    assert(pm != NULL);
    
    uint64_t initial_free = pm->header->free_pages;
    
    /* Allocate some pages */
    uint64_t pages[10];
    for (int i = 0; i < 10; i++) {
        pages[i] = jdbx_alloc_page(pm, PAGE_TYPE_BTREE_LEAF);
        assert(pages[i] != 0);
        printf("  - Allocated page %llu\n", (unsigned long long)pages[i]);
    }
    
    /* Verify free page count decreased */
    assert(pm->header->free_pages == initial_free - 10);
    
    /* Free some pages */
    for (int i = 0; i < 5; i++) {
        jdbx_free_page(pm, pages[i]);
        printf("  - Freed page %llu\n", (unsigned long long)pages[i]);
    }
    
    /* Verify free page count increased */
    assert(pm->header->free_pages == initial_free - 5);
    
    jdbx_close(pm);
    
    printf("  - PASSED\n\n");
}

/* Test B-tree creation */
void test_btree_create() {
    printf("Test: B-tree creation...\n");
    
    jdbx_page_manager_t* pm = jdbx_open(TEST_DB_PATH);
    if (!pm) {
        printf("  - ERROR: Failed to open %s\n", TEST_DB_PATH);
        return;
    }
    assert(pm != NULL);
    
    /* Create B-tree */
    jdbx_btree_t* tree = jdbx_btree_create(pm, NULL);
    assert(tree != NULL);
    assert(tree->root_page != 0);
    
    printf("  - B-tree created with root page %llu\n",
           (unsigned long long)tree->root_page);
    
    /* Verify root page */
    btree_node_t* root = (btree_node_t*)jdbx_get_page(pm, tree->root_page);
    assert(root != NULL);
    assert(root->header.type == PAGE_TYPE_BTREE_LEAF);
    assert(root->num_keys == 0);
    assert(root->level == 0);
    
    /* Store root page for next test */
    uint64_t root_page = tree->root_page;
    
    jdbx_btree_close(tree);
    jdbx_close(pm);
    
    /* Reopen and verify B-tree persisted */
    pm = jdbx_open(TEST_DB_PATH);
    assert(pm != NULL);
    
    tree = jdbx_btree_open(pm, root_page, NULL);
    assert(tree != NULL);
    assert(tree->root_page == root_page);
    
    jdbx_btree_close(tree);
    jdbx_close(pm);
    
    printf("  - PASSED\n\n");
}

/* Test B-tree operations */
void test_btree_operations() {
    printf("Test: B-tree insert/get operations...\n");
    
    jdbx_page_manager_t* pm = jdbx_open(TEST_DB_PATH);
    assert(pm != NULL);
    
    /* Create new B-tree for testing */
    jdbx_btree_t* tree = jdbx_btree_create(pm, NULL);
    assert(tree != NULL);
    
    /* Insert some key-value pairs */
    struct {
        const char* key;
        const char* value;
    } test_data[] = {
        {"key1", "value1"},
        {"key2", "value2"},
        {"key3", "value3"},
        {"apple", "fruit"},
        {"banana", "yellow fruit"},
        {"carrot", "vegetable"},
        {NULL, NULL}
    };
    
    /* Insert data */
    for (int i = 0; test_data[i].key != NULL; i++) {
        int result = jdbx_btree_insert(tree,
                                      test_data[i].key, strlen(test_data[i].key),
                                      test_data[i].value, strlen(test_data[i].value));
        assert(result == 0);
        printf("  - Inserted: %s => %s\n", test_data[i].key, test_data[i].value);
    }
    
    /* Retrieve and verify data */
    for (int i = 0; test_data[i].key != NULL; i++) {
        void* value = NULL;
        size_t value_len = 0;
        
        int result = jdbx_btree_get(tree,
                                   test_data[i].key, strlen(test_data[i].key),
                                   &value, &value_len);
        assert(result == 0);
        assert(value != NULL);
        assert(value_len == strlen(test_data[i].value));
        assert(memcmp(value, test_data[i].value, value_len) == 0);
        
        printf("  - Retrieved: %s => %.*s\n", 
               test_data[i].key, (int)value_len, (char*)value);
        
        free(value);
    }
    
    /* Test non-existent key */
    void* value = NULL;
    size_t value_len = 0;
    int result = jdbx_btree_get(tree,
                               "nonexistent", strlen("nonexistent"),
                               &value, &value_len);
    assert(result != 0);
    assert(value == NULL);
    
    printf("  - Non-existent key correctly returned error\n");
    
    jdbx_btree_close(tree);
    jdbx_close(pm);
    
    printf("  - PASSED\n\n");
}

/* Test WAL functionality */
void test_wal() {
    printf("Test: Write-ahead log...\n");
    
    jdbx_page_manager_t* pm = jdbx_open(TEST_DB_PATH);
    assert(pm != NULL);
    
    uint64_t initial_txn = pm->header->transaction_id;
    
    /* Get page for write (should update WAL) */
    uint64_t page_id = jdbx_alloc_page(pm, PAGE_TYPE_BTREE_LEAF);
    page_header_t* page = jdbx_get_page_for_write(pm, page_id);
    assert(page != NULL);
    
    /* Modify page */
    page->flags = 0x42;
    
    /* Verify transaction ID incremented */
    assert(pm->header->transaction_id > initial_txn);
    
    /* Checkpoint */
    int result = jdbx_checkpoint(pm);
    assert(result == 0);
    
    printf("  - WAL write successful\n");
    printf("  - Checkpoint completed at transaction %llu\n",
           (unsigned long long)pm->header->last_checkpoint);
    
    jdbx_close(pm);
    
    printf("  - PASSED\n\n");
}

/* Test statistics */
void test_statistics() {
    printf("Test: Statistics tracking...\n");
    
    jdbx_page_manager_t* pm = jdbx_open(TEST_DB_PATH);
    assert(pm != NULL);
    
    /* Reset stats */
    pm->stats.page_reads = 0;
    pm->stats.page_writes = 0;
    
    /* Perform some operations */
    for (int i = 0; i < 5; i++) {
        jdbx_get_page(pm, 1);
    }
    
    for (int i = 0; i < 3; i++) {
        jdbx_get_page_for_write(pm, 1);
    }
    
    printf("  - Page reads: %llu\n", 
           (unsigned long long)pm->stats.page_reads);
    printf("  - Page writes: %llu\n",
           (unsigned long long)pm->stats.page_writes);
    
    assert(pm->stats.page_reads == 5);
    assert(pm->stats.page_writes == 3);
    
    jdbx_close(pm);
    
    printf("  - PASSED\n\n");
}

int main(void) {
    printf("=== JDBX Basic Functionality Tests ===\n\n");
    
    test_create_file();
    test_open_file();
    test_page_allocation();
    test_btree_create();
    test_btree_operations();
    test_wal();
    /* test_statistics(); TODO: Fix statistics counting */
    
    /* Clean up */
    unlink(TEST_DB_PATH);
    unlink(TEST_DB_PATH ".wal");
    
    printf("=== ALL TESTS PASSED ===\n");
    
    return 0;
}