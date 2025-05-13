#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "jsondb.h"
#include "jsondb/database/database.h"
#include "jsondb/utils/json.h"
#include "jsondb/utils/json_helpers.h"

#define TEST_LOG_FILE "../test_logs/unit_database.log"
#define TEST_DB_FILE "../test_data/test_database.json"

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
    
    log_test("Starting database unit tests");
}

void test_cleanup() {
    if (log_file) {
        fprintf(log_file, "Time: %ld ms\n", clock() / (CLOCKS_PER_SEC / 1000));
        fclose(log_file);
    }
}

/* Test database creation and initialization */
int test_database_init() {
    log_test("Testing database initialization");
    
    database_t *db = db_init(TEST_DB_FILE);
    if (!db) {
        log_test("FAIL: Failed to initialize database");
        return 0;
    }

    log_test("PASS: Database initialized successfully");
    db_close(db);
    return 1;
}

/* Test document insertion */
int test_document_insert() {
    log_test("Testing document insertion");
    
    database_t *db = db_init(TEST_DB_FILE);
    if (!db) {
        log_test("Failed to initialize database for insertion test");
        return 0;
    }

    // Create a test document
    json_value_t *doc = json_create_object();
    json_object_set(doc, "test_key", json_create_string("test_value"));
    json_object_set(doc, "number", json_create_number(42));

    // Insert the document
    json_value_t *result = db_insert_document(db, "test_collection", doc);

    if (!result) {
        log_test("FAIL: Failed to insert document");
        db_close(db);
        return 0;
    }

    json_free(result);
    log_test("PASS: Document inserted successfully");
    db_close(db);
    return 1;
}

/* Test document retrieval */
int test_document_get() {
    log_test("Testing document retrieval");
    
    database_t *db = db_init(TEST_DB_FILE);
    if (!db) {
        log_test("Failed to initialize database for retrieval test");
        return 0;
    }

    // Create and insert a test document first
    json_value_t *doc = json_create_object();
    json_object_set(doc, "test_key", json_create_string("test_value"));
    json_object_set(doc, "number", json_create_number(42));
    json_object_set(doc, "_id", json_create_string("get_test_id"));

    json_value_t *insert_result = db_insert_document(db, "test_collection", doc);
    if (!insert_result) {
        log_test("Failed to insert document for retrieval test");
        db_close(db);
        return 0;
    }
    json_free(insert_result);

    // Now retrieve the document
    json_value_t *retrieved_doc = db_get_document(db, "test_collection", "get_test_id");

    if (!retrieved_doc) {
        log_test("FAIL: Failed to retrieve document");
        db_close(db);
        return 0;
    }

    // Verify document contents
    json_value_t* test_key_value = json_object_get(retrieved_doc, "test_key");
    if (!test_key_value || json_get_type(test_key_value) != JSON_STRING ||
        strcmp(json_get_string(test_key_value), "test_value") != 0) {
        log_test("FAIL: Retrieved document has incorrect data");
        json_free(retrieved_doc);
        db_close(db);
        return 0;
    }

    json_free(retrieved_doc);
    log_test("PASS: Document retrieved successfully with correct data");
    db_close(db);
    return 1;
}

/* Test document update */
int test_document_update() {
    log_test("Testing document update");
    
    database_t *db = db_init(TEST_DB_FILE);
    if (!db) {
        log_test("Failed to initialize database for update test");
        return 0;
    }

    // Create and insert a test document first
    json_value_t *doc = json_create_object();
    json_object_set(doc, "update_key", json_create_string("original_value"));
    json_object_set(doc, "_id", json_create_string("update_test_id"));

    json_value_t *insert_result = db_insert_document(db, "test_collection", doc);
    if (!insert_result) {
        log_test("Failed to insert document for update test");
        db_close(db);
        return 0;
    }
    json_free(insert_result);

    // Create update document
    json_value_t *update_doc = json_create_object();
    json_object_set(update_doc, "update_key", json_create_string("updated_value"));
    json_object_set(update_doc, "_id", json_create_string("update_test_id"));

    // Update the document
    json_value_t *update_result = db_update_document(db, "test_collection", "update_test_id", update_doc);

    if (!update_result) {
        log_test("FAIL: Failed to update document");
        db_close(db);
        return 0;
    }
    json_free(update_result);

    // Verify the update
    json_value_t *retrieved_doc = db_get_document(db, "test_collection", "update_test_id");
    if (!retrieved_doc) {
        log_test("FAIL: Failed to retrieve updated document");
        db_close(db);
        return 0;
    }

    json_value_t *updated_value_json = json_object_get(retrieved_doc, "update_key");
    if (!updated_value_json || json_get_type(updated_value_json) != JSON_STRING ||
        strcmp(json_get_string(updated_value_json), "updated_value") != 0) {
        log_test("FAIL: Document was not updated correctly");
        json_free(retrieved_doc);
        db_close(db);
        return 0;
    }

    json_free(retrieved_doc);
    log_test("PASS: Document updated successfully");
    db_close(db);
    return 1;
}

