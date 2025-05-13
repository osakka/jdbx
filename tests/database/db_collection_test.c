#include "jsondb/database/database.h"
#include "jsondb/utils/logger.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * Database Collection Test
 * 
 * Tests basic database initialization and collection creation.
 */

int main() {
    /* Print startup message */
    printf("Starting database collection test\n");
    
    /* Initialize logger */
    printf("Initializing logger...\n");
    logger_init("db_collection_test.log", LOG_LEVEL_DEBUG);
    printf("Logger initialized\n");
    
    /* Initialize database */
    const char* db_path = "db_collection_test.json";
    printf("Initializing database at %s...\n", db_path);
    database_t* db = db_init(db_path);
    
    if (!db) {
        printf("Failed to initialize database\n");
        logger_close();
        return 1;
    }
    
    printf("Database initialized successfully\n");
    
    /* Create collection */
    printf("Creating collection...\n");
    if (!db_create_collection(db, "test_collection")) {
        printf("Failed to create collection\n");
        db_close(db);
        logger_close();
        return 1;
    }
    
    printf("Collection created successfully\n");
    
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