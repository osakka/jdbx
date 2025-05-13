#include "jsondb/database/database.h"
#include "jsondb/utils/logger.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * Database Initialization Test
 * 
 * Minimal test to check if database initialization works correctly.
 */

int main() {
    /* Print startup message */
    printf("Starting database initialization test\n");
    
    /* Initialize logger */
    printf("Initializing logger...\n");
    logger_init("db_init_test.log", LOG_LEVEL_DEBUG);
    printf("Logger initialized\n");
    
    /* Initialize database */
    const char* db_path = "db_init_test.json";
    printf("Initializing database at %s...\n", db_path);
    database_t* db = db_init(db_path);
    
    if (!db) {
        printf("Failed to initialize database\n");
        logger_close();
        return 1;
    }
    
    printf("Database initialized successfully\n");
    
    /* Close database */
    printf("Closing database...\n");
    db_close(db);
    printf("Database closed\n");
    
    /* Close logger */
    printf("Closing logger...\n");
    logger_close();
    printf("Logger closed\n");
    
    printf("Test completed successfully\n");
    return 0;
}