#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include "database/database.h"
#include "utils/json.h"
#include "utils/logger.h"

/* Optimized configuration for billion-document testing */
#define BATCH_SIZE 10000         /* Smaller batches for consistent performance */
#define BUFFER_POOL_SIZE 1000    /* Reuse JSON string buffers */
#define CHECKPOINT_INTERVAL 100000
#define MAX_THREADS 8

typedef struct {
    database_t* db;
    int thread_id;
    uint64_t start_id;
    uint64_t count;
    double total_time_ms;
    int errors;
} thread_context_t;

/* Buffer pool for JSON strings */
static char* string_buffers[BUFFER_POOL_SIZE];
static int buffer_index = 0;
static pthread_mutex_t buffer_lock = PTHREAD_MUTEX_INITIALIZER;

static volatile int running = 1;

static double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static void handle_signal(int sig) {
    if (sig == SIGINT) {
        printf("\nShutting down gracefully...\n");
        running = 0;
    }
}

/* Get a buffer from the pool */
static char* get_buffer() {
    pthread_mutex_lock(&buffer_lock);
    char* buf = string_buffers[buffer_index];
    buffer_index = (buffer_index + 1) % BUFFER_POOL_SIZE;
    pthread_mutex_unlock(&buffer_lock);
    return buf;
}

/* Worker thread for parallel insertion */
static void* insert_worker(void* arg) {
    thread_context_t* ctx = (thread_context_t*)arg;
    double start_time = get_time_ms();
    
    /* Pre-create JSON template */
    char* buffer = get_buffer();
    
    for (uint64_t i = 0; i < ctx->count && running; i++) {
        uint64_t doc_id = ctx->start_id + i;
        
        /* Use pre-allocated buffer for JSON */
        snprintf(buffer, 512, 
            "{\"_id\":\"doc-%lu\",\"thread\":%d,\"seq\":%lu,"
            "\"user_id\":%lu,\"category\":%lu,\"score\":%d,"
            "\"data\":\"Document %lu with random %d\",\"active\":%s}",
            doc_id, ctx->thread_id, i,
            doc_id % 1000000, doc_id % 100, rand() % 10000,
            doc_id, rand() % 1000,
            (doc_id % 2) ? "true" : "false");
        
        /* Parse and insert */
        json_value_t* doc = json_parse(buffer);
        if (doc) {
            json_value_t* result = db_insert_document(ctx->db, "load_test", doc);
            if (!result) {
                ctx->errors++;
            } else {
                json_free(result);
            }
            json_free(doc);
        }
        
        /* Progress */
        if ((i + 1) % 10000 == 0) {
            printf("Thread %d: %lu documents inserted\r", 
                   ctx->thread_id, i + 1);
            fflush(stdout);
        }
    }
    
    ctx->total_time_ms = get_time_ms() - start_time;
    return NULL;
}

