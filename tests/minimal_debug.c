#include "jsondb/database/database.h"
#include "jsondb/utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

/**
 * Minimal Debug Test
 * Tests each operation individually
 */

int main(int argc, char *argv[]) {
    /* Check which test to run */
    if (argc < 2) {
        printf("Usage: %s <test_number>\n", argv[0]);
        printf("  1: db_init\n");
        printf("  2: db_create_collection\n");
        printf("  3: db_list_collections\n");
        printf("  4: db_insert_document\n");
        printf("  5: db_close\n");
        return 1;
    }
    
    int test_num = atoi(argv[1]);
    
    /* Initialize logger */
    logger_init("minimal_debug.log", LOG_LEVEL_DEBUG);
    printf("Logger initialized\n");
    
    /* Initialize database */
    if (test_num == 1) {
        printf("TEST 1: db_init\n");
        database_t* db = db_init("minimal_debug.json");
        if (!db) {
            printf("Failed to initialize database\n");
            logger_close();
            return 1;
        }
        printf("Database initialized successfully\n");
        db_close(db);
        return 0;
    }
    
    /* Initialize database for further tests */
    database_t* db = db_init("minimal_debug.json");
    if (!db) {
        printf("Failed to initialize database\n");
        logger_close();
        return 1;
    }
    printf("Database initialized\n");
    
    /* Create collection */
    if (test_num == 2) {
        printf("TEST 2: db_create_collection\n");
        int success = db_create_collection(db, "test_collection");
        if (!success) {
            printf("Collection may already exist, continuing anyway\n");
        } else {
            printf("Collection created successfully\n");
        }
        db_close(db);
        return 0;
    }
    
    /* List collections */
    if (test_num == 3) {
        printf("TEST 3: db_list_collections\n");
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
            printf("List collections completed successfully\n");
        } else {
            printf("Failed to list collections\n");
        }
        db_close(db);
        return 0;
    }
    
    /* Insert document */
    if (test_num == 4) {
        printf("TEST 4: db_insert_document\n");
        
        /* First make sure collection exists */
        int success = db_create_collection(db, "test_collection");
        if (!success) {
            printf("Collection may already exist, continuing anyway\n");
        }
        
        /* Create and insert document */
        json_value_t* doc = json_create_object();
        json_object_set(doc, "name", json_create_string("Debug Test"));
        json_object_set(doc, "value", json_create_integer(42));
        
        printf("About to insert document...\n");
        json_value_t* result = db_insert_document(db, "test_collection", doc);
        
        if (!result) {
            printf("Failed to insert document\n");
            db_close(db);
            logger_close();
            return 1;
        }
        
        printf("Document inserted with ID: %s\n", 
               json_get_string(json_object_get(result, "_id")));
        
        json_free(result);
        printf("Insert completed successfully\n");
        db_close(db);
        return 0;
    }
    
    /* Close database */
    if (test_num == 5) {
        printf("TEST 5: db_close\n");
        db_close(db);
        printf("Database closed successfully\n");
        return 0;
    }
    
    printf("Unknown test number: %d\n", test_num);
    db_close(db);
    logger_close();
    return 1;
}