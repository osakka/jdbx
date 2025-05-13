#include "jsondb/database/database.h"
#include "jsondb/utils/logger.h"
#include "jsondb/utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

/**
 * Database Timeout Test
 * 
 * This test uses alarm signals to prevent hanging when testing potentially problematic functions.
 */

volatile int timeout_occurred = 0;

void timeout_handler(int signum) {
    timeout_occurred = 1;
    printf("TIMEOUT: Operation took too long to complete\n");
}

#define TEST_WITH_TIMEOUT(test_name, code_block, timeout_seconds) do { \
    printf("=== Testing %s (timeout: %d seconds) ===\n", test_name, timeout_seconds); \
    timeout_occurred = 0; \
    signal(SIGALRM, timeout_handler); \
    alarm(timeout_seconds); \
    code_block; \
    alarm(0); \
    if (timeout_occurred) { \
        printf("FAILED: %s timed out after %d seconds\n", test_name, timeout_seconds); \
    } else { \
        printf("PASSED: %s completed successfully\n", test_name); \
    } \
    printf("\n"); \
} while(0)

int main() {
    printf("Starting database timeout test\n\n");
    
    database_t* db = NULL;
    
    // Test 1: Initialize logger
    TEST_WITH_TIMEOUT("Logger initialization", {
        logger_init("db_timeout_test.log", LOG_LEVEL_DEBUG);
    }, 5);
    
    // Test 2: Initialize database
    TEST_WITH_TIMEOUT("Database initialization", {
        db = db_init("db_timeout_test.json");
    }, 5);
    
    if (!db) {
        printf("Cannot continue tests - database initialization failed\n");
        logger_close();
        return 1;
    }
    
    // Test 3: Create collection
    TEST_WITH_TIMEOUT("Collection creation", {
        db_create_collection(db, "test_collection");
    }, 5);
    
    // Test 4: Insert simple document
    json_value_t* doc = json_create_object();
    TEST_WITH_TIMEOUT("Document insertion", {
        json_value_t* result = db_insert_document(db, "test_collection", doc);
        if (result) {
            json_free(result);
        }
    }, 10);
    
    // If document insertion worked, test querying
    if (!timeout_occurred) {
        // Test 5: Query documents
        TEST_WITH_TIMEOUT("Document query", {
            json_value_t* query = json_create_object();
            json_value_t* result = db_query_documents(db, "test_collection", query);
            if (result) {
                json_free(result);
            }
            json_free(query);
        }, 10);
    }
    
    // Test 6: Enable cache (this is where we suspect the problem is)
    TEST_WITH_TIMEOUT("Cache enable", {
        db_enable_cache(db, 10, 0);
    }, 10);
    
    // Test 7: Close database
    TEST_WITH_TIMEOUT("Database close", {
        if (db) {
            db_close(db);
            db = NULL;
        }
    }, 5);
    
    // Test 8: Close logger
    TEST_WITH_TIMEOUT("Logger close", {
        logger_close();
    }, 5);
    
    printf("Database timeout test completed\n");
    return 0;
}