#include "jsondb/database/database.h"
#include "jsondb/utils/logger.h"
#include "jsondb/utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

/**
 * Cache Insert Test
 * 
 * Tests enabling cache and then inserting a document.
 */

volatile int timeout_occurred = 0;

void timeout_handler(int signum) {
    (void)signum; // Unused parameter
    timeout_occurred = 1;
    printf("TIMEOUT: Operation took too long\n");
    
    // Force exit
    _exit(1);
}

int main() {
    printf("Starting cache and insert test\n");
    
    /* Install signal handler */
    signal(SIGALRM, timeout_handler);
    
    /* Initialize logger */
    logger_init("cache_insert.log", LOG_LEVEL_DEBUG);
    printf("Logger initialized\n");
    
    /* Initialize database */
    database_t* db = db_init("cache_insert.json");
    if (!db) {
        printf("Failed to initialize database\n");
        logger_close();
        return 1;
    }
    printf("Database initialized\n");
    
    /* Create collection */
    if (!db_create_collection(db, "test_collection")) {
        printf("Failed to create collection\n");
        db_close(db);
        logger_close();
        return 1;
    }
    printf("Collection created\n");
    
    /* Enable cache */
    printf("Enabling cache...\n");
    alarm(5); // 5 second timeout
    int success = db_enable_cache(db, 10, 0);
    alarm(0);
    
    if (timeout_occurred) {
        /* Handled in signal handler */
        return 1;
    } else if (success) {
        printf("Cache enabled successfully\n");
    } else {
        printf("Failed to enable cache\n");
        db_close(db);
        logger_close();
        return 1;
    }
    
    /* Insert document after enabling cache */
    printf("Inserting document with cache enabled (timeout: 5 seconds)...\n");
    json_value_t* doc = json_create_object();
    json_object_set(doc, "test", json_create_string("Cache Test Document"));
    
    /* Set alarm */
    alarm(5);
    
    /* Try to insert document */
    json_value_t* result = db_insert_document(db, "test_collection", doc);
    
    /* Cancel alarm */
    alarm(0);
    
    if (timeout_occurred) {
        /* Handled in signal handler */
        return 1;
    } else if (result) {
        printf("Document inserted successfully\n");
        json_free(result);
    } else {
        printf("Failed to insert document\n");
    }
    
    /* Clean up */
    printf("Closing database...\n");
    alarm(5);
    db_close(db);
    alarm(0);
    
    logger_close();
    
    printf("Test completed\n");
    return 0;
}