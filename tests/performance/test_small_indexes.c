#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "database/database.h"
#include "utils/logger.h"
#include "utils/json.h"

#define TEST_COLLECTION "users"
#define NUM_DOCS 500  // Test to trigger bucket split

double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

int main() {
    // Initialize logger
    logger_init("/tmp/test_small_indexes.log", LOG_LEVEL_INFO);
    LOG_INFO("Testing with %d documents...", NUM_DOCS);
    
    // Initialize database
    database_t* db = db_init("/tmp/test_small_indexes_db");
    assert(db != NULL);
    
    // Create collection
    int rc = db_create_collection(db, TEST_COLLECTION);
    assert(rc == 0);
    
    // Insert test documents
    LOG_INFO("Inserting %d documents...", NUM_DOCS);
    double start = get_time_ms();
    
    for (int i = 0; i < NUM_DOCS; i++) {
        json_value_t* doc = json_create_object();
        
        char name[64];
        snprintf(name, sizeof(name), "User %d", i);
        json_object_set(doc, "name", json_create_string(name));
        json_object_set(doc, "age", json_create_integer(20 + (i % 60)));
        json_object_set(doc, "score", json_create_number(50.0 + (i % 100)));
        
        char email[128];
        snprintf(email, sizeof(email), "user%d@example.com", i);
        json_object_set(doc, "email", json_create_string(email));
        
        json_value_t* result = db_insert_document(db, TEST_COLLECTION, doc);
        assert(result != NULL);
        
        json_free(doc);
        json_free(result);
        
        if ((i + 1) % 10 == 0) {
            LOG_INFO("Inserted %d documents", i + 1);
        }
    }
    
    double insert_time = get_time_ms() - start;
    LOG_INFO("Insert time: %.2f ms (%.2f docs/sec)", insert_time, 
             (NUM_DOCS * 1000.0) / insert_time);
    
    // Test retrieval
    LOG_INFO("Testing retrieval...");
    start = get_time_ms();
    
    json_value_t* query = json_create_object();
    json_object_set(query, "age", json_create_integer(25));
    
    json_value_t* results = db_query_documents(db, TEST_COLLECTION, query);
    assert(results != NULL);
    
    json_value_t* docs = json_object_get(results, "documents");
    int count = docs ? json_array_size(docs) : 0;
    
    double query_time = get_time_ms() - start;
    LOG_INFO("Found %d documents with age=25 in %.2f ms", count, query_time);
    
    json_free(results);
    json_free(query);
    
    // Test creating an index
    LOG_INFO("Creating index on 'age' field...");
    start = get_time_ms();
    
    index_t* age_index = db_create_index(db, TEST_COLLECTION, "age_idx", "age", INDEX_TYPE_NON_UNIQUE);
    assert(age_index != NULL);
    
    double index_time = get_time_ms() - start;
    LOG_INFO("Index created in %.2f ms", index_time);
    
    // Cleanup
    db_close(db);
    
    LOG_INFO("Test completed successfully!");
    logger_close();
    return 0;
}