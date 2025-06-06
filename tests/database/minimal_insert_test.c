#include "jsondb/database/database.h"
#include "jsondb/utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

/**
 * Minimal Insert Test
 * 
 * This test checks only database initialization and document insertion
 * without using cache to isolate potential issues
 */

/* Global timeout handler */
void timeout_handler(int sig) {
    fprintf(stderr, "\n\nTEST TIMEOUT: Operation took too long, possible deadlock\n");
    exit(1);
}

/* Set an alarm to prevent hanging */
void set_timeout(int seconds) {
    signal(SIGALRM, timeout_handler);
    alarm(seconds);
}

/* Clear alarm */
void clear_timeout() {
    alarm(0);
}

int main() {
    /* Set global timeout */
    set_timeout(30);
    
    /* Initialize logger */
    logger_init("minimal_insert_test.log", LOG_LEVEL_DEBUG);
    printf("Logger initialized\n");
    
    /* Initialize database */
    printf("Initializing database...\n");
    database_t* db = db_init("minimal_insert_test.json");
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
        printf("Failed to create collection\n");
        db_close(db);
        logger_close();
        return 1;
    }
    printf("Collection created\n");
    
    /* Insert document */
    printf("Inserting document...\n");
    json_value_t* doc = json_create_object();
    json_object_set(doc, "name", json_create_string("Test Document"));
    json_object_set(doc, "value", json_create_integer(42));
    
    printf("About to insert document (timeout: 10 seconds)...\n");
    set_timeout(10);
    json_value_t* result = db_insert_document(db, "test_collection", doc);
    clear_timeout();
    printf("Insert completed\n");
    
    if (!result) {
        printf("Failed to insert document\n");
        db_close(db);
        logger_close();
        return 1;
    }
    printf("Document inserted with ID: %s\n", 
           json_get_string(json_object_get(result, "uuid")));
    
    /* Free result */
    json_free(result);
    
    /* Close database */
    printf("Closing database...\n");
    set_timeout(10);
    db_close(db);
    clear_timeout();
    printf("Database closed\n");
    
    logger_close();
    
    printf("Test completed successfully\n");
    return 0;
}