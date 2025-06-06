#include "jsondb/database/database.h"
#include "jsondb/utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

/**
 * Simple Insert Test
 * Focus only on inserting a document with no caching
 */

int main() {
    /* Initialize logger */
    logger_init("simple_insert_test.log", LOG_LEVEL_DEBUG);
    printf("Logger initialized\n");
    
    /* Initialize database */
    printf("Initializing database...\n");
    database_t* db = db_init("simple_insert_test.json");
    if (!db) {
        printf("Failed to initialize database\n");
        logger_close();
        return 1;
    }
    printf("Database initialized\n");
    
    /* Create collection */
    printf("Creating collection...\n");
    int success = db_create_collection(db, "test_collection");
    if (!success) {
        printf("Collection may already exist, continuing anyway\n");
    } else {
        printf("Collection created\n");
    }
    
    /* Create document */
    printf("Creating document...\n");
    json_value_t* doc = json_create_object();
    if (!doc) {
        printf("Failed to create document\n");
        db_close(db);
        logger_close();
        return 1;
    }
    
    json_object_set(doc, "name", json_create_string("Simple Test Document"));
    json_object_set(doc, "value", json_create_integer(42));
    printf("Document created\n");
    
    /* Insert document */
    printf("Inserting document...\n");
    fflush(stdout);
    
    /* Use alarm for safety */
    alarm(5);
    
    json_value_t* result = db_insert_document(db, "test_collection", doc);
    
    /* Cancel alarm */
    alarm(0);
    
    if (!result) {
        printf("Failed to insert document\n");
        json_free(doc);
        db_close(db);
        logger_close();
        return 1;
    }
    
    /* Get document ID */
    json_value_t* id_val = json_object_get(result, "uuid");
    if (id_val && id_val->type == JSON_STRING) {
        printf("Document inserted with ID: %s\n", id_val->value.string);
    } else {
        printf("Document inserted but ID not found\n");
    }
    
    /* Clean up */
    json_free(doc);
    json_free(result);
    
    /* Close database */
    printf("Closing database...\n");
    db_close(db);
    printf("Database closed\n");
    
    logger_close();
    printf("Logger closed\n");
    
    printf("Test completed successfully\n");
    return 0;
}