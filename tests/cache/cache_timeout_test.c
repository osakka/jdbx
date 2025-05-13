#include "jsondb/database/database.h"
#include "jsondb/utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

/**
 * Cache Enable Timeout Test
 * 
 * A minimal test that only checks db_enable_cache with a short timeout.
 */

volatile int timeout_occurred = 0;

void timeout_handler(int signum) {
    (void)signum; // Unused parameter
    timeout_occurred = 1;
    printf("TIMEOUT: Enable cache operation took too long\n");
    
    // Force exit in case the main thread is completely stuck
    // This is not normally recommended, but we're specifically testing a hang issue
    _exit(1);
}

int main() {
    printf("Starting cache enable timeout test\n");
    
    /* Install signal handler */
    signal(SIGALRM, timeout_handler);
    
    /* Initialize logger */
    logger_init("cache_timeout.log", LOG_LEVEL_DEBUG);
    printf("Logger initialized\n");
    
    /* Initialize database */
    database_t* db = db_init("cache_timeout.json");
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
    
    /* Enable cache with timeout */
    printf("Attempting to enable cache (timeout: 5 seconds)...\n");
    
    /* Set alarm */
    alarm(5);
    
    /* Try to enable cache */
    int success = db_enable_cache(db, 10, 0);
    
    /* Cancel alarm */
    alarm(0);
    
    if (timeout_occurred) {
        /* Handled in signal handler */
        return 1;
    } else if (success) {
        printf("Cache enabled successfully\n");
    } else {
        printf("Failed to enable cache\n");
    }
    
    /* Clean up */
    db_close(db);
    logger_close();
    
    printf("Test completed\n");
    return 0;
}