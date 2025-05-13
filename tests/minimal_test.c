#include "jsondb/database/database.h"
#include "jsondb/utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

/**
 * Minimal test for database initialization and cache
 */

int main() {
    /* Initialize logger */
    logger_init("minimal_test.log", LOG_LEVEL_DEBUG);
    printf("Logger initialized\n");
    
    /* Initialize database */
    printf("Initializing database...\n");
    database_t* db = db_init("minimal_test.json");
    if (!db) {
        printf("Failed to initialize database\n");
        logger_close();
        return 1;
    }
    printf("Database initialized\n");
    
    /* Create collection */
    printf("Creating collection...\n");
    const char* collection_name = "test_collection";

    /* Try to avoid any potential issues with the collection name */
    int success = 0;
    /* First check if collection already exists */
    json_value_t* existing = json_object_get(db->collections, collection_name);
    if (existing) {
        printf("Collection already exists, using existing one\n");
        success = 1;
    } else {
        /* Create it if it doesn't exist */
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
    
    /* Close database */
    printf("Closing database...\n");
    db_close(db);
    printf("Database closed\n");
    
    logger_close();
    
    printf("Test completed successfully\n");
    return 0;
}