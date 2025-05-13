#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "jsondb.h"
#include "jsondb/database/database.h"
#include "jsondb/utils/json.h"
#include "jsondb/utils/json_helpers.h"

#define TEST_LOG_FILE "../test_logs/performance_database.log"
#define TEST_DB_FILE "../test_data/perf_database.json"
#define TEST_COLLECTION "perf_test"

#define NUM_DOCUMENTS 1000
#define ITERATIONS 5
#define TEST_TIMEOUT_SEC 60

FILE *log_file = NULL;
clock_t start_time, end_time;

/* Timing and logging functions */
void start_timer() {
    start_time = clock();
}

double stop_timer() {
    end_time = clock();
    return ((double)(end_time - start_time)) / CLOCKS_PER_SEC * 1000.0; // ms
}

void log_test(const char *message) {
    if (log_file) {
        fprintf(log_file, "[%s] %s\n", __TIME__, message);
    }
    printf("%s\n", message);
}

void log_performance(const char *operation, double time_ms, int num_operations) {
    double ops_per_sec = num_operations / (time_ms / 1000.0);
    if (log_file) {
        fprintf(log_file, "PERF: %s - %.2f ms total, %.2f ops/sec\n", 
                operation, time_ms, ops_per_sec);
    }
    printf("PERF: %s - %.2f ms total, %.2f ops/sec\n", 
            operation, time_ms, ops_per_sec);
}

void test_init() {
    log_file = fopen(TEST_LOG_FILE, "w");
    if (!log_file) {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
    }
    
    log_test("Starting database performance tests");
    
    // Cleanup any existing test database
    remove(TEST_DB_FILE);
}

void test_cleanup() {
    if (log_file) {
        fprintf(log_file, "Total Test Time: %.2f sec\n", 
                ((double)(clock() - start_time)) / CLOCKS_PER_SEC);
        fclose(log_file);
    }
    
    // Cleanup test database
    remove(TEST_DB_FILE);
}

/* Initialize the database */
database_t* setup_database() {
    database_t* db = database_init(TEST_DB_FILE);
    if (!db) {
        log_test("Failed to initialize database");
        exit(EXIT_FAILURE);
    }
    return db;
}

/* Create test document with specified size */
json_value_t* create_test_document(int doc_size) {
    json_value_t* doc = json_create_object();
    if (!doc) {
        log_test("Failed to create document");
        return NULL;
    }
    
    // Add standard fields
    json_object_set(doc, "id", json_create_string("test_id"));
    json_object_set(doc, "timestamp", json_create_number(time(NULL)));
    
    // Add array of specified size to control document size
    json_value_t* array = json_create_array();
    if (!array) {
        log_test("Failed to create array");
        return doc;
    }
    
    // Fill array with items to reach target size
    char buffer[32];
    for (int i = 0; i < doc_size / 32; i++) {
        snprintf(buffer, sizeof(buffer), "item_%d", i);
        json_array_append(array, json_create_string(buffer));
    }
    
    json_object_set(doc, "data", array);
    return doc;
}

/* Test document insertion performance */
void test_insert_performance() {
    log_test("Testing document insertion performance");
    database_t* db = setup_database();
    
    double total_time = 0.0;
    
    for (int iter = 0; iter < ITERATIONS; iter++) {
        char message[100];
        snprintf(message, sizeof(message), "Iteration %d/%d", iter + 1, ITERATIONS);
        log_test(message);
        
        start_timer();
        for (int i = 0; i < NUM_DOCUMENTS; i++) {
            char doc_id[32];
            snprintf(doc_id, sizeof(doc_id), "doc_%d_%d", iter, i);
            
            json_value_t* doc = create_test_document(1024); // ~1KB document
            
            if (database_insert_document(db, TEST_COLLECTION, doc_id, doc) != 0) {
                log_test("Failed to insert document");
            }
        }
        double time_ms = stop_timer();
        total_time += time_ms;
        
        log_performance("Document Insertion", time_ms, NUM_DOCUMENTS);
    }
    
    log_performance("Average Document Insertion", total_time / ITERATIONS, NUM_DOCUMENTS);
    
    // Quick verification
    int count = 0;
    for (int iter = 0; iter < ITERATIONS; iter++) {
        for (int i = 0; i < NUM_DOCUMENTS; i++) {
            char doc_id[32];
            snprintf(doc_id, sizeof(doc_id), "doc_%d_%d", iter, i);
            
            if (database_get_document(db, TEST_COLLECTION, doc_id) != NULL) {
                count++;
            }
        }
    }
    
    char message[100];
    snprintf(message, sizeof(message), "Verification: %d/%d documents found", 
             count, NUM_DOCUMENTS * ITERATIONS);
    log_test(message);
    
    database_free(db);
}

