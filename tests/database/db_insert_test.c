#include "jsondb/database/database.h"
#include "jsondb/utils/logger.h"
#include "jsondb/utils/json.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * Database Document Insertion Test
 * 
 * Tests database initialization, collection creation, and document insertion.
 */

int main() {
    /* Print startup message */
    printf("Starting database document insertion test\n");
    
    /* Initialize logger */
    printf("Initializing logger...\n");
    logger_init("db_insert_test.log", LOG_LEVEL_DEBUG);
    printf("Logger initialized\n");
    
    /* Initialize database */
    const char* db_path = "db_insert_test.json";
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
    
    /* Insert document */
    printf("Inserting document...\n");
    json_value_t* doc = json_create_object();
    json_object_set(doc, "name", json_create_string("Test Document"));
    json_object_set(doc, "value", json_create_integer(42));
    
    json_value_t* result = db_insert_document(db, "test_collection", doc);
    if (!result) {
        printf("Failed to insert document\n");
        db_close(db);
        logger_close();
        return 1;
    }
    
    printf("Document inserted successfully\n");
    json_free(result);
    
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