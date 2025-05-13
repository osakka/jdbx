#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "jsondb.h"
#include "jsondb/database/database.h"
#include "jsondb/transaction/transaction.h"
#include "jsondb/transaction/transaction_retry.h"
#include "jsondb/utils/json.h"
#include "jsondb/utils/json_helpers.h"

#define TEST_LOG_FILE "../test_logs/unit_transaction.log"
#define TEST_DB_FILE "../test_data/test_transaction.json"

FILE *log_file = NULL;

void log_test(const char *message) {
    if (log_file) {
        fprintf(log_file, "[%s] %s\n", __TIME__, message);
    }
    printf("%s\n", message);
}

void test_init() {
    log_file = fopen(TEST_LOG_FILE, "w");
    if (!log_file) {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
    }
    
    log_test("Starting transaction unit tests");
}

void test_cleanup() {
    if (log_file) {
        fprintf(log_file, "Time: %ld ms\n", clock() / (CLOCKS_PER_SEC / 1000));
        fclose(log_file);
    }
}

/* Helper function to create a test database with some initial data */
database_t* create_test_database() {
    // Remove any existing test database
    remove(TEST_DB_FILE);
    
    database_t *db = database_init(TEST_DB_FILE);
    if (!db) {
        log_test("FAIL: Failed to initialize database for transaction tests");
        return NULL;
    }
    
    // Add some initial data
    for (int i = 0; i < 5; i++) {
        char doc_id[32];
        snprintf(doc_id, sizeof(doc_id), "doc_%d", i);
        
        json_value_t *doc = json_create_object();
        json_object_set(doc, "name", json_create_string("Initial Document"));
        json_object_set(doc, "value", json_create_number(i));
        
        if (database_insert_document(db, "test_collection", doc_id, doc) != 0) {
            log_test("FAIL: Failed to insert initial data into test database");
            database_free(db);
            return NULL;
        }
    }
    
    return db;
}

/* Test basic transaction operations */
int test_transaction_basics() {
    log_test("Testing basic transaction operations");
    
    database_t *db = create_test_database();
    if (!db) {
        return 0;
    }
    
    // Start a transaction
    transaction_t *tx = transaction_begin(db);
    if (!tx) {
        log_test("FAIL: Failed to begin transaction");
        database_free(db);
        return 0;
    }
    
    // Perform operations within the transaction
    json_value_t *doc1 = json_create_object();
    json_object_set(doc1, "name", json_create_string("Transaction Test"));
    json_object_set(doc1, "value", json_create_number(100));
    
    if (transaction_insert_document(tx, "test_collection", "tx_doc_1", doc1) != 0) {
        log_test("FAIL: Failed to insert document in transaction");
        transaction_rollback(tx);
        database_free(db);
        return 0;
    }
    
    // Update an existing document
    json_value_t *doc2 = json_create_object();
    json_object_set(doc2, "name", json_create_string("Updated in Transaction"));
    json_object_set(doc2, "value", json_create_number(200));
    
    if (transaction_update_document(tx, "test_collection", "doc_0", doc2) != 0) {
        log_test("FAIL: Failed to update document in transaction");
        transaction_rollback(tx);
        database_free(db);
        return 0;
    }
    
    // Commit the transaction
    if (transaction_commit(tx) != 0) {
        log_test("FAIL: Failed to commit transaction");
        database_free(db);
        return 0;
    }
    
    // Verify that changes are visible after commit
    json_value_t *retrieved_doc1 = database_get_document(db, "test_collection", "tx_doc_1");
    if (!retrieved_doc1) {
        log_test("FAIL: Inserted document not found after commit");
        database_free(db);
        return 0;
    }
    
    json_value_t *retrieved_doc2 = database_get_document(db, "test_collection", "doc_0");
    if (!retrieved_doc2) {
        log_test("FAIL: Updated document not found after commit");
        database_free(db);
        return 0;
    }
    
    json_value_t *name_value = json_object_get(retrieved_doc2, "name");
    if (!name_value || strcmp(json_get_string(name_value), "Updated in Transaction") != 0) {
        log_test("FAIL: Document not correctly updated in transaction");
        database_free(db);
        return 0;
    }
    
    log_test("PASS: Basic transaction operations succeeded");
    database_free(db);
    return 1;
}

