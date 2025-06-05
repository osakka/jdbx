#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <assert.h>
#include <stdatomic.h>
#include "database/database.h"
#include "database/batch_operations.h"
#include "utils/json.h"
#include "utils/logger.h"
#include "utils/production_config.h"

/* Target: 100K ops/sec sustained load */
#define TARGET_OPS_PER_SEC 100000
#define NUM_THREADS 16
#define REPORT_INTERVAL 10  /* Report every 10 seconds */
#define BATCH_SIZE 100      /* Operations per batch */

/* Operation mix (percentages) */
#define READ_PERCENT 70
#define WRITE_PERCENT 20
#define UPDATE_PERCENT 5
#define DELETE_PERCENT 5

/* Shared state */
typedef struct {
    atomic_uint_fast64_t total_ops;
    atomic_uint_fast64_t successful_ops;
    atomic_uint_fast64_t failed_ops;
    atomic_uint_fast64_t read_ops;
    atomic_uint_fast64_t write_ops;
    atomic_uint_fast64_t update_ops;
    atomic_uint_fast64_t delete_ops;
    atomic_uint_fast64_t total_latency_us;
    atomic_uint_fast64_t max_latency_us;
    volatile int running;
    database_t* db;
    pthread_t threads[NUM_THREADS];
} stress_test_state_t;

static stress_test_state_t g_state = {0};

/* High-resolution timer */
static uint64_t get_time_us() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000;
}

/* Signal handler */
static void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\n\nReceived signal %d, shutting down...\n", sig);
        g_state.running = 0;
    }
}

/* Generate random operation type based on percentages */
static int get_operation_type() {
    int r = rand() % 100;
    if (r < READ_PERCENT) return 0;  /* READ */
    if (r < READ_PERCENT + WRITE_PERCENT) return 1;  /* WRITE */
    if (r < READ_PERCENT + WRITE_PERCENT + UPDATE_PERCENT) return 2;  /* UPDATE */
    return 3;  /* DELETE */
}

/* Worker thread function */
static void* stress_worker(void* arg) {
    int thread_id = *(int*)arg;
    free(arg);
    
    /* Thread-local random seed */
    srand(time(NULL) + thread_id);
    
    /* Pre-allocate document IDs for this thread */
    uint64_t doc_base = thread_id * 1000000;
    uint64_t next_doc_id = doc_base;
    
    char doc_id[64];
    uint64_t ops_count = 0;
    
    while (g_state.running) {
        int op_type = get_operation_type();
        uint64_t start_time = get_time_us();
        int success = 0;
        
        switch (op_type) {
            case 0: { /* READ */
                uint64_t random_id = doc_base + (rand() % (next_doc_id - doc_base + 1));
                snprintf(doc_id, sizeof(doc_id), "doc-%lu", random_id);
                
                json_value_t* doc = db_get_document(g_state.db, "stress_test", doc_id);
                if (doc) {
                    json_free(doc);
                    success = 1;
                }
                atomic_fetch_add(&g_state.read_ops, 1);
                break;
            }
            
            case 1: { /* WRITE */
                snprintf(doc_id, sizeof(doc_id), "doc-%lu", next_doc_id++);
                
                json_value_t* doc = json_create_object();
                json_object_set(doc, "_id", json_create_string(doc_id));
                json_object_set(doc, "thread", json_create_integer(thread_id));
                json_object_set(doc, "timestamp", json_create_integer(time(NULL)));
                json_object_set(doc, "value", json_create_integer(rand()));
                
                json_value_t* result = db_insert_document(g_state.db, "stress_test", doc);
                if (result) {
                    json_free(result);
                    success = 1;
                }
                json_free(doc);
                atomic_fetch_add(&g_state.write_ops, 1);
                break;
            }
            
            case 2: { /* UPDATE */
                uint64_t random_id = doc_base + (rand() % (next_doc_id - doc_base + 1));
                snprintf(doc_id, sizeof(doc_id), "doc-%lu", random_id);
                
                json_value_t* doc = json_create_object();
                json_object_set(doc, "updated_at", json_create_integer(time(NULL)));
                json_object_set(doc, "update_count", json_create_integer(ops_count));
                
                json_value_t* result = db_update_document(g_state.db, "stress_test", doc_id, doc);
                if (result) {
                    json_free(result);
                    success = 1;
                }
                json_free(doc);
                atomic_fetch_add(&g_state.update_ops, 1);
                break;
            }
            
            case 3: { /* DELETE */
                uint64_t random_id = doc_base + (rand() % (next_doc_id - doc_base + 1));
                snprintf(doc_id, sizeof(doc_id), "doc-%lu", random_id);
                
                success = (db_delete_document(g_state.db, "stress_test", doc_id) == 0);
                atomic_fetch_add(&g_state.delete_ops, 1);
                break;
            }
        }
        
        uint64_t end_time = get_time_us();
        uint64_t latency = end_time - start_time;
        
        atomic_fetch_add(&g_state.total_ops, 1);
        if (success) {
            atomic_fetch_add(&g_state.successful_ops, 1);
        } else {
            atomic_fetch_add(&g_state.failed_ops, 1);
        }
        
        atomic_fetch_add(&g_state.total_latency_us, latency);
        
        /* Update max latency if needed */
        uint64_t current_max = atomic_load(&g_state.max_latency_us);
        while (latency > current_max) {
            if (atomic_compare_exchange_weak(&g_state.max_latency_us, &current_max, latency)) {
                break;
            }
        }
        
        ops_count++;
        
        /* Rate limiting to achieve target ops/sec */
        if (ops_count % BATCH_SIZE == 0) {
            usleep(BATCH_SIZE * 1000000 / (TARGET_OPS_PER_SEC / NUM_THREADS));
        }
    }
    
    return NULL;
}

