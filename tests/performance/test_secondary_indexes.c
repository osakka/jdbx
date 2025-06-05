#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "database/database.h"
#include "utils/logger.h"
#include "utils/json.h"

#define TEST_COLLECTION "users"
#define NUM_DOCS 10000

double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

int main() {
    // Initialize logger
    logger_init("/tmp/test_indexes.log", LOG_LEVEL_DEBUG);
    LOG_INFO("Testing secondary indexes...");
    
    // Initialize database
    database_t* db = db_init("/tmp/test_indexes_db");
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
        
        if ((i + 1) % 1000 == 0) {
            LOG_INFO("Inserted %d documents", i + 1);
        }
    }
    
    double insert_time = get_time_ms() - start;
    LOG_INFO("Insert time: %.2f ms (%.2f docs/sec)", insert_time, 
             (NUM_DOCS * 1000.0) / insert_time);
    
    // Test 1: Query without index (baseline)
    LOG_INFO("\nTest 1: Query without index");
    start = get_time_ms();
    
    json_value_t* query = json_create_object();
    json_object_set(query, "age", json_create_integer(25));
    
    json_value_t* results = db_query_documents(db, TEST_COLLECTION, query);
    assert(results != NULL);
    
    json_value_t* docs = json_object_get(results, "documents");
    int count = docs ? json_array_size(docs) : 0;
    
    double query_time_no_index = get_time_ms() - start;
    LOG_INFO("Found %d documents in %.2f ms (no index)", count, query_time_no_index);
    
    json_free(results);
    json_free(query);
    
    // Test 2: Create index on 'age' field
    LOG_INFO("\nTest 2: Creating index on 'age' field");
    start = get_time_ms();
    
    index_t* age_index = db_create_index(db, TEST_COLLECTION, "age_idx", "age", INDEX_TYPE_NON_UNIQUE);
    assert(age_index != NULL);
    
    double index_create_time = get_time_ms() - start;
    LOG_INFO("Index created in %.2f ms", index_create_time);
    
    // Test 3: Query with index
    LOG_INFO("\nTest 3: Query with index");
    start = get_time_ms();
    
    query = json_create_object();
    json_object_set(query, "age", json_create_integer(25));
    
    results = db_query_documents(db, TEST_COLLECTION, query);
    assert(results != NULL);
    
    docs = json_object_get(results, "documents");
    count = docs ? json_array_size(docs) : 0;
    
    double query_time_with_index = get_time_ms() - start;
    LOG_INFO("Found %d documents in %.2f ms (with index)", count, query_time_with_index);
    LOG_INFO("Speedup: %.2fx", query_time_no_index / query_time_with_index);
    
    json_free(results);
    json_free(query);
    
    // Test 4: List indexes
    LOG_INFO("\nTest 4: List indexes");
    json_value_t* indexes = db_list_indexes(db, TEST_COLLECTION);
    if (indexes) {
        char* json_str = json_stringify(indexes);
        LOG_INFO("Indexes: %s", json_str);
        free(json_str);
        json_free(indexes);
    }
    
    // Test 5: Range query (would benefit from B+tree)
    LOG_INFO("\nTest 5: Range query test");
    start = get_time_ms();
    
    // For now, we'll test multiple point queries
    int range_count = 0;
    for (int age = 30; age <= 35; age++) {
        query = json_create_object();
        json_object_set(query, "age", json_create_integer(age));
        
        results = db_query_documents(db, TEST_COLLECTION, query);
        if (results) {
            docs = json_object_get(results, "documents");
            if (docs) {
                range_count += json_array_size(docs);
            }
            json_free(results);
        }
        json_free(query);
    }
    
    double range_query_time = get_time_ms() - start;
    LOG_INFO("Range query found %d documents in %.2f ms", range_count, range_query_time);
    
    // Test 6: Drop index
    LOG_INFO("\nTest 6: Drop index");
    rc = db_drop_index(db, TEST_COLLECTION, "age");
    assert(rc == 0);
    LOG_INFO("Index dropped successfully");
    
    // Cleanup
    db_close(db);
    
    LOG_INFO("\n=== Secondary Index Tests Complete ===");
    LOG_INFO("Summary:");
    LOG_INFO("- Insert %d docs: %.2f ms", NUM_DOCS, insert_time);
    LOG_INFO("- Query without index: %.2f ms", query_time_no_index);
    LOG_INFO("- Create index: %.2f ms", index_create_time);
    LOG_INFO("- Query with index: %.2f ms", query_time_with_index);
    LOG_INFO("- Performance improvement: %.2fx", query_time_no_index / query_time_with_index);
    
    logger_close();
    return 0;
}