/* Test transaction rollback */
int test_transaction_rollback() {
    log_test("Testing transaction rollback");
    
    database_t *db = create_test_database();
    if (!db) {
        return 0;
    }
    
    // Save a copy of an original document for verification
    json_value_t *original_doc = database_get_document(db, "test_collection", "doc_1");
    json_value_t *original_name = json_object_get(original_doc, "name");
    const char *orig_name_str = json_get_string(original_name);
    
    // Start a transaction
    transaction_t *tx = transaction_begin(db);
    if (!tx) {
        log_test("FAIL: Failed to begin transaction for rollback test");
        database_free(db);
        return 0;
    }
    
    // Perform operations within the transaction
    json_value_t *new_doc = json_create_object();
    json_object_set(new_doc, "name", json_create_string("Will Be Rolled Back"));
    json_object_set(new_doc, "value", json_create_number(999));
    
    if (transaction_insert_document(tx, "test_collection", "rollback_doc", new_doc) != 0) {
        log_test("FAIL: Failed to insert document in rollback test");
        transaction_rollback(tx);
        database_free(db);
        return 0;
    }
    
    // Update an existing document
    json_value_t *update_doc = json_create_object();
    json_object_set(update_doc, "name", json_create_string("Updated Then Rolled Back"));
    json_object_set(update_doc, "value", json_create_number(777));
    
    if (transaction_update_document(tx, "test_collection", "doc_1", update_doc) != 0) {
        log_test("FAIL: Failed to update document in rollback test");
        transaction_rollback(tx);
        database_free(db);
        return 0;
    }
    
    // Delete a document
    if (transaction_delete_document(tx, "test_collection", "doc_2") != 0) {
        log_test("FAIL: Failed to delete document in rollback test");
        transaction_rollback(tx);
        database_free(db);
        return 0;
    }
    
    // Roll back the transaction
    if (transaction_rollback(tx) != 0) {
        log_test("FAIL: Failed to roll back transaction");
        database_free(db);
        return 0;
    }
    
    // Verify that changes were rolled back
    
    // 1. The inserted document should not exist
    json_value_t *rollback_doc = database_get_document(db, "test_collection", "rollback_doc");
    if (rollback_doc) {
        log_test("FAIL: Inserted document still exists after rollback");
        database_free(db);
        return 0;
    }
    
    // 2. The updated document should have its original value
    json_value_t *updated_doc = database_get_document(db, "test_collection", "doc_1");
    if (!updated_doc) {
        log_test("FAIL: Could not find document that was updated then rolled back");
        database_free(db);
        return 0;
    }
    
    json_value_t *name_value = json_object_get(updated_doc, "name");
    if (!name_value || strcmp(json_get_string(name_value), orig_name_str) != 0) {
        log_test("FAIL: Document not correctly rolled back to original value");
        database_free(db);
        return 0;
    }
    
    // 3. The deleted document should still exist
    json_value_t *deleted_doc = database_get_document(db, "test_collection", "doc_2");
    if (!deleted_doc) {
        log_test("FAIL: Document delete not rolled back properly");
        database_free(db);
        return 0;
    }
    
    log_test("PASS: Transaction rollback succeeded");
    database_free(db);
    return 1;
}

/* Test transaction isolation */
int test_transaction_isolation() {
    log_test("Testing transaction isolation");
    
    database_t *db = create_test_database();
    if (!db) {
        return 0;
    }
    
    // Start transaction 1
    transaction_t *tx1 = transaction_begin(db);
    if (!tx1) {
        log_test("FAIL: Failed to begin transaction 1");
        database_free(db);
        return 0;
    }
    
    // Modify a document in transaction 1
    json_value_t *doc1 = json_create_object();
    json_object_set(doc1, "name", json_create_string("Modified in TX1"));
    json_object_set(doc1, "value", json_create_number(101));
    
    if (transaction_update_document(tx1, "test_collection", "doc_3", doc1) != 0) {
        log_test("FAIL: Failed to update document in transaction 1");
        transaction_rollback(tx1);
        database_free(db);
        return 0;
    }
    
    // Start transaction 2
    transaction_t *tx2 = transaction_begin(db);
    if (!tx2) {
        log_test("FAIL: Failed to begin transaction 2");
        transaction_rollback(tx1);
        database_free(db);
        return 0;
    }
    
    // Transaction 2 should see the original value of doc_3, not tx1's uncommitted change
    json_value_t *doc_in_tx2 = transaction_get_document(tx2, "test_collection", "doc_3");
    if (!doc_in_tx2) {
        log_test("FAIL: Failed to get document in transaction 2");
        transaction_rollback(tx1);
        transaction_rollback(tx2);
        database_free(db);
        return 0;
    }
    
    json_value_t *name_in_tx2 = json_object_get(doc_in_tx2, "name");
    if (!name_in_tx2 || strcmp(json_get_string(name_in_tx2), "Initial Document") != 0) {
        log_test("FAIL: Transaction 2 sees uncommitted changes from transaction 1");
        transaction_rollback(tx1);
        transaction_rollback(tx2);
        database_free(db);
        return 0;
    }
    
    // Modify the same document in transaction 2
    json_value_t *doc2 = json_create_object();
    json_object_set(doc2, "name", json_create_string("Modified in TX2"));
    json_object_set(doc2, "value", json_create_number(102));
    
    // This should not cause a conflict yet since tx1 has not committed
    if (transaction_update_document(tx2, "test_collection", "doc_3", doc2) != 0) {
        log_test("FAIL: Failed to update document in transaction 2");
        transaction_rollback(tx1);
        transaction_rollback(tx2);
        database_free(db);
        return 0;
    }
    
    // Commit transaction 1
    if (transaction_commit(tx1) != 0) {
        log_test("FAIL: Failed to commit transaction 1");
        transaction_rollback(tx2);
        database_free(db);
        return 0;
    }
    
    // Now committing transaction 2 should detect a conflict
    int result = transaction_commit(tx2);
    if (result == 0) {
        log_test("FAIL: Transaction 2 committed successfully despite write conflict");
        database_free(db);
        return 0;
    }
    
    // Verify that doc_3 has tx1's changes after conflict
    json_value_t *final_doc = database_get_document(db, "test_collection", "doc_3");
    if (!final_doc) {
        log_test("FAIL: Could not find document after conflict");
        database_free(db);
        return 0;
    }
    
    json_value_t *final_name = json_object_get(final_doc, "name");
    if (!final_name || strcmp(json_get_string(final_name), "Modified in TX1") != 0) {
        log_test("FAIL: Document has incorrect value after conflict");
        database_free(db);
        return 0;
    }
    
    log_test("PASS: Transaction isolation succeeded");
    database_free(db);
    return 1;
}