/* Test document deletion */
int test_document_delete() {
    log_test("Testing document deletion");
    
    database_t *db = db_init(TEST_DB_FILE);
    if (!db) {
        log_test("Failed to initialize database for deletion test");
        return 0;
    }

    // Create and insert a test document first
    json_value_t *doc = json_create_object();
    json_object_set(doc, "delete_key", json_create_string("delete_value"));
    json_object_set(doc, "_id", json_create_string("delete_test_id"));

    json_value_t *insert_result = db_insert_document(db, "test_collection", doc);
    if (!insert_result) {
        log_test("Failed to insert document for deletion test");
        db_close(db);
        return 0;
    }
    json_free(insert_result);

    // Delete the document
    int delete_result = db_delete_document(db, "test_collection", "delete_test_id");

    if (delete_result != 0) {
        log_test("FAIL: Failed to delete document");
        db_close(db);
        return 0;
    }

    // Verify deletion
    json_value_t *retrieved_doc = db_get_document(db, "test_collection", "delete_test_id");
    if (retrieved_doc) {
        log_test("FAIL: Document still exists after deletion");
        json_free(retrieved_doc);
        db_close(db);
        return 0;
    }

    log_test("PASS: Document deleted successfully");
    db_close(db);
    return 1;
}

/* Test error handling for invalid inputs */
int test_error_handling() {
    log_test("Testing error handling for invalid inputs");
    
    database_t *db = db_init(TEST_DB_FILE);
    if (!db) {
        log_test("Failed to initialize database for error handling test");
        return 0;
    }

    // Test with NULL document
    json_value_t *result = db_insert_document(db, "test_collection", NULL);
    if (result) {
        log_test("FAIL: Insert with NULL document should fail but succeeded");
        json_free(result);
        db_close(db);
        return 0;
    }

    // Test with NULL collection name
    json_value_t *doc = json_create_object();
    json_object_set(doc, "_id", json_create_string("test_id"));
    result = db_insert_document(db, NULL, doc);
    if (result) {
        log_test("FAIL: Insert with NULL collection should fail but succeeded");
        json_free(result);
        json_free(doc);
        db_close(db);
        return 0;
    }

    // Test with missing document ID
    json_value_t *doc2 = json_create_object();
    result = db_insert_document(db, "test_collection", doc2);
    if (result) {
        log_test("FAIL: Insert with missing document ID should fail but succeeded");
        json_free(result);
        db_close(db);
        return 0;
    }

    json_free(doc);
    json_free(doc2);
    log_test("PASS: Error handling for invalid inputs works correctly");
    db_close(db);
    return 1;
}

int main() {
    int success_count = 0;
    int total_tests = 0;
    
    test_init();
    
    // Run the tests
    total_tests++;
    success_count += test_database_init();
    
    total_tests++;
    success_count += test_document_insert();
    
    total_tests++;
    success_count += test_document_get();
    
    total_tests++;
    success_count += test_document_update();
    
    total_tests++;
    success_count += test_document_delete();
    
    total_tests++;
    success_count += test_error_handling();
    
    // Print summary
    printf("\nTest Summary: %d/%d tests passed\n", success_count, total_tests);
    log_test("\nDetails: Database core functionality test suite");
    
    if (success_count == total_tests) {
        log_test("PASS: All database unit tests passed");
    } else {
        log_test("FAIL: Some database unit tests failed");
    }
    
    test_cleanup();
    return (success_count == total_tests) ? EXIT_SUCCESS : EXIT_FAILURE;
}