int main(int argc, char* argv[]) {
    uint64_t target_docs = 1000000;  /* Default 1M */
    int num_threads = 4;             /* Default 4 threads */
    
    if (argc > 1) {
        target_docs = strtoull(argv[1], NULL, 10);
    }
    if (argc > 2) {
        num_threads = atoi(argv[2]);
        if (num_threads > MAX_THREADS) num_threads = MAX_THREADS;
    }
    
    /* Initialize buffer pool */
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        string_buffers[i] = malloc(512);
    }
    
    signal(SIGINT, handle_signal);
    logger_init("/tmp/optimized_load_test.log", LOG_LEVEL_INFO);
    
    printf("Optimized JSONdb Load Test\n");
    printf("==========================\n");
    printf("Target: %lu documents\n", target_docs);
    printf("Threads: %d\n", num_threads);
    printf("Starting...\n\n");
    
    /* Initialize database */
    database_t* db = db_init("/tmp/optimized_test_db");
    if (!db) {
        fprintf(stderr, "Failed to initialize database\n");
        return 1;
    }
    
    /* Create collection without indexes initially */
    db_create_collection(db, "load_test");
    
    /* Phase 1: Bulk insertion */
    printf("Phase 1: Bulk document insertion\n");
    printf("--------------------------------\n");
    
    double phase1_start = get_time_ms();
    uint64_t docs_per_thread = target_docs / num_threads;
    
    pthread_t threads[MAX_THREADS];
    thread_context_t contexts[MAX_THREADS];
    
    /* Launch threads */
    for (int i = 0; i < num_threads; i++) {
        contexts[i].db = db;
        contexts[i].thread_id = i;
        contexts[i].start_id = i * docs_per_thread;
        contexts[i].count = (i == num_threads - 1) ? 
            target_docs - (i * docs_per_thread) : docs_per_thread;
        contexts[i].errors = 0;
        
        pthread_create(&threads[i], NULL, insert_worker, &contexts[i]);
    }
    
    /* Wait for completion */
    uint64_t total_errors = 0;
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        total_errors += contexts[i].errors;
        printf("\nThread %d: %.0f docs/sec\n", i,
               contexts[i].count * 1000.0 / contexts[i].total_time_ms);
    }
    
    double phase1_end = get_time_ms();
    double phase1_time = (phase1_end - phase1_start) / 1000.0;
    
    printf("\nPhase 1 Complete:\n");
    printf("  Total time: %.2f seconds\n", phase1_time);
    printf("  Documents: %lu\n", target_docs - total_errors);
    printf("  Errors: %lu\n", total_errors);
    printf("  Throughput: %.0f docs/sec\n", target_docs / phase1_time);
    
    /* Phase 2: Create indexes */
    printf("\nPhase 2: Creating indexes\n");
    printf("-------------------------\n");
    
    double idx_start = get_time_ms();
    db_create_index(db, "load_test", "user_idx", "user_id", INDEX_TYPE_NON_UNIQUE);
    double idx1_time = get_time_ms() - idx_start;
    printf("  user_id index: %.2f seconds\n", idx1_time / 1000.0);
    
    idx_start = get_time_ms();
    db_create_index(db, "load_test", "category_idx", "category", INDEX_TYPE_NON_UNIQUE);
    double idx2_time = get_time_ms() - idx_start;
    printf("  category index: %.2f seconds\n", idx2_time / 1000.0);
    
    /* Phase 3: Performance validation */
    printf("\nPhase 3: Performance validation\n");
    printf("-------------------------------\n");
    
    /* Test random reads */
    printf("Testing random reads...\n");
    double read_total = 0;
    int read_count = 1000;
    for (int i = 0; i < read_count; i++) {
        char id[64];
        snprintf(id, sizeof(id), "doc-%lu", (uint64_t)(rand() % target_docs));
        
        double start = get_time_ms();
        json_value_t* doc = db_get_document(db, "load_test", id);
        double end = get_time_ms();
        
        if (doc) {
            read_total += (end - start);
            json_free(doc);
        }
    }
    printf("  Average read latency: %.3f ms", read_total / read_count);
    if (read_total / read_count < 1.0) {
        printf(" ✓ SUB-MILLISECOND!\n");
    } else {
        printf("\n");
    }
    
    /* Test range queries */
    printf("Testing range queries...\n");
    json_value_t* query = json_create_object();
    json_value_t* range = json_create_object();
    json_object_set(range, "$gte", json_create_integer(500000));
    json_object_set(range, "$lt", json_create_integer(500100));
    json_object_set(query, "user_id", range);
    
    double range_start = get_time_ms();
    json_value_t* results = db_query_documents(db, "load_test", query);
    double range_end = get_time_ms();
    
    if (results) {
        json_value_t* docs = json_object_get(results, "documents");
        printf("  Range query: %.2f ms for %zu results", 
               range_end - range_start, json_array_size(docs));
        if (range_end - range_start < 1.0) {
            printf(" ✓ SUB-MILLISECOND!\n");
        } else {
            printf("\n");
        }
        json_free(results);
    }
    json_free(query);
    
    /* Final summary */
    printf("\n=== FINAL SUMMARY ===\n");
    printf("Total documents: %lu\n", target_docs);
    printf("Total time: %.2f seconds\n", 
           (get_time_ms() - phase1_start) / 1000.0);
    printf("Overall throughput: %.0f docs/sec\n", 
           target_docs * 1000.0 / (get_time_ms() - phase1_start));
    
    /* Cleanup */
    db_close(db);
    logger_close();
    
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        free(string_buffers[i]);
    }
    
    return 0;
}