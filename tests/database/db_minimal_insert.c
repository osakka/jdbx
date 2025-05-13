#include "jsondb/database/database.h"
#include "jsondb/utils/logger.h"
#include "jsondb/utils/json.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * Database Minimal Insert Test
 * 
 * A test with minimal document insertion to identify the hang.
 */

int main() {
    printf("Starting minimal insert test\n");
    
    /* Initialize logger */
    logger_init("db_minimal_insert.log", LOG_LEVEL_DEBUG);
    
    /* Initialize database */
    database_t* db = db_init("db_minimal_insert.json");
    if (!db) {
        printf("Failed to initialize database\n");
        logger_close();
        return 1;
    }
    
    /* Create collection */
    if (!db_create_collection(db, "minimal_collection")) {
        printf("Failed to create collection\n");
        db_close(db);
        logger_close();
        return 1;
    }
    
    /* Create the simplest possible document */
    json_value_t* doc = json_create_object();
    
    /* Try to insert the document */
    printf("Attempting to insert minimal document...\n");
    
    /* The hanging appears to be in the insertion - let's try to debug */
    json_value_t* result = db_insert_document(db, "minimal_collection", doc);
    
    if (result) {
        printf("Document inserted successfully\n");
        json_free(result);
    } else {
        printf("Failed to insert document\n");
    }
    
    /* Close database */
    db_close(db);
    
    /* Close logger */
    logger_close();
    
    printf("Test completed\n");
    return 0;
}