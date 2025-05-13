#include "jsondb/database/database.h"
#include "jsondb/utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

/**
 * Special Insert Debug Test
 * 
 * This test contains a simplified version of db_insert_document to diagnose deadlocks
 */

/* Signal handler for timeout */
void timeout_handler(int sig) {
    (void)sig;  /* Unused */
    fprintf(stderr, "TEST TIMEOUT: Possible deadlock detected\n");
    exit(1);
}

/* Set a timeout alarm */
void set_timeout(int seconds) {
    signal(SIGALRM, timeout_handler);
    alarm(seconds);
}

/* Clear timeout alarm */
void clear_timeout() {
    alarm(0);
}

/* Our simplified document insertion function */
json_value_t* debug_insert_document(database_t* db, const char* collection_name, json_value_t* document) {
    printf("STEP 1: Validating parameters\n");
    if (!db || !collection_name || !document || document->type != JSON_OBJECT) {
        printf("ERROR: Invalid parameters\n");
        return NULL;
    }
    
    printf("STEP 2: Cloning document\n");
    json_value_t* doc_copy = json_clone(document);
    if (!doc_copy) {
        printf("ERROR: Failed to clone document\n");
        return NULL;
    }
    
    printf("STEP 3: Checking/generating ID\n");
    if (!json_object_has(doc_copy, "_id")) {
        char* id = malloc(37);  /* UUID string plus null terminator */
        if (id) {
            sprintf(id, "debug-uuid-%ld", random());
            json_object_set(doc_copy, "_id", json_create_string(id));
            free(id);
        }
    }
    
    printf("STEP 4: Creating result object\n");
    json_value_t* result = json_create_object();
    if (!result) {
        printf("ERROR: Failed to create result\n");
        json_free(doc_copy);
        return NULL;
    }
    
    json_value_t* id = json_object_get(doc_copy, "_id");
    if (id && id->type == JSON_STRING) {
        json_object_set(result, "_id", json_create_string(id->value.string));
    }
    
    printf("STEP 5: Locking database\n");
    pthread_mutex_lock(&db->lock);
    printf("STEP 6: Database locked successfully\n");
    
    printf("STEP 7: Getting collection\n");
    json_value_t* collection = json_object_get(db->collections, collection_name);
    if (!collection || collection->type != JSON_ARRAY) {
        printf("ERROR: Collection not found\n");
        pthread_mutex_unlock(&db->lock);
        json_free(doc_copy);
        json_free(result);
        return NULL;
    }
    
    printf("STEP 8: Appending document to collection\n");
    json_array_append(collection, doc_copy);
    db->is_modified = 1;
    
    printf("STEP 9: Unlocking database\n");
    pthread_mutex_unlock(&db->lock);
    
    printf("STEP 10: Successfully inserted document\n");
    return result;
}

int main() {
    /* Initialize logger */
    logger_init("insert_debug.log", LOG_LEVEL_DEBUG);
    printf("Logger initialized\n");
    
    /* Initialize database */
    printf("Initializing database...\n");
    database_t* db = db_init("insert_debug.json");
    if (!db) {
        printf("Failed to initialize database\n");
        logger_close();
        return 1;
    }
    printf("Database initialized\n");
    
    /* Create collection */
    printf("Creating collection...\n");
    int success = 0;
    
    /* Collection might already exist, handle this case */
    pthread_mutex_lock(&db->lock);
    json_value_t* existing = json_object_get(db->collections, "test_collection");
    if (existing) {
        printf("Collection already exists\n");
        success = 1;
    } else {
        /* Create collection while we have the lock */
        json_value_t* new_collection = json_create_array();
        if (new_collection) {
            json_object_set(db->collections, "test_collection", new_collection);
            success = 1;
            printf("Collection created\n");
        }
    }
    pthread_mutex_unlock(&db->lock);
    
    if (!success) {
        printf("Failed to create/find collection\n");
        db_close(db);
        logger_close();
        return 1;
    }
    
    /* Create document */
    printf("Creating document...\n");
    json_value_t* doc = json_create_object();
    json_object_set(doc, "name", json_create_string("Test Document"));
    json_object_set(doc, "value", json_create_integer(42));
    printf("Document created\n");
    
    /* Insert document using our debug version */
    printf("Inserting document using debug function...\n");
    set_timeout(10);  /* Set 10 second timeout */
    json_value_t* result = debug_insert_document(db, "test_collection", doc);
    clear_timeout();
    
    if (!result) {
        printf("Failed to insert document\n");
        json_free(doc);
        db_close(db);
        logger_close();
        return 1;
    }
    
    printf("Document inserted with ID: %s\n", 
           json_get_string(json_object_get(result, "_id")));
    
    /* Free result and document */
    json_free(result);
    json_free(doc);
    
    /* Close database */
    printf("Closing database...\n");
    db_close(db);
    printf("Database closed\n");
    
    logger_close();
    printf("Test completed successfully\n");
    return 0;
}