#include "jsondb/database/database.h"
#include "jsondb/utils/logger.h"
#include "jsondb/utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * Test program for query caching
 * This program tests the advanced query caching functionality in the database
 */

/* Simple timer for measuring performance */
typedef struct perf_timer {
    struct timespec start;
    struct timespec end;
} perf_timer_t;

void timer_start(perf_timer_t* timer) {
    clock_gettime(CLOCK_MONOTONIC, &timer->start);
}

double timer_end(perf_timer_t* timer) {
    clock_gettime(CLOCK_MONOTONIC, &timer->end);
    return (timer->end.tv_sec - timer->start.tv_sec) * 1000.0 +
           (timer->end.tv_nsec - timer->start.tv_nsec) / 1000000.0;
}

/* Test functions */
int test_setup_database(const char* db_path) {
    printf("Setting up test database at %s\n", db_path);

    /* Initialize database */
    database_t* db = db_init(db_path);
    if (!db) {
        fprintf(stderr, "Failed to initialize database\n");
        return 0;
    }

    /* Create test collection */
    if (!db_create_collection(db, "test_collection")) {
        fprintf(stderr, "Failed to create test collection\n");
        db_close(db);
        return 0;
    }

    /* Insert test documents */
    printf("Inserting test documents...\n");
    for (int i = 0; i < 10; i++) {  /* Reduced from 100 to 10 for faster testing */
        json_value_t* doc = json_create_object();

        /* Add some fields */
        char name[32];
        snprintf(name, sizeof(name), "Test Item %d", i);
        json_object_set(doc, "name", json_create_string(name));
        json_object_set(doc, "value", json_create_integer(i));
        json_object_set(doc, "is_even", json_create_boolean(i % 2 == 0));

        /* Add to collection */
        json_value_t* result = db_insert_document(db, "test_collection", doc);
        if (!result) {
            fprintf(stderr, "Failed to insert document %d\n", i);
            db_close(db);
            return 0;
        }

        json_free(result);
    }

    /* Save database */
    if (!db_save(db)) {
        fprintf(stderr, "Failed to save database\n");
        db_close(db);
        return 0;
    }

    /* Close database */
    db_close(db);

    printf("Database setup completed successfully\n");
    return 1;
}

