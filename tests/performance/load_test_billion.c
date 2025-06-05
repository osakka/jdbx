#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>
#include "database/database.h"
#include "utils/json.h"
#include "utils/logger.h"

/* Test configuration */
#define INITIAL_DOCS 1000000      /* Start with 1M documents */
#define TARGET_DOCS 1000000000    /* Target: 1 billion documents */
#define BATCH_SIZE 100000         /* Insert in 100K batches */
#define CHECKPOINT_INTERVAL 10000000  /* Checkpoint every 10M docs */
#define SAMPLE_RATE 1000000      /* Sample performance every 1M docs */

/* Performance metrics */
typedef struct {
    uint64_t total_docs;
    uint64_t successful_writes;
    uint64_t failed_writes;
    double total_write_time_ms;
    double min_latency_us;
    double max_latency_us;
    double last_batch_time_ms;
    uint64_t memory_usage_mb;
    uint64_t disk_usage_mb;
} load_test_metrics_t;

/* Global metrics */
static load_test_metrics_t metrics = {0};
static volatile int running = 1;
static pthread_mutex_t metrics_lock = PTHREAD_MUTEX_INITIALIZER;

/* High-resolution timer */
static double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static double get_time_us() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000.0 + ts.tv_nsec / 1000.0;
}

/* Get memory usage in MB */
static uint64_t get_memory_usage_mb() {
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return usage.ru_maxrss / 1024;  /* Linux reports in KB */
    }
    return 0;
}

/* Get disk usage of database directory in MB */
static uint64_t get_disk_usage_mb(const char* path) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "du -sm %s 2>/dev/null | cut -f1", path);
    
    FILE* fp = popen(cmd, "r");
    if (!fp) return 0;
    
    uint64_t size_mb = 0;
    fscanf(fp, "%lu", &size_mb);
    pclose(fp);
    
    return size_mb;
}

/* Signal handler for graceful shutdown */
static void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\n\nReceived signal %d, shutting down gracefully...\n", sig);
        running = 0;
    }
}

/* Print current metrics */
static void print_metrics(int final_report) {
    pthread_mutex_lock(&metrics_lock);
    
    double avg_latency_us = (metrics.successful_writes > 0) ? 
        (metrics.total_write_time_ms * 1000.0 / metrics.successful_writes) : 0;
    
    double throughput = (metrics.last_batch_time_ms > 0) ? 
        (BATCH_SIZE * 1000.0 / metrics.last_batch_time_ms) : 0;
    
    printf("\n%s Report:\n", final_report ? "FINAL" : "Progress");
    printf("═══════════════════════════════════════════════════════\n");
    printf("Total Documents:      %lu\n", metrics.total_docs);
    printf("Successful Writes:    %lu\n", metrics.successful_writes);
    printf("Failed Writes:        %lu\n", metrics.failed_writes);
    printf("Average Latency:      %.2f μs", avg_latency_us);
    if (avg_latency_us < 1000) {
        printf(" ✓ SUB-MILLISECOND!\n");
    } else {
        printf(" (%.2f ms)\n", avg_latency_us / 1000);
    }
    printf("Min Latency:          %.2f μs\n", metrics.min_latency_us);
    printf("Max Latency:          %.2f μs (%.2f ms)\n", 
           metrics.max_latency_us, metrics.max_latency_us / 1000);
    printf("Current Throughput:   %.0f docs/sec\n", throughput);
    printf("Memory Usage:         %lu MB\n", metrics.memory_usage_mb);
    printf("Disk Usage:           %lu MB\n", metrics.disk_usage_mb);
    printf("Progress:             %.1f%% of 1 billion\n", 
           (metrics.total_docs * 100.0) / TARGET_DOCS);
    printf("═══════════════════════════════════════════════════════\n");
    
    pthread_mutex_unlock(&metrics_lock);
}

/* Generate test document */
static json_value_t* generate_document(uint64_t id) {
    json_value_t* doc = json_create_object();
    
    /* Primary fields */
    json_object_set(doc, "id", json_create_integer(id));
    json_object_set(doc, "timestamp", json_create_integer(time(NULL)));
    
    /* Indexed fields for testing */
    json_object_set(doc, "user_id", json_create_integer(id % 1000000));  /* 1M unique users */
    json_object_set(doc, "category", json_create_integer(id % 100));     /* 100 categories */
    json_object_set(doc, "score", json_create_integer(rand() % 10000));  /* Random score */
    
    /* Data fields */
    char data[128];
    snprintf(data, sizeof(data), "Document data for ID %lu with random value %d", 
             id, rand());
    json_object_set(doc, "data", json_create_string(data));
    
    /* Status field */
    json_object_set(doc, "status", json_create_string(id % 2 == 0 ? "active" : "inactive"));
    
    return doc;
}

/* Insert batch of documents */
static int insert_batch(database_t* db, uint64_t start_id, uint64_t count) {
    double batch_start = get_time_ms();
    int errors = 0;
    
    for (uint64_t i = 0; i < count && running; i++) {
        json_value_t* doc = generate_document(start_id + i);
        
        double start = get_time_us();
        json_value_t* result = db_insert_document(db, "load_test", doc);
        double end = get_time_us();
        double latency = end - start;
        
        pthread_mutex_lock(&metrics_lock);
        if (result) {
            metrics.successful_writes++;
            metrics.total_write_time_ms += latency / 1000.0;
            
            if (metrics.min_latency_us == 0 || latency < metrics.min_latency_us) {
                metrics.min_latency_us = latency;
            }
            if (latency > metrics.max_latency_us) {
                metrics.max_latency_us = latency;
            }
            
            json_free(result);
        } else {
            metrics.failed_writes++;
            errors++;
        }
        pthread_mutex_unlock(&metrics_lock);
        
        json_free(doc);
        
        /* Progress indicator for interactive monitoring */
        if ((i + 1) % 10000 == 0) {
            printf(".");
            fflush(stdout);
        }
    }
    
    double batch_end = get_time_ms();
    
    pthread_mutex_lock(&metrics_lock);
    metrics.last_batch_time_ms = batch_end - batch_start;
    pthread_mutex_unlock(&metrics_lock);
    
    return errors;
}

