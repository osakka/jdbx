#include "jsondb/database/database.h"
#include "jsondb/utils/logger.h"
#include "jsondb/utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Database Cache Debug Utility
 * 
 * This program tests each step of database initialization and caching
 * to identify where the hang/issue occurs.
 */

int main() {
    /* Step 1: Initialize logger */
    printf("Step 1: Initializing logger\n");
    logger_init("db_cache_debug.log", LOG_LEVEL_DEBUG);
    printf("Logger initialized\n");
    
    /* Step 2: Initialize database */
    printf("Step 2: Initializing database\n");
    const char* db_path = "db_cache_debug.json";
    database_t* db = db_init(db_path);
    if (!db) {
        fprintf(stderr, "Failed to initialize database\n");
        logger_close();
        return 1;
    }
    printf("Database initialized\n");
    
    /* Step 3: Create collection */
    printf("Step 3: Creating collection\n");
    if (!db_create_collection(db, "debug_collection")) {
        fprintf(stderr, "Failed to create collection\n");
        db_close(db);
        logger_close();
        return 1;
    }
    printf("Collection created\n");
    
    /* Step 4: Insert a document */
    printf("Step 4: Inserting document\n");
    json_value_t* doc = json_create_object();
    json_object_set(doc, "test", json_create_string("Debug Document"));
    
    json_value_t* result = db_insert_document(db, "debug_collection", doc);
    if (!result) {
        fprintf(stderr, "Failed to insert document\n");
        db_close(db);
        logger_close();
        return 1;
    }
    json_free(result);
    printf("Document inserted\n");
    
    /* Step 5: Save database */
    printf("Step 5: Saving database\n");
    if (!db_save(db)) {
        fprintf(stderr, "Failed to save database\n");
        db_close(db);
        logger_close();
        return 1;
    }
    printf("Database saved\n");
    
    /* Step 6: Enable cache (potential hang point) */
    printf("Step 6: Enabling cache\n");
    /* We'll stop before this point to verify previous steps work */
    /*
    if (!db_enable_cache(db, 10, 0)) {
        fprintf(stderr, "Failed to enable cache\n");
        db_close(db);
        logger_close();
        return 1;
    }
    printf("Cache enabled\n");
    */
    
    /* Step 7: Close database */
    printf("Step 7: Closing database\n");
    db_close(db);
    printf("Database closed\n");
    
    /* Step 8: Close logger */
    printf("Step 8: Closing logger\n");
    logger_close();
    printf("Logger closed\n");
    
    printf("Debug test completed successfully\n");
    return 0;
}