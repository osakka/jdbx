#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include "database/database.h"
#include "utils/json.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"

typedef struct {
    database_t* db;
    int thread_id;
    int operations;
    int errors;
} thread_data_t;

void* worker_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    
    for (int i = 0; i < data->operations; i++) {
        json_value_t* doc = json_create_object();
        json_object_set(doc, "thread", json_create_integer(data->thread_id));
        json_object_set(doc, "seq", json_create_integer(i));
        json_object_set(doc, "timestamp", json_create_integer(time(NULL)));
        
        json_value_t* result = db_insert_document(data->db, "stress_test", doc);
        if (!result) {
            data->errors++;
            LOG_ERROR("Thread %d: Failed to insert document %d", data->thread_id, i);
        } else {
            json_free(result);
        }
        
        json_free(doc);
    }
    
    return NULL;
}

int main() {
    logger_init("/tmp/concurrent_stress.log", LOG_LEVEL_INFO);
    
    printf("Concurrent stress test...\n");
    
    database_t* db = db_init("/tmp/stress_db");
    assert(db != NULL);
    
    int rc = db_create_collection(db, "stress_test");
    assert(rc == 0);
    
    const int num_threads = 4;
    const int ops_per_thread = 100;
    pthread_t threads[num_threads];
    thread_data_t thread_data[num_threads];
    
    printf("Starting %d threads with %d operations each...\n", num_threads, ops_per_thread);
    
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].db = db;
        thread_data[i].thread_id = i;
        thread_data[i].operations = ops_per_thread;
        thread_data[i].errors = 0;
        
        pthread_create(&threads[i], NULL, worker_thread, &thread_data[i]);
    }
    
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        printf("Thread %d completed with %d errors\n", i, thread_data[i].errors);
    }
    
    // Check total documents
    json_value_t* info = db_list_collections_with_info(db);
    if (info) {
        char* str = json_stringify(info);
        printf("Collection info: %s\n", str);
        buffer_pool_free_safe(str);
        json_free(info);
    }
    
    db_close(db);
    logger_close();
    
    return 0;
}