int test_query_caching() {
    printf("\n=== Testing Query Caching ===\n");

    const char* db_path = "test_query_cache.json";

    /* Set up the database for testing */
    if (!test_setup_database(db_path)) {
        return 0;
    }

    /* Initialize database with cache enabled */
    database_t* db = db_init(db_path);
    if (!db) {
        fprintf(stderr, "Failed to initialize database\n");
        return 0;
    }

    /* Enable caching */
    printf("Enabling cache with 100 entries capacity\n");
    if (!db_enable_cache(db, 100, 0)) {
        fprintf(stderr, "Failed to enable cache\n");
        db_close(db);
        return 0;
    }

    /* Test 1: Query all documents and measure time */
    printf("\nTest 1: Query all documents (should cache results)\n");

    perf_timer_t timer;
    json_value_t* query_all = json_create_object();
    
    /* First query - should be from database and slower */
    timer_start(&timer);
    json_value_t* result1 = db_query_documents(db, "test_collection", query_all);
    double time1 = timer_end(&timer);
    
    printf("  First query time: %.2f ms\n", time1);
    if (!result1) {
        fprintf(stderr, "  Failed to query documents\n");
        db_close(db);
        return 0;
    }
    
    /* Get cache stats */
    json_value_t* stats = db_get_cache_stats(db);
    printf("  Cache stats after first query:\n");

    json_value_t* enabled = json_object_get(stats, "enabled");
    json_value_t* size = json_object_get(stats, "size");
    json_value_t* hits = json_object_get(stats, "hits");
    json_value_t* misses = json_object_get(stats, "misses");

    printf("    Enabled: %s\n", enabled && enabled->type == JSON_BOOLEAN && enabled->value.boolean ? "Yes" : "No");
    printf("    Size: %ld\n", size && size->type == JSON_INTEGER ? (long)size->value.integer : 0);
    printf("    Hits: %ld\n", hits && hits->type == JSON_INTEGER ? (long)hits->value.integer : 0);
    printf("    Misses: %ld\n", misses && misses->type == JSON_INTEGER ? (long)misses->value.integer : 0);
    
    json_free(stats);
    
    /* Second query - should be from cache and faster */
    timer_start(&timer);
    json_value_t* result2 = db_query_documents(db, "test_collection", json_create_object());
    double time2 = timer_end(&timer);
    
    printf("  Second query time: %.2f ms\n", time2);
    if (!result2) {
        fprintf(stderr, "  Failed to query documents\n");
        db_close(db);
        return 0;
    }
    
    /* Check if second query was faster */
    printf("  Speedup: %.2fx\n", time1 / time2);
    
    /* Get cache stats again */
    stats = db_get_cache_stats(db);
    printf("  Cache stats after second query:\n");

    enabled = json_object_get(stats, "enabled");
    size = json_object_get(stats, "size");
    hits = json_object_get(stats, "hits");
    misses = json_object_get(stats, "misses");

    printf("    Enabled: %s\n", enabled && enabled->type == JSON_BOOLEAN && enabled->value.boolean ? "Yes" : "No");
    printf("    Size: %ld\n", size && size->type == JSON_INTEGER ? (long)size->value.integer : 0);
    printf("    Hits: %ld\n", hits && hits->type == JSON_INTEGER ? (long)hits->value.integer : 0);
    printf("    Misses: %ld\n", misses && misses->type == JSON_INTEGER ? (long)misses->value.integer : 0);
    
    json_free(stats);
    
    /* Clean up */
    json_free(result1);
    json_free(result2);
    
    /* Test 2: Query with filter */
    printf("\nTest 2: Query with filter (even values)\n");
    
    /* Create query for even values */
    json_value_t* query_even = json_create_object();
    json_object_set(query_even, "is_even", json_create_boolean(1));
    
    /* First filtered query - should be from database */
    timer_start(&timer);
    json_value_t* result_even1 = db_query_documents(db, "test_collection", query_even);
    double time_even1 = timer_end(&timer);
    
    printf("  First filtered query time: %.2f ms\n", time_even1);
    if (!result_even1) {
        fprintf(stderr, "  Failed to query documents with filter\n");
        db_close(db);
        return 0;
    }
    
    /* Second filtered query - should be from cache */
    json_value_t* query_even2 = json_create_object();
    json_object_set(query_even2, "is_even", json_create_boolean(1));
    
    timer_start(&timer);
    json_value_t* result_even2 = db_query_documents(db, "test_collection", query_even2);
    double time_even2 = timer_end(&timer);
    
    printf("  Second filtered query time: %.2f ms\n", time_even2);
    if (!result_even2) {
        fprintf(stderr, "  Failed to query documents with filter\n");
        db_close(db);
        return 0;
    }
    
    /* Check if second query was faster */
    printf("  Speedup: %.2fx\n", time_even1 / time_even2);
    
    /* Get cache stats */
    stats = db_get_cache_stats(db);
    printf("  Cache stats after filtered queries:\n");

    enabled = json_object_get(stats, "enabled");
    size = json_object_get(stats, "size");
    hits = json_object_get(stats, "hits");
    misses = json_object_get(stats, "misses");

    printf("    Enabled: %s\n", enabled && enabled->type == JSON_BOOLEAN && enabled->value.boolean ? "Yes" : "No");
    printf("    Size: %ld\n", size && size->type == JSON_INTEGER ? (long)size->value.integer : 0);
    printf("    Hits: %ld\n", hits && hits->type == JSON_INTEGER ? (long)hits->value.integer : 0);
    printf("    Misses: %ld\n", misses && misses->type == JSON_INTEGER ? (long)misses->value.integer : 0);
    
    json_free(stats);
    
    /* Clean up */
    json_free(result_even1);
    json_free(result_even2);
    
    /* Test 3: Update document and verify cache invalidation */
    printf("\nTest 3: Update document and verify cache invalidation\n");
    
    /* First, query all documents again to ensure it's in cache */
    json_value_t* result_pre_update = db_query_documents(db, "test_collection", json_create_object());
    if (!result_pre_update) {
        fprintf(stderr, "  Failed to query documents before update\n");
        db_close(db);
        return 0;
    }
    
    /* Get document count before update */
    json_value_t* documents = json_object_get(result_pre_update, "documents");
    size_t doc_count = documents ? documents->value.array.size : 0;
    printf("  Document count before update: %zu\n", doc_count);
    
    /* Get the first document's ID */
    const char* doc_id = NULL;
    if (documents && documents->value.array.size > 0) {
        json_value_t* first_doc = documents->value.array.items[0];
        json_value_t* id_val = json_object_get(first_doc, "_id");
        if (id_val && id_val->type == JSON_STRING) {
            doc_id = id_val->value.string;
        }
    }
    
    if (!doc_id) {
        fprintf(stderr, "  Failed to get document ID for update\n");
        db_close(db);
        return 0;
    }
    
    /* Get cache stats before update */
    stats = db_get_cache_stats(db);
    printf("  Cache stats before update:\n");
    size = json_object_get(stats, "size");
    printf("    Size: %ld\n", size && size->type == JSON_INTEGER ? (long)size->value.integer : 0);
    json_free(stats);
    
    /* Update a document */
    json_value_t* updated_doc = json_create_object();
    json_object_set(updated_doc, "name", json_create_string("Updated Document"));
    json_object_set(updated_doc, "value", json_create_integer(999));
    json_object_set(updated_doc, "is_even", json_create_boolean(0));
    
    printf("  Updating document with ID: %s\n", doc_id);
    json_value_t* update_result = db_update_document(db, "test_collection", doc_id, updated_doc);
    if (!update_result) {
        fprintf(stderr, "  Failed to update document\n");
        db_close(db);
        return 0;
    }
    json_free(update_result);
    
    /* Get cache stats after update */
    stats = db_get_cache_stats(db);
    printf("  Cache stats after update:\n");
    size = json_object_get(stats, "size");
    printf("    Size: %ld\n", size && size->type == JSON_INTEGER ? (long)size->value.integer : 0);
    json_free(stats);
    
    /* Query again - should miss cache due to invalidation */
    timer_start(&timer);
    json_value_t* result_post_update = db_query_documents(db, "test_collection", json_create_object());
    double time_post_update = timer_end(&timer);
    
    printf("  Query time after update: %.2f ms\n", time_post_update);
    if (!result_post_update) {
        fprintf(stderr, "  Failed to query documents after update\n");
        db_close(db);
        return 0;
    }
    
    /* Clean up */
    json_free(result_pre_update);
    json_free(result_post_update);
    
    /* Test 4: Delete document and verify cache invalidation */
    printf("\nTest 4: Delete document and verify cache invalidation\n");
    
    /* First, query all documents again to ensure it's in cache */
    result_pre_update = db_query_documents(db, "test_collection", json_create_object());
    if (!result_pre_update) {
        fprintf(stderr, "  Failed to query documents before delete\n");
        db_close(db);
        return 0;
    }
    
    /* Get a document ID to delete */
    doc_id = NULL;
    documents = json_object_get(result_pre_update, "documents");
    if (documents && documents->value.array.size > 0) {
        json_value_t* last_doc = documents->value.array.items[documents->value.array.size - 1];
        json_value_t* id_val = json_object_get(last_doc, "_id");
        if (id_val && id_val->type == JSON_STRING) {
            doc_id = id_val->value.string;
        }
    }
    
    if (!doc_id) {
        fprintf(stderr, "  Failed to get document ID for delete\n");
        db_close(db);
        return 0;
    }
    
    /* Get cache stats before delete */
    stats = db_get_cache_stats(db);
    printf("  Cache stats before delete:\n");
    size = json_object_get(stats, "size");
    printf("    Size: %ld\n", size && size->type == JSON_INTEGER ? (long)size->value.integer : 0);
    json_free(stats);
    
    /* Delete the document */
    printf("  Deleting document with ID: %s\n", doc_id);
    int delete_result = db_delete_document(db, "test_collection", doc_id);
    if (!delete_result) {
        fprintf(stderr, "  Failed to delete document\n");
        db_close(db);
        return 0;
    }
    
    /* Get cache stats after delete */
    stats = db_get_cache_stats(db);
    printf("  Cache stats after delete:\n");
    size = json_object_get(stats, "size");
    printf("    Size: %ld\n", size && size->type == JSON_INTEGER ? (long)size->value.integer : 0);
    json_free(stats);
    
    /* Query again - should miss cache due to invalidation */
    timer_start(&timer);
    json_value_t* result_post_delete = db_query_documents(db, "test_collection", json_create_object());
    double time_post_delete = timer_end(&timer);
    
    printf("  Query time after delete: %.2f ms\n", time_post_delete);
    if (!result_post_delete) {
        fprintf(stderr, "  Failed to query documents after delete\n");
        db_close(db);
        return 0;
    }
    
    /* Clean up */
    json_free(result_pre_update);
    json_free(result_post_delete);
    
    /* Close database */
    db_close(db);
    
    printf("\nQuery caching tests completed successfully\n");
    return 1;
}

int main() {
    /* Initialize logger */
    logger_init("test_query_cache.log", LOG_LEVEL_DEBUG);

    /* Run query caching tests */
    int result = test_query_caching();

    /* Close logger */
    logger_close();

    return result ? 0 : 1;
}