/* Test document retrieval performance */
void test_retrieval_performance() {
    log_test("Testing document retrieval performance");
    database_t* db = setup_database();
    
    // Insert documents for retrieval test
    for (int i = 0; i < NUM_DOCUMENTS; i++) {
        char doc_id[32];
        snprintf(doc_id, sizeof(doc_id), "retrieval_doc_%d", i);
        
        json_value_t* doc = create_test_document(1024); // ~1KB document
        
        if (database_insert_document(db, TEST_COLLECTION, doc_id, doc) != 0) {
            log_test("Failed to insert document for retrieval test");
        }
    }
    
    double total_time = 0.0;
    
    for (int iter = 0; iter < ITERATIONS; iter++) {
        char message[100];
        snprintf(message, sizeof(message), "Iteration %d/%d", iter + 1, ITERATIONS);
        log_test(message);
        
        start_timer();
        int found = 0;
        for (int i = 0; i < NUM_DOCUMENTS; i++) {
            char doc_id[32];
            snprintf(doc_id, sizeof(doc_id), "retrieval_doc_%d", i);
            
            json_value_t* doc = database_get_document(db, TEST_COLLECTION, doc_id);
            if (doc) {
                found++;
            }
        }
        double time_ms = stop_timer();
        total_time += time_ms;
        
        snprintf(message, sizeof(message), "Found %d/%d documents", found, NUM_DOCUMENTS);
        log_test(message);
        log_performance("Document Retrieval", time_ms, NUM_DOCUMENTS);
    }
    
    log_performance("Average Document Retrieval", total_time / ITERATIONS, NUM_DOCUMENTS);
    
    database_free(db);
}

/* Test document update performance */
void test_update_performance() {
    log_test("Testing document update performance");
    database_t* db = setup_database();
    
    // Insert documents for update test
    for (int i = 0; i < NUM_DOCUMENTS; i++) {
        char doc_id[32];
        snprintf(doc_id, sizeof(doc_id), "update_doc_%d", i);
        
        json_value_t* doc = create_test_document(1024); // ~1KB document
        
        if (database_insert_document(db, TEST_COLLECTION, doc_id, doc) != 0) {
            log_test("Failed to insert document for update test");
        }
    }
    
    double total_time = 0.0;
    
    for (int iter = 0; iter < ITERATIONS; iter++) {
        char message[100];
        snprintf(message, sizeof(message), "Iteration %d/%d", iter + 1, ITERATIONS);
        log_test(message);
        
        start_timer();
        for (int i = 0; i < NUM_DOCUMENTS; i++) {
            char doc_id[32];
            snprintf(doc_id, sizeof(doc_id), "update_doc_%d", i);
            
            json_value_t* doc = create_test_document(1024); // ~1KB document
            json_object_set(doc, "update_iteration", json_create_number(iter));
            
            if (database_update_document(db, TEST_COLLECTION, doc_id, doc) != 0) {
                log_test("Failed to update document");
            }
        }
        double time_ms = stop_timer();
        total_time += time_ms;
        
        log_performance("Document Update", time_ms, NUM_DOCUMENTS);
    }
    
    log_performance("Average Document Update", total_time / ITERATIONS, NUM_DOCUMENTS);
    
    database_free(db);
}

