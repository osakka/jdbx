#include "jsondb/database/database.h"
#include "jsondb/utils/logger.h"
#include "jsondb/utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

/**
 * Insert Then Cache Test
 * 
 * Tests inserting a document and then enabling the cache.
 */

volatile int timeout_occurred = 0;
volatile int current_operation = 0;

void timeout_handler(int signum) {
    (void)signum; // Unused parameter
    timeout_occurred = 1;
    
    printf("TIMEOUT: Operation %d took too long\n", current_operation);
    
    // Force exit
    _exit(1);
}

#define WITH_TIMEOUT(op_num, code_block, timeout_seconds) do { \
    printf("Operation %d (timeout: %d seconds)...\n", op_num, timeout_seconds); \
    current_operation = op_num; \
    timeout_occurred = 0; \
    alarm(timeout_seconds); \
    code_block; \
    alarm(0); \
    if (!timeout_occurred) { \
        printf("Operation %d completed successfully\n", op_num); \
    } \
} while(0)

int main() {
    printf("Starting insert then cache test\n");
    
    /* Install signal handler */
    signal(SIGALRM, timeout_handler);
    
    /* Initialize logger */
    WITH_TIMEOUT(1, {
        logger_init("insert_then_cache.log", LOG_LEVEL_DEBUG);
    }, 5);
    
    /* Initialize database */
    database_t* db = NULL;
    WITH_TIMEOUT(2, {
        db = db_init("insert_then_cache.json");
    }, 5);
    
    if (!db) {
        printf("Failed to initialize database\n");
        logger_close();
        return 1;
    }
    
    /* Create collection */
    int success = 0;
    WITH_TIMEOUT(3, {
        success = db_create_collection(db, "test_collection");
    }, 5);
    
    if (!success) {
        printf("Failed to create collection\n");
        db_close(db);
        logger_close();
        return 1;
    }
    
    /* First, insert a document */
    json_value_t* doc = json_create_object();
    json_object_set(doc, "test", json_create_string("Test Document"));
    
    json_value_t* result = NULL;
    WITH_TIMEOUT(4, {
        result = db_insert_document(db, "test_collection", doc);
    }, 5);
    
    if (!result) {
        printf("Failed to insert document\n");
        db_close(db);
        logger_close();
        return 1;
    }
    
    json_free(result);
    
    /* Save database */
    WITH_TIMEOUT(5, {
        success = db_save(db);
    }, 5);
    
    if (!success) {
        printf("Failed to save database\n");
        db_close(db);
        logger_close();
        return 1;
    }
    
    /* Now enable cache */
    WITH_TIMEOUT(6, {
        success = db_enable_cache(db, 10, 0);
    }, 5);
    
    if (!success) {
        printf("Failed to enable cache\n");
        db_close(db);
        logger_close();
        return 1;
    }
    
    /* Try to query with cache enabled */
    json_value_t* query = json_create_object();
    json_value_t* query_result = NULL;
    
    WITH_TIMEOUT(7, {
        query_result = db_query_documents(db, "test_collection", query);
    }, 5);
    
    if (query_result) {
        printf("Query successful\n");
        json_free(query_result);
    } else {
        printf("Query failed\n");
    }
    
    json_free(query);
    
    /* Clean up */
    WITH_TIMEOUT(8, {
        db_close(db);
    }, 5);
    
    logger_close();
    
    printf("Test completed\n");
    return 0;
}