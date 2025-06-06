#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include "database/database.h"
#include "utils/json.h"

#define NUM_READERS 10
#define NUM_WRITERS 2
#define OPERATIONS_PER_THREAD 1000

typedef struct {
    database_t *db;
    int thread_id;
    int operations;
    double elapsed_time;
} thread_data_t;

void* reader_thread(void* arg) {
    thread_data_t *data = (thread_data_t*)arg;
    struct timespec start, end;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < data->operations; i++) {
        json_value_t *query = json_parse("{}");
        json_value_t *result = db_query_documents(data->db, "test_collection", query);
        if (result) json_free(result);
        json_free(query);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    data->elapsed_time = (end.tv_sec - start.tv_sec) + 
                        (end.tv_nsec - start.tv_nsec) / 1e9;
    
    return NULL;
}

void* writer_thread(void* arg) {
    thread_data_t *data = (thread_data_t*)arg;
    struct timespec start, end;
    char doc_str[256];
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < data->operations; i++) {
        snprintf(doc_str, sizeof(doc_str), 
                "{\"thread_id\": %d, \"value\": %d}", 
                data->thread_id, i);
        json_value_t *doc = json_parse(doc_str);
        json_value_t *result = db_insert_document(data->db, "test_collection", doc);
        if (result) json_free(result);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    data->elapsed_time = (end.tv_sec - start.tv_sec) + 
                        (end.tv_nsec - start.tv_nsec) / 1e9;
    
    return NULL;
}

int main() {
    database_t *db = db_init("test_rwlock.db");
    if (!db) {
        fprintf(stderr, "Failed to initialize database\n");
        return 1;
    }
    
    // Create test collection
    db_create_collection(db, "test_collection");
    
    pthread_t readers[NUM_READERS];
    pthread_t writers[NUM_WRITERS];
    thread_data_t reader_data[NUM_READERS];
    thread_data_t writer_data[NUM_WRITERS];
    
    struct timespec test_start, test_end;
    clock_gettime(CLOCK_MONOTONIC, &test_start);
    
    // Start writer threads
    for (int i = 0; i < NUM_WRITERS; i++) {
        writer_data[i].db = db;
        writer_data[i].thread_id = i;
        writer_data[i].operations = OPERATIONS_PER_THREAD;
        pthread_create(&writers[i], NULL, writer_thread, &writer_data[i]);
    }
    
    // Start reader threads
    for (int i = 0; i < NUM_READERS; i++) {
        reader_data[i].db = db;
        reader_data[i].thread_id = i;
        reader_data[i].operations = OPERATIONS_PER_THREAD;
        pthread_create(&readers[i], NULL, reader_thread, &reader_data[i]);
    }
    
    // Wait for all threads
    for (int i = 0; i < NUM_WRITERS; i++) {
        pthread_join(writers[i], NULL);
    }
    for (int i = 0; i < NUM_READERS; i++) {
        pthread_join(readers[i], NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &test_end);
    double total_time = (test_end.tv_sec - test_start.tv_sec) + 
                       (test_end.tv_nsec - test_start.tv_nsec) / 1e9;
    
    // Calculate throughput
    int total_ops = (NUM_READERS + NUM_WRITERS) * OPERATIONS_PER_THREAD;
    double throughput = total_ops / total_time;
    
    printf("=== Concurrency Test Results ===\n");
    printf("Readers: %d, Writers: %d\n", NUM_READERS, NUM_WRITERS);
    printf("Operations per thread: %d\n", OPERATIONS_PER_THREAD);
    printf("Total operations: %d\n", total_ops);
    printf("Total time: %.3f seconds\n", total_time);
    printf("Throughput: %.0f ops/second\n", throughput);
    
    printf("\nReader thread times:\n");
    for (int i = 0; i < NUM_READERS; i++) {
        printf("  Reader %d: %.3f seconds\n", i, reader_data[i].elapsed_time);
    }
    
    printf("\nWriter thread times:\n");
    for (int i = 0; i < NUM_WRITERS; i++) {
        printf("  Writer %d: %.3f seconds\n", i, writer_data[i].elapsed_time);
    }
    
    db_close(db);
    return 0;
}