/* Test transaction retry mechanism */
int test_transaction_retry() {
    log_test("Testing transaction retry mechanism");
    
    database_t *db = create_test_database();
    if (!db) {
        return 0;
    }
    
    // Define a transaction function that will be retried
    int retry_count = 0;
    
    transaction_retry_result_t retry_func(transaction_t *tx, void *user_data) {
        retry_count++;
        char message[100];
        snprintf(message, sizeof(message), "Executing transaction retry attempt %d", retry_count);
        log_test(message);
        
        // Insert a document in the transaction
        json_value_t *doc = json_create_object();
        json_object_set(doc, "name", json_create_string("Retry Test"));
        json_object_set(doc, "retry_count", json_create_number(retry_count));
        json_object_set(doc, "timestamp", json_create_number(time(NULL)));
        
        if (transaction_insert_document(tx, "test_collection", "retry_doc", doc) != 0) {
            log_test("FAIL: Failed to insert document in retry transaction");
            return TRANSACTION_RETRY_ERROR;
        }
        
        // For testing, we'll simulate a conflict on the first attempt
        if (retry_count == 1) {
            log_test("Simulating conflict for retry test");
            return TRANSACTION_RETRY_CONFLICT;
        }
        
        return TRANSACTION_RETRY_COMMIT;
    }
    
    // Execute the transaction with automatic retry
    int max_retries = 3;
    int result = transaction_execute_with_retry(db, retry_func, NULL, max_retries);
    
    if (result != 0) {
        log_test("FAIL: Transaction retry mechanism failed");
        database_free(db);
        return 0;
    }
    
    // Verify that retry happened
    if (retry_count < 2) {
        log_test("FAIL: Transaction was not retried as expected");
        database_free(db);
        return 0;
    }
    
    // Verify that the document exists with the correct retry count
    json_value_t *final_doc = database_get_document(db, "test_collection", "retry_doc");
    if (!final_doc) {
        log_test("FAIL: Could not find document after retry");
        database_free(db);
        return 0;
    }
    
    json_value_t *count_val = json_object_get(final_doc, "retry_count");
    if (!count_val || json_get_number(count_val) != retry_count) {
        log_test("FAIL: Document has incorrect retry count");
        database_free(db);
        return 0;
    }
    
    log_test("PASS: Transaction retry mechanism succeeded");
    database_free(db);
    return 1;
}

int main() {
    int success_count = 0;
    int total_tests = 0;
    
    test_init();
    
    // Run the tests
    total_tests++;
    success_count += test_transaction_basics();
    
    total_tests++;
    success_count += test_transaction_rollback();
    
    total_tests++;
    success_count += test_transaction_isolation();
    
    total_tests++;
    success_count += test_transaction_retry();
    
    // Print summary
    printf("\nTest Summary: %d/%d tests passed\n", success_count, total_tests);
    log_test("\nDetails: Transaction system test suite");
    
    if (success_count == total_tests) {
        log_test("PASS: All transaction tests passed");
    } else {
        log_test("FAIL: Some transaction tests failed");
    }
    
    test_cleanup();
    return (success_count == total_tests) ? EXIT_SUCCESS : EXIT_FAILURE;
}