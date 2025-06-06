#include "jsondb/database/database.h"
#include "jsondb/utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

/**
 * Database Deadlock Test
 * 
 * This test investigates potential deadlocks in database operations
 * without using cache or other advanced features
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
    logger_init("db_deadlock_test.log", LOG_LEVEL_DEBUG);
    printf("Logger initialized\n");
    
    /* Initialize database */
    printf("Initializing database...\n");
    database_t* db = db_init("db_deadlock_test.json");
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
    
    /* Create document for insert */
    printf("Creating document...\n");
    json_value_t* doc = json_create_object();
    json_object_set(doc, "name", json_create_string("Test Document"));
    json_object_set(doc, "value", json_create_integer(42));
    printf("Document created\n");
    
    /* Prepare for insert */
    printf("Starting document insert (timeout: 5 seconds)...\n");
    set_timeout(5);
    printf("1. Creating identifier for document\n");
    
    /* Do the insert with debug points */
    printf("2. Locking database\n");
    pthread_mutex_lock(&db->lock);
    printf("3. Database locked\n");
    
    /* Get collection */
    json_value_t* collection = json_object_get(db->collections, "test_collection");
    printf("4. Collection retrieved\n");
    
    /* Get collection struct */
    printf("5. Getting collection struct\n");
    collection_t* coll = db_get_collection(db, "test_collection");
    if (!coll) {
        printf("Failed to get collection struct\n");
        pthread_mutex_unlock(&db->lock);
        json_free(doc);
        db_close(db);
        logger_close();
        return 1;
    }
    printf("6. Collection struct retrieved\n");
    
    /* Lock collection */
    printf("7. Locking collection\n");
    pthread_mutex_t* collection_lock = &coll->lock;
    pthread_mutex_lock(collection_lock);
    printf("8. Collection locked\n");
    
    /* Unlock in reverse order */
    printf("9. Unlocking collection\n");
    pthread_mutex_unlock(collection_lock);
    printf("10. Collection unlocked\n");
    
    printf("11. Unlocking database\n");
    pthread_mutex_unlock(&db->lock);
    printf("12. Database unlocked\n");
    
    /* Now try normal insert */
    printf("13. Now performing regular insert\n");
    json_value_t* result = db_insert_document(db, "test_collection", doc);
    printf("14. Insert completed\n");
    clear_timeout();
    
    if (!result) {
        printf("Failed to insert document\n");
        json_free(doc);
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
    set_timeout(5);
    db_close(db);
    clear_timeout();
    printf("Database closed\n");
    
    logger_close();
    
    printf("Test completed successfully\n");
    return 0;
}