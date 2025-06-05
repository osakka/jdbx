#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "database/database.h"
#include "utils/json.h"
#include "utils/logger.h"

static double get_time_us() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000.0 + ts.tv_nsec / 1000.0;
}

int main() {
    logger_init("/tmp/test_btree_query.log", LOG_LEVEL_INFO);
    
    printf("B+tree Index Query Test\n");
    printf("=======================\n\n");
    
    database_t* db = db_init("/tmp/btree_test_db");
    assert(db != NULL);
    
    db_create_collection(db, "indexed_test");
    
    /* Create index BEFORE inserting documents */
    printf("Creating index on 'value' field...\n");
    index_t* idx = db_create_index(db, "indexed_test", "value_idx", "value", INDEX_TYPE_NON_UNIQUE);
    if (idx) {
        printf("Index created successfully\n");
        free(idx->name);
        free(idx->field_path);
        free(idx);
    }
    
    /* Insert test documents */
    printf("\nInserting 1000 test documents...\n");
    for (int i = 0; i < 1000; i++) {
        json_value_t* doc = json_create_object();
        json_object_set(doc, "id", json_create_integer(i));
        json_object_set(doc, "value", json_create_integer(i * 10));
        json_object_set(doc, "category", json_create_string(i % 2 == 0 ? "even" : "odd"));
        
        json_value_t* result = db_insert_document(db, "indexed_test", doc);
        if (result) json_free(result);
        json_free(doc);
        
        if ((i + 1) % 100 == 0) {
            printf("  Inserted %d documents...\r", i + 1);
            fflush(stdout);
        }
    }
    printf("\n");
    
    /* Test exact match query */
    printf("\nTesting exact match query (value = 500):\n");
    json_value_t* eq_query = json_create_object();
    json_value_t* eq_cond = json_create_object();
    json_object_set(eq_cond, "$eq", json_create_integer(500));
    json_object_set(eq_query, "value", eq_cond);
    
    double eq_start = get_time_us();
    json_value_t* eq_results = db_query_documents(db, "indexed_test", eq_query);
    double eq_end = get_time_us();
    
    if (eq_results) {
        json_value_t* docs = json_object_get(eq_results, "documents");
        printf("  Found %zu documents in %.2f μs", json_array_size(docs), eq_end - eq_start);
        if (eq_end - eq_start < 1000) {
            printf(" ✓ SUB-MILLISECOND!\n");
        } else {
            printf(" (%.2f ms)\n", (eq_end - eq_start) / 1000);
        }
        json_free(eq_results);
    }
    json_free(eq_query);
    
    /* Test range query */
    printf("\nTesting range query (value >= 400 AND value < 600):\n");
    json_value_t* range_query = json_create_object();
    json_value_t* range_cond = json_create_object();
    json_object_set(range_cond, "$gte", json_create_integer(400));
    json_object_set(range_cond, "$lt", json_create_integer(600));
    json_object_set(range_query, "value", range_cond);
    
    double range_start = get_time_us();
    json_value_t* range_results = db_query_documents(db, "indexed_test", range_query);
    double range_end = get_time_us();
    
    if (range_results) {
        json_value_t* docs = json_object_get(range_results, "documents");
        printf("  Found %zu documents in %.2f μs", json_array_size(docs), range_end - range_start);
        if (range_end - range_start < 1000) {
            printf(" ✓ SUB-MILLISECOND!\n");
        } else {
            printf(" (%.2f ms)\n", (range_end - range_start) / 1000);
        }
        json_free(range_results);
    }
    json_free(range_query);
    
    /* Test query without index (no field match) */
    printf("\nTesting query without index (category = 'even'):\n");
    json_value_t* no_idx_query = json_create_object();
    json_object_set(no_idx_query, "category", json_create_string("even"));
    
    double no_idx_start = get_time_us();
    json_value_t* no_idx_results = db_query_documents(db, "indexed_test", no_idx_query);
    double no_idx_end = get_time_us();
    
    if (no_idx_results) {
        json_value_t* docs = json_object_get(no_idx_results, "documents");
        printf("  Found %zu documents in %.2f μs", json_array_size(docs), no_idx_end - no_idx_start);
        if (no_idx_end - no_idx_start < 1000) {
            printf(" ✓ SUB-MILLISECOND!\n");
        } else {
            printf(" (%.2f ms)\n", (no_idx_end - no_idx_start) / 1000);
        }
        json_free(no_idx_results);
    }
    json_free(no_idx_query);
    
    db_close(db);
    logger_close();
    return 0;
}