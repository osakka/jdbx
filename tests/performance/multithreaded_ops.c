#include "jsondb/database/database.h"
#include "jsondb/utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

/**
 * This is a complete demonstration of deadlock-free thread operations
 * using a global database lock without complex collection locking
 */

#define NUM_THREADS 4
#define DOCS_PER_THREAD 5

/* Shared database */
database_t* db = NULL;
const char* collection_name = "thread_test_collection";

/* Thread-safe operation */
json_value_t* safe_insert_document(database_t* db, const char* collection_name, json_value_t* document) {
    if (!db || !collection_name || !document || document->type != JSON_OBJECT) {
        printf("Invalid parameters for safe_insert_document\n");
        return NULL;
    }
    
    /* Work with a copy of the document to prevent race conditions */
    json_value_t* doc_copy = json_clone(document);
    if (!doc_copy) {
        printf("Failed to clone document\n");
        return NULL;
    }
    
    /* Generate ID if needed - do this before getting any locks */
    if (!json_object_has(doc_copy, "uuid")) {
        char id[40];
        snprintf(id, sizeof(id), "thread-doc-%ld", random());
        json_object_set(doc_copy, "uuid", json_create_string(id));
    }
    
    /* Prepare result - also before locking */
    json_value_t* result = json_create_object();
    if (!result) {
        printf("Failed to create result object\n");
        json_free(doc_copy);
        return NULL;
    }
    
    /* Copy ID to result */
    json_value_t* id = json_object_get(doc_copy, "uuid");
    if (id && id->type == JSON_STRING) {
        json_object_set(result, "uuid", json_create_string(id->value.string));
    }
    
    /* Acquire single global database lock for thread safety */
    pthread_mutex_lock(&db->lock);
    
    /* Do all work while holding one lock */
    json_value_t* collection = json_object_get(db->collections, collection_name);
    if (!collection || collection->type != JSON_ARRAY) {
        printf("Collection not found: %s\n", collection_name);
        pthread_mutex_unlock(&db->lock);
        json_free(doc_copy);
        json_free(result);
        return NULL;
    }
    
    /* Insert document into collection */
    json_array_append(collection, doc_copy);
    db->is_modified = 1;
    
    /* Release the lock after all work is done */
    pthread_mutex_unlock(&db->lock);
    
    return result;
}

/* Thread function */
void* thread_worker(void* arg) {
    int thread_id = *((int*)arg);
    
    printf("Thread %d starting...\n", thread_id);
    
    for (int i = 0; i < DOCS_PER_THREAD; i++) {
        /* Create document */
        json_value_t* doc = json_create_object();
        if (!doc) {
            printf("Thread %d: Failed to create document %d\n", thread_id, i);
            continue;
        }
        
        char name[50];
        sprintf(name, "Thread %d Document %d", thread_id, i);
        json_object_set(doc, "name", json_create_string(name));
        json_object_set(doc, "thread_id", json_create_integer(thread_id));
        json_object_set(doc, "seq", json_create_integer(i));
        
        /* Insert document using our thread-safe function */
        printf("Thread %d: Inserting document %d...\n", thread_id, i);
        json_value_t* result = safe_insert_document(db, collection_name, doc);
        
        if (result) {
            printf("Thread %d: Successfully inserted document %d with ID: %s\n", 
                  thread_id, i, 
                  json_get_string(json_object_get(result, "uuid")));
            json_free(result);
        } else {
            printf("Thread %d: Failed to insert document %d\n", thread_id, i);
        }
        
        /* Clean up document */
        json_free(doc);
        
        /* Small delay to make thread interleaving more likely */
        usleep(10000);  /* 10ms */
    }
    
    printf("Thread %d completed all operations\n", thread_id);
    return NULL;
}

/* Query database to verify inserted documents */
void verify_documents() {
    printf("\nVerifying documents...\n");
    
    /* Lock database for read access */
    pthread_mutex_lock(&db->lock);
    
    json_value_t* collection = json_object_get(db->collections, collection_name);
    if (!collection || collection->type != JSON_ARRAY) {
        printf("Failed to find collection\n");
        pthread_mutex_unlock(&db->lock);
        return;
    }
    
    int total_docs = collection->value.array.size;
    printf("Found %d documents\n", total_docs);
    
    /* Expected document count */
    int expected = NUM_THREADS * DOCS_PER_THREAD;
    if (total_docs == expected) {
        printf("SUCCESS: Document count matches expected (%d)\n", expected);
    } else {
        printf("ERROR: Expected %d documents, but found %d\n", expected, total_docs);
    }
    
    /* Unlock database */
    pthread_mutex_unlock(&db->lock);
}

int main() {
    /* Initialize random seed */
    srandom(time(NULL));
    
    /* Initialize logger */
    logger_init("multithreaded_ops.log", LOG_LEVEL_DEBUG);
    printf("Logger initialized\n");
    
    /* Initialize database */
    printf("Initializing database...\n");
    db = db_init("multithreaded_ops.json");
    if (!db) {
        printf("Failed to initialize database\n");
        logger_close();
        return 1;
    }
    printf("Database initialized\n");
    
    /* Create collection for testing */
    printf("Creating test collection...\n");
    
    pthread_mutex_lock(&db->lock);
    
    /* Check if collection already exists */
    json_value_t* existing = json_object_get(db->collections, collection_name);
    if (existing) {
        printf("Collection already exists\n");
    } else {
        /* Create new collection */
        json_value_t* new_collection = json_create_array();
        if (new_collection) {
            json_object_set(db->collections, collection_name, new_collection);
            printf("Collection created\n");
        } else {
            printf("Failed to create collection\n");
            pthread_mutex_unlock(&db->lock);
            db_close(db);
            logger_close();
            return 1;
        }
    }
    
    pthread_mutex_unlock(&db->lock);
    
    /* Create worker threads */
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    printf("Creating %d worker threads...\n", NUM_THREADS);
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        if (pthread_create(&threads[i], NULL, thread_worker, &thread_ids[i]) != 0) {
            printf("Failed to create thread %d\n", i);
            db_close(db);
            logger_close();
            return 1;
        }
    }
    
    /* Wait for all threads to complete */
    printf("Waiting for threads to complete...\n");
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        printf("Thread %d joined\n", i);
    }
    
    /* Verify all documents were properly inserted */
    verify_documents();
    
    /* Close database */
    printf("Closing database...\n");
    db_close(db);
    printf("Database closed\n");
    
    logger_close();
    
    printf("Test completed successfully\n");
    return 0;
}