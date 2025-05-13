#include "jsondb/database/database.h"
#include "jsondb/utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

/**
 * Multi-thread Test
 * 
 * This test uses multiple threads to test for race conditions and deadlocks
 */

#define NUM_THREADS 4
#define DOCS_PER_THREAD 3

/* Shared database */
database_t* db = NULL;
const char* collection_name = "test_collection";

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

/* Thread function */
void* thread_worker(void* arg) {
    int thread_id = *((int*)arg);
    
    printf("Thread %d starting...\n", thread_id);
    
    for (int i = 0; i < DOCS_PER_THREAD; i++) {
        /* Create document */
        json_value_t* doc = json_create_object();
        char name[50];
        sprintf(name, "Document from thread %d #%d", thread_id, i);
        json_object_set(doc, "name", json_create_string(name));
        json_object_set(doc, "thread_id", json_create_integer(thread_id));
        json_object_set(doc, "doc_num", json_create_integer(i));
        
        /* Insert document */
        printf("Thread %d inserting document %d...\n", thread_id, i);
        json_value_t* result = db_insert_document(db, collection_name, doc);
        
        if (result) {
            printf("Thread %d inserted document %d with ID: %s\n", 
                  thread_id, i, json_get_string(json_object_get(result, "_id")));
            json_free(result);
        } else {
            printf("Thread %d failed to insert document %d\n", thread_id, i);
        }
        
        /* Sleep a bit to allow other threads to run */
        usleep(10000);  /* 10ms */
    }
    
    printf("Thread %d completed\n", thread_id);
    return NULL;
}

int main() {
    /* Set global timeout */
    set_timeout(60);  /* 60 second timeout */
    
    /* Initialize logger */
    logger_init("multi_thread_test.log", LOG_LEVEL_DEBUG);
    printf("Logger initialized\n");
    
    /* Initialize database */
    printf("Initializing database...\n");
    db = db_init("multi_thread_test.json");
    if (!db) {
        printf("Failed to initialize database\n");
        logger_close();
        return 1;
    }
    printf("Database initialized\n");
    
    /* Create collection */
    printf("Creating collection...\n");
    int success = db_create_collection(db, collection_name);
    if (!success) {
        printf("Failed to create collection\n");
        db_close(db);
        logger_close();
        return 1;
    }
    printf("Collection created\n");
    
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
    
    /* Close database */
    printf("Closing database...\n");
    db_close(db);
    printf("Database closed\n");
    
    logger_close();
    
    clear_timeout();
    printf("Test completed successfully\n");
    return 0;
}