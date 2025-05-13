#include "jsondb/database/database.h"
#include "jsondb/utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

/**
 * Even Simpler Test
 * Tests database operations with very minimal approach
 */

int main() {
    /* Initialize logger */
    logger_init("simpler_test.log", LOG_LEVEL_DEBUG);
    printf("Logger initialized\n");
    
    /* Initialize database with blank file (in-memory only) */
    printf("Initializing database...\n");
    database_t* db = db_init("simpler_test.json");
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

    /* Verify collection exists */
    json_value_t* collections = db_list_collections(db);
    if (collections) {
        printf("Collections in database: ");
        for (size_t i = 0; i < json_array_size(collections); i++) {
            json_value_t* name = json_array_get(collections, i);
            if (name && name->type == JSON_STRING) {
                printf("%s ", name->value.string);
            }
        }
        printf("\n");
        json_free(collections);
    }
    
    /* Create document */
    printf("Creating document...\n");
    json_value_t* doc = json_create_object();
    json_object_set(doc, "name", json_create_string("Simple Test"));
    json_object_set(doc, "value", json_create_integer(42));
    printf("Document created\n");
    
    /* Insert document */
    printf("Inserting document...\n");
    json_value_t* result = db_insert_document(db, "test_collection", doc);
    if (!result) {
        printf("Failed to insert document\n");
        db_close(db);
        logger_close();
        return 1;
    }
    printf("Document inserted with ID: %s\n",
           json_get_string(json_object_get(result, "_id")));

    /* Free result */
    json_free(result);

    /* Close database */
    printf("Closing database...\n");
    db_close(db);
    printf("Database closed\n");
    
    logger_close();
    
    printf("Test completed successfully\n");
    return 0;
}