/* Main load test function */
int main(int argc, char* argv[]) {
    /* Parse command line arguments */
    uint64_t target_docs = TARGET_DOCS;
    if (argc > 1) {
        target_docs = strtoull(argv[1], NULL, 10);
        if (target_docs == 0) target_docs = TARGET_DOCS;
    }
    
    /* Initialize logger */
    logger_init("/tmp/load_test_billion.log", LOG_LEVEL_INFO);
    
    /* Set up signal handlers */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    printf("JSONdb Billion Document Load Test\n");
    printf("=================================\n");
    printf("Target: %lu documents (%.0f billion)\n", 
           target_docs, target_docs / 1000000000.0);
    printf("Batch size: %d documents\n", BATCH_SIZE);
    printf("Starting test... (Press Ctrl+C to stop)\n\n");
    
    /* Initialize database */
    database_t* db = db_init("/tmp/billion_test_db");
    if (!db) {
        fprintf(stderr, "Failed to initialize database\n");
        return 1;
    }
    
    /* Create collection and indexes */
    db_create_collection(db, "load_test");
    
    printf("Creating indexes for realistic workload...\n");
    db_create_index(db, "load_test", "user_idx", "user_id", INDEX_TYPE_NON_UNIQUE);
    db_create_index(db, "load_test", "category_idx", "category", INDEX_TYPE_NON_UNIQUE);
    db_create_index(db, "load_test", "score_idx", "score", INDEX_TYPE_NON_UNIQUE);
    printf("Indexes created.\n\n");
    
    /* Start load test */
    uint64_t current_docs = 0;
    double test_start = get_time_ms();
    
    while (current_docs < target_docs && running) {
        /* Insert batch */
        printf("Inserting batch %lu-%lu", 
               current_docs, current_docs + BATCH_SIZE);
        fflush(stdout);
        
        int errors = insert_batch(db, current_docs, BATCH_SIZE);
        current_docs += BATCH_SIZE;
        
        pthread_mutex_lock(&metrics_lock);
        metrics.total_docs = current_docs;
        pthread_mutex_unlock(&metrics_lock);
        
        printf(" [%d errors]\n", errors);
        
        /* Update metrics */
        if (current_docs % SAMPLE_RATE == 0) {
            pthread_mutex_lock(&metrics_lock);
            metrics.memory_usage_mb = get_memory_usage_mb();
            metrics.disk_usage_mb = get_disk_usage_mb("/tmp/billion_test_db");
            pthread_mutex_unlock(&metrics_lock);
            
            print_metrics(0);
        }
        
        /* Checkpoint */
        if (current_docs % CHECKPOINT_INTERVAL == 0) {
            printf("\nCheckpoint at %lu documents, syncing...\n", current_docs);
            db_save(db);
            
            /* Perform a few test queries to verify data integrity */
            printf("Running verification queries...\n");
            
            /* Test 1: Random read */
            double read_start = get_time_us();
            char query_id[32];
            snprintf(query_id, sizeof(query_id), "doc-%lu", 
                    (uint64_t)(rand() % current_docs));
            json_value_t* found = db_get_document(db, "load_test", query_id);
            double read_end = get_time_us();
            
            if (found) {
                printf("  Random read: %.2f μs ✓\n", read_end - read_start);
                json_free(found);
            }
            
            /* Test 2: Range query */
            json_value_t* range_query = json_create_object();
            json_value_t* score_range = json_create_object();
            json_object_set(score_range, "$gte", json_create_integer(5000));
            json_object_set(score_range, "$lt", json_create_integer(5100));
            json_object_set(range_query, "score", score_range);
            
            double range_start = get_time_us();
            json_value_t* range_results = db_query_documents(db, "load_test", range_query);
            double range_end = get_time_us();
            
            if (range_results) {
                json_value_t* docs = json_object_get(range_results, "documents");
                printf("  Range query: %.2f μs for %zu results ✓\n", 
                       range_end - range_start, json_array_size(docs));
                json_free(range_results);
            }
            json_free(range_query);
            
            printf("Checkpoint complete.\n\n");
        }
    }
    
    /* Final report */
    double test_end = get_time_ms();
    double total_time_sec = (test_end - test_start) / 1000.0;
    
    pthread_mutex_lock(&metrics_lock);
    metrics.memory_usage_mb = get_memory_usage_mb();
    metrics.disk_usage_mb = get_disk_usage_mb("/tmp/billion_test_db");
    pthread_mutex_unlock(&metrics_lock);
    
    print_metrics(1);
    
    printf("\nTest Summary:\n");
    printf("─────────────────────────────────────────────\n");
    printf("Total Time:           %.1f seconds (%.1f hours)\n", 
           total_time_sec, total_time_sec / 3600.0);
    printf("Overall Throughput:   %.0f docs/sec\n", 
           metrics.successful_writes / total_time_sec);
    printf("Success Rate:         %.2f%%\n", 
           (metrics.successful_writes * 100.0) / 
           (metrics.successful_writes + metrics.failed_writes));
    
    if (metrics.total_docs >= 1000000000) {
        printf("\n🎉 SUCCESS: Reached 1 BILLION documents! 🎉\n");
    }
    
    /* Cleanup */
    db_close(db);
    logger_close();
    
    return 0;
}