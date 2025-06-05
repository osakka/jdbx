#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <stdint.h>
#include "database/database.h"
#include "utils/json.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"

/* High-resolution timer for sub-millisecond measurements */
static double get_time_us() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000.0 + ts.tv_nsec / 1000.0;
}

int main() {
    logger_init("/tmp/test_perf_simple.log", LOG_LEVEL_INFO);
    
    /* Initialize database */
    database_t* db = db_init("/tmp/test_perf_db");
    assert(db != NULL);
    
    /* Create collection */
    int rc = db_create_collection(db, "perf_test");
    assert(rc == 0);
    
    printf("JSONdb Performance Test\n");
    printf("======================\n\n");
    
    /* Test 1: Single Document Insert */
    printf("1. Single Document Insert Test\n");
    double total_insert_us = 0;
    const int num_inserts = 1000;
    
    for (int i = 0; i < num_inserts; i++) {
        json_value_t* doc = json_create_object();
        json_object_set(doc, "id", json_create_integer(i));
        json_object_set(doc, "name", json_create_string("Test Document"));
        json_object_set(doc, "value", json_create_integer(i * 100));
        
        double start = get_time_us();
        json_value_t* result = db_insert_document(db, "perf_test", doc);
        char* doc_id = NULL;
        if (result) {
            json_value_t* id_val = json_object_get(result, "_id");
            if (id_val && id_val->type == JSON_STRING) {
                doc_id = buffer_pool_strdup(id_val->value.string);
            }
            json_free(result);
        }
        double end = get_time_us();
        
        if (doc_id) {
            total_insert_us += (end - start);
            buffer_pool_free_safe(doc_id);
        }
        json_free(doc);
    }
    
    double avg_insert_us = total_insert_us / num_inserts;
    printf("   Average insert time: %.2f μs", avg_insert_us);
    if (avg_insert_us < 1000) {
        printf(" ✓ SUB-MILLISECOND!\n");
    } else {
        printf(" (%.2f ms)\n", avg_insert_us / 1000);
    }
    
    /* Test 2: Point Query by ID */
    printf("\n2. Point Query Test\n");
    
    /* First, get some document IDs */
    json_value_t* empty_query = json_create_object();
    json_value_t* results = db_query_documents(db, "perf_test", empty_query);
    json_free(empty_query);
    
    if (results && json_array_size(results) > 0) {
        double total_query_us = 0;
        int num_queries = 100;
        if (num_queries > json_array_size(results)) {
            num_queries = json_array_size(results);
        }
        
        for (int i = 0; i < num_queries; i++) {
            json_value_t* doc = json_array_get(results, i);
            json_value_t* id_val = json_object_get(doc, "_id");
            if (id_val && id_val->type == JSON_STRING) {
                const char* doc_id = id_val->value.string;
                
                double start = get_time_us();
                json_value_t* found = db_get_document(db, "perf_test", doc_id);
                double end = get_time_us();
                
                if (found) {
                    total_query_us += (end - start);
                    json_free(found);
                }
            }
        }
        
        double avg_query_us = total_query_us / num_queries;
        printf("   Average query time: %.2f μs", avg_query_us);
        if (avg_query_us < 1000) {
            printf(" ✓ SUB-MILLISECOND!\n");
        } else {
            printf(" (%.2f ms)\n", avg_query_us / 1000);
        }
    }
    if (results) json_free(results);
    
    /* Test 3: Simple Update */
    printf("\n3. Update Test\n");
    
    /* Get documents to update */
    json_value_t* update_query = json_create_object();
    json_value_t* update_results = db_query_documents(db, "perf_test", update_query);
    json_free(update_query);
    
    if (update_results && json_array_size(update_results) > 0) {
        double total_update_us = 0;
        int num_updates = 100;
        if (num_updates > json_array_size(update_results)) {
            num_updates = json_array_size(update_results);
        }
        
        for (int i = 0; i < num_updates; i++) {
            json_value_t* doc = json_array_get(update_results, i);
            json_value_t* id_val = json_object_get(doc, "_id");
            if (id_val && id_val->type == JSON_STRING) {
                const char* doc_id = id_val->value.string;
                
                json_value_t* update = json_create_object();
                json_object_set(update, "updated", json_create_boolean(1));
                json_object_set(update, "timestamp", json_create_integer(time(NULL)));
                
                double start = get_time_us();
                json_value_t* update_result = db_update_document(db, "perf_test", doc_id, update);
                int rc = update_result ? 0 : -1;
                if (update_result) json_free(update_result);
                double end = get_time_us();
                
                if (rc == 0) {
                    total_update_us += (end - start);
                }
                json_free(update);
            }
        }
        
        double avg_update_us = total_update_us / num_updates;
        printf("   Average update time: %.2f μs", avg_update_us);
        if (avg_update_us < 1000) {
            printf(" ✓ SUB-MILLISECOND!\n");
        } else {
            printf(" (%.2f ms)\n", avg_update_us / 1000);
        }
    }
    if (update_results) json_free(update_results);
    
    /* Test 4: Range Query */
    printf("\n4. Range Query Test\n");
    double total_range_us = 0;
    int num_ranges = 10;
    
    for (int i = 0; i < num_ranges; i++) {
        json_value_t* range_query = json_create_object();
        json_value_t* value_range = json_create_object();
        json_object_set(value_range, "$gte", json_create_integer(i * 100));
        json_object_set(value_range, "$lt", json_create_integer((i + 10) * 100));
        json_object_set(range_query, "value", value_range);
        
        double start = get_time_us();
        json_value_t* range_results = db_query_documents(db, "perf_test", range_query);
        double end = get_time_us();
        
        if (range_results) {
            total_range_us += (end - start);
            json_free(range_results);
        }
        json_free(range_query);
    }
    
    double avg_range_us = total_range_us / num_ranges;
    printf("   Average range query time: %.2f μs", avg_range_us);
    if (avg_range_us < 1000) {
        printf(" ✓ SUB-MILLISECOND!\n");
    } else {
        printf(" (%.2f ms)\n", avg_range_us / 1000);
    }
    
    /* Cleanup */
    db_close(db);
    logger_close();
    
    return 0;
}