/* Test document deletion performance */
void test_delete_performance() {
    log_test("Testing document deletion performance");
    database_t* db = setup_database();
    
    // Insert documents for deletion test
    for (int i = 0; i < NUM_DOCUMENTS; i++) {
        char doc_id[32];
        snprintf(doc_id, sizeof(doc_id), "delete_doc_%d", i);
        
        json_value_t* doc = create_test_document(1024); // ~1KB document
        
        if (database_insert_document(db, TEST_COLLECTION, doc_id, doc) != 0) {
            log_test("Failed to insert document for deletion test");
        }
    }
    
    start_timer();
    int deleted = 0;
    for (int i = 0; i < NUM_DOCUMENTS; i++) {
        char doc_id[32];
        snprintf(doc_id, sizeof(doc_id), "delete_doc_%d", i);
        
        if (database_delete_document(db, TEST_COLLECTION, doc_id) == 0) {
            deleted++;
        }
    }
    double time_ms = stop_timer();
    
    char message[100];
    snprintf(message, sizeof(message), "Deleted %d/%d documents", deleted, NUM_DOCUMENTS);
    log_test(message);
    log_performance("Document Deletion", time_ms, NUM_DOCUMENTS);
    
    database_free(db);
}

/* Test performance with larger documents */
void test_large_document_performance() {
    log_test("Testing performance with large documents (10KB)");
    database_t* db = setup_database();
    
    double insert_time = 0.0;
    double retrieve_time = 0.0;
    double update_time = 0.0;
    double delete_time = 0.0;
    
    // Fewer documents for large doc test
    int num_large_docs = NUM_DOCUMENTS / 10;
    
    // Insert
    start_timer();
    for (int i = 0; i < num_large_docs; i++) {
        char doc_id[32];
        snprintf(doc_id, sizeof(doc_id), "large_doc_%d", i);
        
        json_value_t* doc = create_test_document(10 * 1024); // ~10KB document
        
        if (database_insert_document(db, TEST_COLLECTION, doc_id, doc) != 0) {
            log_test("Failed to insert large document");
        }
    }
    insert_time = stop_timer();
    log_performance("Large Document Insertion", insert_time, num_large_docs);
    
    // Retrieve
    start_timer();
    for (int i = 0; i < num_large_docs; i++) {
        char doc_id[32];
        snprintf(doc_id, sizeof(doc_id), "large_doc_%d", i);
        
        json_value_t* doc = database_get_document(db, TEST_COLLECTION, doc_id);
        if (!doc) {
            log_test("Failed to retrieve large document");
        }
    }
    retrieve_time = stop_timer();
    log_performance("Large Document Retrieval", retrieve_time, num_large_docs);
    
    // Update
    start_timer();
    for (int i = 0; i < num_large_docs; i++) {
        char doc_id[32];
        snprintf(doc_id, sizeof(doc_id), "large_doc_%d", i);
        
        json_value_t* doc = create_test_document(10 * 1024); // ~10KB document
        json_object_set(doc, "updated", json_create_boolean(1));
        
        if (database_update_document(db, TEST_COLLECTION, doc_id, doc) != 0) {
            log_test("Failed to update large document");
        }
    }
    update_time = stop_timer();
    log_performance("Large Document Update", update_time, num_large_docs);
    
    // Delete
    start_timer();
    for (int i = 0; i < num_large_docs; i++) {
        char doc_id[32];
        snprintf(doc_id, sizeof(doc_id), "large_doc_%d", i);
        
        if (database_delete_document(db, TEST_COLLECTION, doc_id) != 0) {
            log_test("Failed to delete large document");
        }
    }
    delete_time = stop_timer();
    log_performance("Large Document Deletion", delete_time, num_large_docs);
    
    database_free(db);
}

int main() {
    // Start overall timer
    start_time = clock();
    
    test_init();
    
    // Set alarm for test timeout
    alarm(TEST_TIMEOUT_SEC);
    
    test_insert_performance();
    test_retrieval_performance();
    test_update_performance();
    test_delete_performance();
    test_large_document_performance();
    
    test_cleanup();
    return EXIT_SUCCESS;
}