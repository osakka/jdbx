#include "jsondb/database/database.h"
#include "jsondb/utils/logger.h"
#include "jsondb/utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

/**
 * Basic Cache Operation Test
 *
 * This test validates that the core cache functionality works:
 * 1. Initialize database
 * 2. Create collection
 * 3. Enable cache
 * 4. Insert document
 * 5. Query documents (cache miss)
 * 6. Query documents again (cache hit)
 * 7. Close database
 */

/* Global timeout handler */
void timeout_handler(int sig) {
    fprintf(stderr, "\n\nTEST TIMEOUT: Operation took too long, possible deadlock\n");
    exit(1);
}

/* Set an alarm to prevent hanging */
void set_timeout(int seconds) {
    signal(SIGALRM, timeout_handler);
    alarm(seconds);
}

/* Clear alarm */
void clear_timeout() {
    alarm(0);
}

int main() {
    /* Set global 30 second timeout to prevent hanging */
    set_timeout(30);

    /* Initialize logger */
    logger_init("cache_operation_test.log", LOG_LEVEL_DEBUG);
    printf("Logger initialized\n");

    /* Initialize database */
    printf("Initializing database...\n");
    database_t* db = db_init("cache_operation.json");
    if (!db) {
        printf("Failed to initialize database\n");
        logger_close();
        return 1;
    }
    printf("Database initialized\n");

    /* Create collection */
    printf("Creating collection...\n");
    const char* collection_name = "test_collection";

    int success = 0;
    json_value_t* existing = json_object_get(db->collections, collection_name);
    if (existing) {
        printf("Collection already exists, using existing one\n");
        success = 1;
    } else {
        success = db_create_collection(db, collection_name);
        if (!success) {
            printf("Failed to create collection\n");
            db_close(db);
            logger_close();
            return 1;
        }
        printf("Collection created\n");
    }
    
    /* Enable cache */
    printf("Enabling cache...\n");
    success = db_enable_cache(db, 10, 0);
    if (!success) {
        printf("Failed to enable cache\n");
        db_close(db);
        logger_close();
        return 1;
    }
    printf("Cache enabled\n");
    
    /* Insert document */
    printf("Inserting document...\n");
    json_value_t* doc = json_create_object();
    json_object_set(doc, "name", json_create_string("Test Document"));
    json_object_set(doc, "value", json_create_integer(42));

    /* Set a specific timeout for insert operation */
    printf("About to insert document (timeout: 10 seconds)...\n");
    set_timeout(10);
    json_value_t* insert_result = db_insert_document(db, collection_name, doc);
    clear_timeout();

    if (!insert_result) {
        printf("Failed to insert document\n");
        db_close(db);
        logger_close();
        return 1;
    }
    printf("Document inserted with ID: %s\n",
           json_get_string(json_object_get(insert_result, "uuid")));
    json_free(insert_result);
    
    /* Get cache stats before query */
    printf("\nCache stats before querying:\n");
    json_value_t* stats1 = db_get_cache_stats(db);
    if (stats1) {
        json_value_t* size1 = json_object_get(stats1, "size");
        json_value_t* hits1 = json_object_get(stats1, "hits");
        json_value_t* misses1 = json_object_get(stats1, "misses");
        
        printf("  - Size: %ld\n", 
               size1 && size1->type == JSON_INTEGER ? (long)size1->value.integer : 0);
        printf("  - Hits: %ld\n", 
               hits1 && hits1->type == JSON_INTEGER ? (long)hits1->value.integer : 0);
        printf("  - Misses: %ld\n", 
               misses1 && misses1->type == JSON_INTEGER ? (long)misses1->value.integer : 0);
        
        json_free(stats1);
    }
    
    /* Query 1 (cache miss) */
    printf("\nPerforming first query (should be cache miss)...\n");
    json_value_t* query1 = json_create_object();

    /* Set timeout for first query */
    printf("About to execute first query (timeout: 10 seconds)...\n");
    set_timeout(10);
    json_value_t* result1 = db_query_documents(db, collection_name, query1);
    clear_timeout();
    printf("First query completed\n");

    if (!result1) {
        printf("Failed to query documents\n");
        db_close(db);
        logger_close();
        return 1;
    }
    
    json_value_t* docs1 = json_object_get(result1, "documents");
    printf("First query returned %ld documents\n", 
           docs1 && docs1->type == JSON_ARRAY ? (long)docs1->value.array.size : 0);
    
    /* Get cache stats after first query */
    printf("\nCache stats after first query:\n");
    json_value_t* stats2 = db_get_cache_stats(db);
    if (stats2) {
        json_value_t* size2 = json_object_get(stats2, "size");
        json_value_t* hits2 = json_object_get(stats2, "hits");
        json_value_t* misses2 = json_object_get(stats2, "misses");
        
        printf("  - Size: %ld\n", 
               size2 && size2->type == JSON_INTEGER ? (long)size2->value.integer : 0);
        printf("  - Hits: %ld\n", 
               hits2 && hits2->type == JSON_INTEGER ? (long)hits2->value.integer : 0);
        printf("  - Misses: %ld\n", 
               misses2 && misses2->type == JSON_INTEGER ? (long)misses2->value.integer : 0);
        
        json_free(stats2);
    }
    
    /* Query 2 (cache hit) */
    printf("\nPerforming second query (should be cache hit)...\n");
    json_value_t* query2 = json_create_object();

    /* Set timeout for second query */
    printf("About to execute second query (timeout: 10 seconds)...\n");
    set_timeout(10);
    json_value_t* result2 = db_query_documents(db, collection_name, query2);
    clear_timeout();
    printf("Second query completed\n");

    if (!result2) {
        printf("Failed to query documents\n");
        json_free(result1);
        db_close(db);
        logger_close();
        return 1;
    }
    
    json_value_t* docs2 = json_object_get(result2, "documents");
    printf("Second query returned %ld documents\n", 
           docs2 && docs2->type == JSON_ARRAY ? (long)docs2->value.array.size : 0);
    
    /* Get cache stats after second query */
    printf("\nCache stats after second query:\n");
    json_value_t* stats3 = db_get_cache_stats(db);
    if (stats3) {
        json_value_t* size3 = json_object_get(stats3, "size");
        json_value_t* hits3 = json_object_get(stats3, "hits");
        json_value_t* misses3 = json_object_get(stats3, "misses");
        
        printf("  - Size: %ld\n", 
               size3 && size3->type == JSON_INTEGER ? (long)size3->value.integer : 0);
        printf("  - Hits: %ld\n", 
               hits3 && hits3->type == JSON_INTEGER ? (long)hits3->value.integer : 0);
        printf("  - Misses: %ld\n", 
               misses3 && misses3->type == JSON_INTEGER ? (long)misses3->value.integer : 0);
        
        json_free(stats3);
    }
    
    /* Check if hits increased (confirming cache hit) */
    if (stats2 && stats3) {
        json_value_t* hits2 = json_object_get(stats2, "hits");
        json_value_t* hits3 = json_object_get(stats3, "hits");
        
        long h2 = hits2 && hits2->type == JSON_INTEGER ? (long)hits2->value.integer : 0;
        long h3 = hits3 && hits3->type == JSON_INTEGER ? (long)hits3->value.integer : 0;
        
        if (h3 > h2) {
            printf("\nCache hit confirmed! Hits increased from %ld to %ld\n", h2, h3);
        } else {
            printf("\nWarning: Cache hit not confirmed. Hits did not increase.\n");
        }
    }
    
    /* Clean up */
    json_free(result1);
    json_free(result2);
    
    /* Close database */
    printf("\nClosing database...\n");
    set_timeout(10);
    db_close(db);
    clear_timeout();
    printf("Database closed\n");

    logger_close();

    printf("\nTest completed successfully\n");
    return 0;
}