/* Monitoring thread */
static void* monitor_thread(void* arg) {
    (void)arg;
    
    uint64_t last_ops = 0;
    uint64_t last_time = get_time_us();
    
    while (g_state.running) {
        sleep(REPORT_INTERVAL);
        
        uint64_t current_ops = atomic_load(&g_state.total_ops);
        uint64_t current_time = get_time_us();
        uint64_t successful = atomic_load(&g_state.successful_ops);
        uint64_t failed = atomic_load(&g_state.failed_ops);
        
        double elapsed_sec = (current_time - last_time) / 1000000.0;
        double ops_per_sec = (current_ops - last_ops) / elapsed_sec;
        
        uint64_t total_latency = atomic_load(&g_state.total_latency_us);
        uint64_t max_latency = atomic_load(&g_state.max_latency_us);
        double avg_latency = successful > 0 ? (double)total_latency / successful : 0;
        
        printf("\n=== STRESS TEST STATUS ===\n");
        printf("Time: %ld seconds\n", (current_time - last_time) / 1000000);
        printf("Total operations: %lu\n", current_ops);
        printf("Successful: %lu (%.1f%%)\n", successful, 
               current_ops > 0 ? (successful * 100.0 / current_ops) : 0);
        printf("Failed: %lu\n", failed);
        printf("Current throughput: %.0f ops/sec", ops_per_sec);
        
        if (ops_per_sec >= TARGET_OPS_PER_SEC) {
            printf(" ✓ TARGET ACHIEVED!\n");
        } else {
            printf(" (%.1f%% of target)\n", (ops_per_sec * 100.0 / TARGET_OPS_PER_SEC));
        }
        
        printf("Average latency: %.2f μs", avg_latency);
        if (avg_latency < 1000) {
            printf(" ✓ SUB-MILLISECOND!\n");
        } else {
            printf(" (%.2f ms)\n", avg_latency / 1000);
        }
        printf("Max latency: %.2f μs (%.2f ms)\n", (double)max_latency, max_latency / 1000.0);
        
        printf("\nOperation breakdown:\n");
        printf("  Reads:   %lu (%.1f%%)\n", atomic_load(&g_state.read_ops),
               atomic_load(&g_state.read_ops) * 100.0 / current_ops);
        printf("  Writes:  %lu (%.1f%%)\n", atomic_load(&g_state.write_ops),
               atomic_load(&g_state.write_ops) * 100.0 / current_ops);
        printf("  Updates: %lu (%.1f%%)\n", atomic_load(&g_state.update_ops),
               atomic_load(&g_state.update_ops) * 100.0 / current_ops);
        printf("  Deletes: %lu (%.1f%%)\n", atomic_load(&g_state.delete_ops),
               atomic_load(&g_state.delete_ops) * 100.0 / current_ops);
        
        last_ops = current_ops;
        last_time = current_time;
    }
    
    return NULL;
}

