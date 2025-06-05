#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "database/database.h"
#include "database/batch_operations.h"
#include "utils/json.h"
#include "utils/logger.h"
#include "utils/production_config.h"

static double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

int main() {
    logger_init("/tmp/test_batch_insert.log", LOG_LEVEL_INFO);
    
    printf("Batch Insert API Test\n");
    printf("====================\n\n");
    
    /* Use medium production config for better performance */
    production_config_init("medium");
    
    database_t* db = db_init("/tmp/batch_test_db");
    assert(db != NULL);
    
    db_create_collection(db, "batch_test");
    
    /* Test 1: Small batch */
    printf("Test 1: Small batch (100 documents)\n");
    json_value_t* small_batch = json_create_array();
    for (int i = 0; i < 100; i++) {
        json_value_t* doc = json_create_object();
        json_object_set(doc, "batch_id", json_create_integer(1));
        json_object_set(doc, "seq", json_create_integer(i));
        json_object_set(doc, "data", json_create_string("Test document"));
        json_array_append(small_batch, doc);
    }
    
    double start = get_time_ms();
    batch_insert_result_t* result1 = db_batch_insert_documents(db, "batch_test", small_batch, NULL);
    double end = get_time_ms();
    
    if (result1) {
        printf("  Inserted: %zu/%zu documents in %.2f ms\n", 
               result1->successful_inserts, result1->total_documents, end - start);
        printf("  Throughput: %.0f docs/sec\n", 
               result1->successful_inserts * 1000.0 / (end - start));
        batch_insert_result_free(result1);
    }
    json_free(small_batch);
    
    /* Test 2: Large batch */
    printf("\nTest 2: Large batch (10,000 documents)\n");
    json_value_t* large_batch = json_create_array();
    for (int i = 0; i < 10000; i++) {
        json_value_t* doc = json_create_object();
        json_object_set(doc, "batch_id", json_create_integer(2));
        json_object_set(doc, "seq", json_create_integer(i));
        json_object_set(doc, "value", json_create_integer(i * 10));
        json_object_set(doc, "timestamp", json_create_integer(time(NULL)));
        json_array_append(large_batch, doc);
    }
    
    start = get_time_ms();
    batch_insert_result_t* result2 = db_batch_insert_documents(db, "batch_test", large_batch, NULL);
    end = get_time_ms();
    
    if (result2) {
        printf("  Inserted: %zu/%zu documents in %.2f ms\n", 
               result2->successful_inserts, result2->total_documents, end - start);
        printf("  Throughput: %.0f docs/sec\n", 
               result2->successful_inserts * 1000.0 / (end - start));
        
        if (end - start < 1000) {
            printf("  ✓ SUB-SECOND for 10K documents!\n");
        }
        
        batch_insert_result_free(result2);
    }
    json_free(large_batch);
    
    /* Test 3: Batch with options */
    printf("\nTest 3: Batch with ignore_duplicates option\n");
    json_value_t* dup_batch = json_create_array();
    
    /* Add some new documents */
    for (int i = 0; i < 50; i++) {
        json_value_t* doc = json_create_object();
        json_object_set(doc, "_id", json_create_string("unique_doc"));
        json_object_set(doc, "value", json_create_integer(i));
        json_array_append(dup_batch, doc);
    }
    
    batch_insert_options_t options = {
        .ignore_duplicates = true,
        .batch_size = 1000,
        .defer_indexing = true
    };
    
    start = get_time_ms();
    batch_insert_result_t* result3 = db_batch_insert_documents(db, "batch_test", dup_batch, &options);
    end = get_time_ms();
    
    if (result3) {
        printf("  Attempted: %zu documents\n", result3->total_documents);
        printf("  Successful: %zu documents\n", result3->successful_inserts);
        printf("  Failed (duplicates): %zu documents\n", result3->failed_inserts);
        printf("  Time: %.2f ms\n", end - start);
        batch_insert_result_free(result3);
    }
    json_free(dup_batch);
    
    /* Final stats */
    json_value_t* collections = db_list_collections_with_info(db);
    if (collections && json_array_size(collections) > 0) {
        json_value_t* coll_info = json_array_get(collections, 0);
        json_value_t* count = json_object_get(coll_info, "document_count");
        if (count) {
            printf("\nFinal document count: %.0f\n", count->value.number);
        }
        json_free(collections);
    }
    
    db_close(db);
    logger_close();
    return 0;
}