#include "jsondb/utils/json.h"
#include "jsondb/utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

/**
 * Minimal Database Implementation Test
 * A completely simplified database implementation for testing
 * without using any of the real database code
 */

/* Signal handler for timeout */
void handle_timeout(int sig) {
    (void)sig;
    printf("\nTIMEOUT: Operation took too long to complete\n");
    exit(1);
}

/* Simplified database structure */
typedef struct {
    json_value_t* collections;
    pthread_mutex_t lock;
} mini_db_t;

/* Initialize database */
mini_db_t* mini_db_init() {
    mini_db_t* db = (mini_db_t*)malloc(sizeof(mini_db_t));
    if (!db) {
        return NULL;
    }
    
    /* Initialize collections object */
    db->collections = json_create_object();
    if (!db->collections) {
        free(db);
        return NULL;
    }
    
    /* Initialize mutex */
    pthread_mutex_init(&db->lock, NULL);
    
    return db;
}

/* Close database */
void mini_db_close(mini_db_t* db) {
    if (!db) {
        return;
    }
    
    /* Free resources */
    json_free(db->collections);
    pthread_mutex_destroy(&db->lock);
    free(db);
}

/* Create collection */
int mini_db_create_collection(mini_db_t* db, const char* name) {
    if (!db || !name) {
        return 0;
    }
    
    pthread_mutex_lock(&db->lock);
    
    /* Check if collection already exists */
    if (json_object_has(db->collections, name)) {
        pthread_mutex_unlock(&db->lock);
        return 0;
    }
    
    /* Create new collection */
    json_value_t* collection = json_create_array();
    if (!collection) {
        pthread_mutex_unlock(&db->lock);
        return 0;
    }
    
    /* Add collection to database */
    json_object_set(db->collections, name, collection);
    
    pthread_mutex_unlock(&db->lock);
    
    return 1;
}

/* Generate unique ID */
char* mini_generate_id() {
    static int counter = 0;
    char* id = (char*)malloc(37);
    if (!id) return NULL;
    
    sprintf(id, "test-id-%d", counter++);
    return id;
}

/* Insert document */
json_value_t* mini_db_insert_document(mini_db_t* db, const char* collection_name, json_value_t* document) {
    printf("  - Starting insert document operation\n");
    if (!db || !collection_name || !document || document->type != JSON_OBJECT) {
        printf("  - Invalid parameters\n");
        return NULL;
    }
    
    printf("  - Cloning document\n");
    /* Make a copy of document */
    json_value_t* doc_copy = json_clone(document);
    if (!doc_copy) {
        printf("  - Failed to clone document\n");
        return NULL;
    }
    
    printf("  - Adding ID to document\n");
    /* Add ID if not provided */
    if (!json_object_has(doc_copy, "_id")) {
        char* id = mini_generate_id();
        if (id) {
            json_object_set(doc_copy, "_id", json_create_string(id));
            free(id);
        } else {
            printf("  - Failed to generate ID\n");
            json_free(doc_copy);
            return NULL;
        }
    }
    
    printf("  - Getting document ID\n");
    /* Get document ID */
    json_value_t* id_val = json_object_get(doc_copy, "_id");
    if (!id_val || id_val->type != JSON_STRING) {
        printf("  - Document has no valid ID\n");
        json_free(doc_copy);
        return NULL;
    }
    
    printf("  - Creating result object\n");
    /* Create result object */
    json_value_t* result = json_create_object();
    if (!result) {
        printf("  - Failed to create result object\n");
        json_free(doc_copy);
        return NULL;
    }
    json_object_set(result, "_id", json_create_string(id_val->value.string));
    
    printf("  - Locking database\n");
    /* Lock database */
    pthread_mutex_lock(&db->lock);
    
    printf("  - Getting collection\n");
    /* Get collection */
    json_value_t* collection = json_object_get(db->collections, collection_name);
    if (!collection || collection->type != JSON_ARRAY) {
        printf("  - Collection not found\n");
        pthread_mutex_unlock(&db->lock);
        json_free(result);
        json_free(doc_copy);
        return NULL;
    }
    
    printf("  - Adding document to collection\n");
    /* Add document to collection */
    json_array_append(collection, doc_copy);
    
    printf("  - Unlocking database\n");
    /* Unlock database */
    pthread_mutex_unlock(&db->lock);
    
    printf("  - Insert operation completed successfully\n");
    return result;
}

/* Main function */
int main() {
    printf("=== Minimal Database Implementation Test ===\n");
    
    /* Initialize logger */
    logger_init("minimal_db_test.log", LOG_LEVEL_DEBUG);
    printf("Logger initialized\n");
    
    /* Step 1: Initialize database */
    printf("\nStep 1: Initializing minimal database... ");
    mini_db_t* db = mini_db_init();
    if (!db) {
        printf("FAILED\n");
        logger_close();
        return 1;
    }
    printf("SUCCESS\n");
    
    /* Step 2: Create collection */
    printf("\nStep 2: Creating test collection... ");
    int result = mini_db_create_collection(db, "test_collection");
    if (!result) {
        printf("FAILED\n");
        mini_db_close(db);
        logger_close();
        return 1;
    }
    printf("SUCCESS\n");
    
    /* Step 3: Insert document */
    printf("\nStep 3: Inserting document...\n");
    fflush(stdout);
    
    /* Set up timeout handler */
    signal(SIGALRM, handle_timeout);
    alarm(10);  /* 10-second timeout */
    
    /* Create document */
    json_value_t* doc = json_create_object();
    json_object_set(doc, "name", json_create_string("Test Document"));
    json_object_set(doc, "value", json_create_integer(42));
    
    /* Insert document */
    json_value_t* insert_result = mini_db_insert_document(db, "test_collection", doc);
    
    /* Cancel timeout */
    alarm(0);
    
    if (!insert_result) {
        printf("FAILED\n");
        json_free(doc);
        mini_db_close(db);
        logger_close();
        return 1;
    }
    
    /* Get document ID */
    json_value_t* id_val = json_object_get(insert_result, "_id");
    printf("SUCCESS: Document inserted with ID: %s\n", id_val->value.string);
    
    /* Clean up */
    json_free(insert_result);
    json_free(doc);
    mini_db_close(db);
    logger_close();
    
    printf("\n=== Test completed successfully! ===\n");
    return 0;
}