int main(int argc, char* argv[]) {
    int duration_hours = 24;  /* Default 24 hours */
    
    if (argc > 1) {
        duration_hours = atoi(argv[1]);
        if (duration_hours <= 0) duration_hours = 24;
    }
    
    /* Set up signal handlers */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    /* Initialize */
    logger_init("/tmp/stress_test_sustained.log", LOG_LEVEL_INFO);
    production_config_init("large");  /* Use large config for stress test */
    
    printf("JSONdb Sustained Load Stress Test\n");
    printf("=================================\n");
    printf("Target: %d ops/sec for %d hours\n", TARGET_OPS_PER_SEC, duration_hours);
    printf("Threads: %d\n", NUM_THREADS);
    printf("Operation mix: %d%% read, %d%% write, %d%% update, %d%% delete\n",
           READ_PERCENT, WRITE_PERCENT, UPDATE_PERCENT, DELETE_PERCENT);
    printf("\nPress Ctrl+C to stop...\n\n");
    
    /* Initialize database */
    g_state.db = db_init("/tmp/stress_test_db");
    if (!g_state.db) {
        fprintf(stderr, "Failed to initialize database\n");
        return 1;
    }
    
    /* Create test collection with indexes */
    db_create_collection(g_state.db, "stress_test");
    db_create_index(g_state.db, "stress_test", "thread_idx", "thread", INDEX_TYPE_NON_UNIQUE);
    db_create_index(g_state.db, "stress_test", "timestamp_idx", "timestamp", INDEX_TYPE_NON_UNIQUE);
    
    g_state.running = 1;
    
    /* Start monitoring thread */
    pthread_t monitor;
    pthread_create(&monitor, NULL, monitor_thread, NULL);
    
    /* Start worker threads */
    printf("Starting %d worker threads...\n", NUM_THREADS);
    for (int i = 0; i < NUM_THREADS; i++) {
        int* thread_id = malloc(sizeof(int));
        *thread_id = i;
        pthread_create(&g_state.threads[i], NULL, stress_worker, thread_id);
    }
    
    /* Run for specified duration */
    time_t start_time = time(NULL);
    time_t target_end = start_time + (duration_hours * 3600);
    
    while (g_state.running && time(NULL) < target_end) {
        sleep(60);  /* Check every minute */
    }
    
    /* Shutdown */
    g_state.running = 0;
    
    printf("\nShutting down worker threads...\n");
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(g_state.threads[i], NULL);
    }
    pthread_join(monitor, NULL);
    
    /* Final report */
    printf("\n\n=== FINAL STRESS TEST REPORT ===\n");
    printf("Total duration: %.1f hours\n", (time(NULL) - start_time) / 3600.0);
    printf("Total operations: %lu\n", atomic_load(&g_state.total_ops));
    printf("Successful: %lu (%.2f%%)\n", atomic_load(&g_state.successful_ops),
           atomic_load(&g_state.successful_ops) * 100.0 / atomic_load(&g_state.total_ops));
    printf("Failed: %lu\n", atomic_load(&g_state.failed_ops));
    
    double avg_ops_sec = atomic_load(&g_state.total_ops) / (double)(time(NULL) - start_time);
    printf("Average throughput: %.0f ops/sec", avg_ops_sec);
    if (avg_ops_sec >= TARGET_OPS_PER_SEC) {
        printf(" ✓ TARGET ACHIEVED!\n");
    } else {
        printf(" (%.1f%% of target)\n", (avg_ops_sec * 100.0 / TARGET_OPS_PER_SEC));
    }
    
    db_close(g_state.db);
    logger_close();
    
    return 0;
}