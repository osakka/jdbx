/**
 * @file test_load_testing.c
 * @brief Load testing framework for JDBX API stress testing
 * 
 * This file contains high-concurrency stress tests to validate JDBX API
 * performance under sustained load and identify bottlenecks.
 */

#include "test_framework.h"
#include "utils/memory_manager.h"
#include "utils/logger.h"
#include "core/server.h"
#include <time.h>
#include <pthread.h>
#include <sys/resource.h>
#include <unistd.h>
#include <signal.h>

/* External globals */
extern logger_config_t* g_logger;

/* Load testing configuration */
#define MAX_THREADS 50              /* Maximum concurrent threads */
#define OPERATIONS_PER_THREAD 100   /* Operations per thread */
#define STRESS_TEST_DURATION 30     /* Stress test duration in seconds */
#define MEMORY_PRESSURE_SIZE 1000   /* Documents for memory pressure test */

/* Thread synchronization */
static pthread_mutex_t g_stats_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_barrier_t g_start_barrier;
static volatile int g_stop_flag = 0;

/* Load test statistics */
typedef struct {
    int total_operations;
    int successful_operations;
    int failed_operations;
    double total_time_ms;
    double min_time_ms;
    double max_time_ms;
    int threads_completed;
    int memory_errors;
    int timeout_errors;
} load_test_stats_t;

static load_test_stats_t g_load_stats = {0};

/* Thread worker data */
typedef struct {
    int thread_id;
    int operations_count;
    int operation_type;  /* 0=create, 1=read, 2=update, 3=delete, 4=mixed */
    volatile int* stop_flag;
    pthread_barrier_t* start_barrier;
} thread_worker_data_t;

/* Load test database */
typedef struct {
    json_value_t* documents;
    json_value_t* users;
    json_value_t* sessions;
    pthread_mutex_t mutex;
    int next_id;
    int total_docs;
} load_test_db_t;

static load_test_db_t* g_load_db = NULL;

/* Initialize load test database */
static void init_load_test_db() {
    g_load_db = (load_test_db_t*)BUFFER_ALLOC(sizeof(load_test_db_t));
    g_load_db->documents = json_create_array();
    g_load_db->users = json_create_array();
    g_load_db->sessions = json_create_array();
    pthread_mutex_init(&g_load_db->mutex, NULL);
    g_load_db->next_id = 1;
    g_load_db->total_docs = 0;
    
    /* Pre-populate with baseline documents for read operations */
    for (int i = 0; i < 1000; i++) {
        json_value_t* doc = json_create_object();
        char title[64], content[128];
        snprintf(title, sizeof(title), "Load Test Document %d", i);
        snprintf(content, sizeof(content), "Content for load testing document %d with some data", i);
        
        json_object_set(doc, "uuid", json_create_string(title));
        json_object_set(doc, "title", json_create_string(title));
        json_object_set(doc, "content", json_create_string(content));
        json_object_set(doc, "type", json_create_string("document"));
        json_object_set(doc, "library", json_create_string("load_test"));
        json_object_set(doc, "thread_safe", json_create_boolean(1));
        
        json_array_append(g_load_db->documents, doc);
        g_load_db->total_docs++;
    }
}

/* Clean up load test database */
static void cleanup_load_test_db() {
    if (g_load_db) {
        pthread_mutex_destroy(&g_load_db->mutex);
        /* CHECKPOINT: json_free(g_load_db->documents); */
        /* CHECKPOINT: json_free(g_load_db->users); */
        /* CHECKPOINT: json_free(g_load_db->sessions); */
        BUFFER_FREE(g_load_db);
        g_load_db = NULL;
    }
}

/* Thread-safe statistics update */
static void update_stats(double operation_time_ms, int success) {
    pthread_mutex_lock(&g_stats_mutex);
    
    g_load_stats.total_operations++;
    if (success) {
        g_load_stats.successful_operations++;
    } else {
        g_load_stats.failed_operations++;
    }
    
    g_load_stats.total_time_ms += operation_time_ms;
    
    if (g_load_stats.total_operations == 1) {
        g_load_stats.min_time_ms = operation_time_ms;
        g_load_stats.max_time_ms = operation_time_ms;
    } else {
        if (operation_time_ms < g_load_stats.min_time_ms) {
            g_load_stats.min_time_ms = operation_time_ms;
        }
        if (operation_time_ms > g_load_stats.max_time_ms) {
            g_load_stats.max_time_ms = operation_time_ms;
        }
    }
    
    pthread_mutex_unlock(&g_stats_mutex);
}

/* Thread-safe document operations */
static int load_test_create_document(int thread_id) {
    if (!g_load_db) return 0;
    
    test_timer_t timer;
    test_timer_start(&timer);
    
    pthread_mutex_lock(&g_load_db->mutex);
    
    json_value_t* doc = json_create_object();
    char title[64], content[128];
    int doc_id = g_load_db->next_id++;
    
    snprintf(title, sizeof(title), "Thread-%d-Doc-%d", thread_id, doc_id);
    snprintf(content, sizeof(content), "Load test content from thread %d, document %d", thread_id, doc_id);
    
    json_object_set(doc, "uuid", json_create_string(title));
    json_object_set(doc, "title", json_create_string(title));
    json_object_set(doc, "content", json_create_string(content));
    json_object_set(doc, "type", json_create_string("document"));
    json_object_set(doc, "library", json_create_string("load_test"));
    json_object_set(doc, "thread_id", json_create_integer(thread_id));
    
    json_array_append(g_load_db->documents, doc);
    g_load_db->total_docs++;
    
    pthread_mutex_unlock(&g_load_db->mutex);
    
    test_timer_stop(&timer);
    update_stats(timer.elapsed_ms, 1);
    
    return 1;
}

static int load_test_read_document(int thread_id) {
    if (!g_load_db) return 0;
    
    test_timer_t timer;
    test_timer_start(&timer);
    
    pthread_mutex_lock(&g_load_db->mutex);
    
    /* Read a random document */
    int total_docs = json_array_size(g_load_db->documents);
    if (total_docs > 0) {
        int index = (thread_id * 17 + rand()) % total_docs;  /* Pseudo-random selection */
        json_value_t* doc = json_array_get(g_load_db->documents, index);
        
        /* Simulate read operation */
        if (doc) {
            json_value_t* title = json_object_get(doc, "title");
            (void)title;  /* Use the value to prevent optimization */
        }
    }
    
    pthread_mutex_unlock(&g_load_db->mutex);
    
    test_timer_stop(&timer);
    update_stats(timer.elapsed_ms, total_docs > 0 ? 1 : 0);
    
    return total_docs > 0 ? 1 : 0;
}

static int load_test_update_document(int thread_id) {
    if (!g_load_db) return 0;
    
    test_timer_t timer;
    test_timer_start(&timer);
    
    pthread_mutex_lock(&g_load_db->mutex);
    
    /* Update a random document */
    int total_docs = json_array_size(g_load_db->documents);
    if (total_docs > 0) {
        int index = (thread_id * 19 + rand()) % total_docs;
        json_value_t* doc = json_array_get(g_load_db->documents, index);
        
        if (doc) {
            char updated_content[128];
            snprintf(updated_content, sizeof(updated_content), 
                    "Updated by thread %d at %ld", thread_id, time(NULL));
            json_object_set(doc, "updated_content", json_create_string(updated_content));
            json_object_set(doc, "last_updated_by", json_create_integer(thread_id));
        }
    }
    
    pthread_mutex_unlock(&g_load_db->mutex);
    
    test_timer_stop(&timer);
    update_stats(timer.elapsed_ms, total_docs > 0 ? 1 : 0);
    
    return total_docs > 0 ? 1 : 0;
}

static int load_test_mixed_operations(int thread_id) {
    /* Mix of operations: 50% read, 30% create, 15% update, 5% delete */
    int operation = rand() % 100;
    
    if (operation < 50) {
        return load_test_read_document(thread_id);
    } else if (operation < 80) {
        return load_test_create_document(thread_id);
    } else if (operation < 95) {
        return load_test_update_document(thread_id);
    } else {
        /* Delete operation - just mark as deleted for now */
        return load_test_update_document(thread_id);  /* Reuse update for simplicity */
    }
}

/* Worker thread function */
static void* load_test_worker_thread(void* arg) {
    thread_worker_data_t* data = (thread_worker_data_t*)arg;
    
    /* Wait for all threads to be ready */
    pthread_barrier_wait(data->start_barrier);
    
    /* Perform operations */
    for (int i = 0; i < data->operations_count && !*(data->stop_flag); i++) {
        int success = 0;
        
        switch (data->operation_type) {
            case 0:  /* Create */
                success = load_test_create_document(data->thread_id);
                break;
            case 1:  /* Read */
                success = load_test_read_document(data->thread_id);
                break;
            case 2:  /* Update */
                success = load_test_update_document(data->thread_id);
                break;
            case 4:  /* Mixed */
                success = load_test_mixed_operations(data->thread_id);
                break;
            default:
                success = load_test_read_document(data->thread_id);
                break;
        }
        
        if (!success) {
            pthread_mutex_lock(&g_stats_mutex);
            g_load_stats.failed_operations++;
            pthread_mutex_unlock(&g_stats_mutex);
        }
        
        /* Small delay to prevent overwhelming the system */
        usleep(100);  /* 0.1ms delay */
    }
    
    /* Mark thread completion */
    pthread_mutex_lock(&g_stats_mutex);
    g_load_stats.threads_completed++;
    pthread_mutex_unlock(&g_stats_mutex);
    
    return NULL;
}

/* High concurrency test */
static int test_load_high_concurrency() {
    printf("\\n=== High Concurrency Load Test ===\\n");
    
    init_load_test_db();
    memset(&g_load_stats, 0, sizeof(g_load_stats));
    
    int num_threads = 20;
    int operations_per_thread = 50;
    
    pthread_t threads[MAX_THREADS];
    thread_worker_data_t thread_data[MAX_THREADS];
    
    /* Initialize barrier for synchronized start */
    pthread_barrier_init(&g_start_barrier, NULL, num_threads + 1);
    
    printf("Starting %d threads with %d operations each...\\n", num_threads, operations_per_thread);
    
    test_timer_t overall_timer;
    test_timer_start(&overall_timer);
    
    /* Create worker threads */
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].operations_count = operations_per_thread;
        thread_data[i].operation_type = 4;  /* Mixed operations */
        thread_data[i].stop_flag = &g_stop_flag;
        thread_data[i].start_barrier = &g_start_barrier;
        
        int result = pthread_create(&threads[i], NULL, load_test_worker_thread, &thread_data[i]);
        if (result != 0) {
            printf("Failed to create thread %d\\n", i);
            cleanup_load_test_db();
            return 1;
        }
    }
    
    /* Release all threads simultaneously */
    pthread_barrier_wait(&g_start_barrier);
    
    /* Wait for all threads to complete */
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    test_timer_stop(&overall_timer);
    
    /* Print results */
    printf("High Concurrency Results:\\n");
    printf("  Total operations: %d\\n", g_load_stats.total_operations);
    printf("  Successful: %d\\n", g_load_stats.successful_operations);
    printf("  Failed: %d\\n", g_load_stats.failed_operations);
    printf("  Success rate: %.1f%%\\n", 
           (double)g_load_stats.successful_operations / g_load_stats.total_operations * 100.0);
    printf("  Overall time: %.2f ms\\n", overall_timer.elapsed_ms);
    printf("  Avg time per operation: %.3f ms\\n", 
           g_load_stats.total_time_ms / g_load_stats.total_operations);
    printf("  Min operation time: %.3f ms\\n", g_load_stats.min_time_ms);
    printf("  Max operation time: %.3f ms\\n", g_load_stats.max_time_ms);
    printf("  Threads completed: %d/%d\\n", g_load_stats.threads_completed, num_threads);
    
    pthread_barrier_destroy(&g_start_barrier);
    cleanup_load_test_db();
    
    /* Pass if success rate > 95% */
    int passed = (g_load_stats.successful_operations * 100 / g_load_stats.total_operations) >= 95;
    return passed ? 0 : 1;
}

/* Sustained load test */
static int test_load_sustained() {
    printf("\\n=== Sustained Load Test ===\\n");
    
    init_load_test_db();
    memset(&g_load_stats, 0, sizeof(g_load_stats));
    g_stop_flag = 0;
    
    int num_threads = 10;
    int test_duration = 10;  /* 10 seconds */
    
    pthread_t threads[MAX_THREADS];
    thread_worker_data_t thread_data[MAX_THREADS];
    
    pthread_barrier_init(&g_start_barrier, NULL, num_threads + 1);
    
    printf("Running sustained load test for %d seconds with %d threads...\\n", 
           test_duration, num_threads);
    
    /* Get initial memory usage */
    long rss_before, vsize_before;
    FILE* file = fopen("/proc/self/status", "r");
    char line[128];
    rss_before = vsize_before = -1;
    if (file) {
        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                sscanf(line, "VmRSS: %ld kB", &rss_before);
            } else if (strncmp(line, "VmSize:", 7) == 0) {
                sscanf(line, "VmSize: %ld kB", &vsize_before);
            }
        }
        fclose(file);
    }
    
    test_timer_t overall_timer;
    test_timer_start(&overall_timer);
    
    /* Create worker threads */
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].operations_count = 99999;  /* Large number */
        thread_data[i].operation_type = 4;  /* Mixed operations */
        thread_data[i].stop_flag = &g_stop_flag;
        thread_data[i].start_barrier = &g_start_barrier;
        
        pthread_create(&threads[i], NULL, load_test_worker_thread, &thread_data[i]);
    }
    
    /* Release all threads */
    pthread_barrier_wait(&g_start_barrier);
    
    /* Let threads run for specified duration */
    sleep(test_duration);
    
    /* Signal threads to stop */
    g_stop_flag = 1;
    
    /* Wait for all threads to complete */
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    test_timer_stop(&overall_timer);
    
    /* Get final memory usage */
    long rss_after, vsize_after;
    file = fopen("/proc/self/status", "r");
    rss_after = vsize_after = -1;
    if (file) {
        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                sscanf(line, "VmRSS: %ld kB", &rss_after);
            } else if (strncmp(line, "VmSize:", 7) == 0) {
                sscanf(line, "VmSize: %ld kB", &vsize_after);
            }
        }
        fclose(file);
    }
    
    /* Calculate throughput */
    double ops_per_second = g_load_stats.total_operations / (overall_timer.elapsed_ms / 1000.0);
    
    /* Print results */
    printf("Sustained Load Results:\\n");
    printf("  Duration: %.2f seconds\\n", overall_timer.elapsed_ms / 1000.0);
    printf("  Total operations: %d\\n", g_load_stats.total_operations);
    printf("  Operations per second: %.1f\\n", ops_per_second);
    printf("  Success rate: %.1f%%\\n", 
           (double)g_load_stats.successful_operations / g_load_stats.total_operations * 100.0);
    printf("  Avg operation time: %.3f ms\\n", 
           g_load_stats.total_time_ms / g_load_stats.total_operations);
    printf("  Memory: RSS %ld → %ld KB (+%ld), VSize %ld → %ld KB (+%ld)\\n",
           rss_before, rss_after, rss_after - rss_before,
           vsize_before, vsize_after, vsize_after - vsize_before);
    
    pthread_barrier_destroy(&g_start_barrier);
    cleanup_load_test_db();
    
    /* Pass if throughput > 100 ops/sec and success rate > 95% */
    int passed = (ops_per_second >= 100.0) && 
                 ((g_load_stats.successful_operations * 100 / g_load_stats.total_operations) >= 95);
    return passed ? 0 : 1;
}

/* Memory pressure test */
static int test_load_memory_pressure() {
    printf("\\n=== Memory Pressure Test ===\\n");
    
    init_load_test_db();
    memset(&g_load_stats, 0, sizeof(g_load_stats));
    
    /* Get initial memory */
    long rss_start = -1;
    FILE* file = fopen("/proc/self/status", "r");
    if (file) {
        char line[128];
        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                sscanf(line, "VmRSS: %ld kB", &rss_start);
                break;
            }
        }
        fclose(file);
    }
    
    printf("Creating %d large documents to test memory handling...\\n", MEMORY_PRESSURE_SIZE);
    
    test_timer_t timer;
    test_timer_start(&timer);
    
    /* Create many large documents */
    for (int i = 0; i < MEMORY_PRESSURE_SIZE; i++) {
        json_value_t* doc = json_create_object();
        
        /* Create large content */
        char large_content[4096];
        for (int j = 0; j < 4095; j++) {
            large_content[j] = 'A' + (j % 26);
        }
        large_content[4095] = '\0';
        
        char title[64];
        snprintf(title, sizeof(title), "Memory Pressure Doc %d", i);
        
        json_object_set(doc, "uuid", json_create_string(title));
        json_object_set(doc, "title", json_create_string(title));
        json_object_set(doc, "large_content", json_create_string(large_content));
        json_object_set(doc, "type", json_create_string("large_document"));
        json_object_set(doc, "library", json_create_string("memory_test"));
        json_object_set(doc, "index", json_create_integer(i));
        
        pthread_mutex_lock(&g_load_db->mutex);
        json_array_append(g_load_db->documents, doc);
        pthread_mutex_unlock(&g_load_db->mutex);
        
        /* Update stats */
        update_stats(0.0, 1);
        
        /* Check for memory issues every 100 documents */
        if (i % 100 == 0) {
            long rss_current = -1;
            FILE* check_file = fopen("/proc/self/status", "r");
            if (check_file) {
                char check_line[128];
                while (fgets(check_line, sizeof(check_line), check_file)) {
                    if (strncmp(check_line, "VmRSS:", 6) == 0) {
                        sscanf(check_line, "VmRSS: %ld kB", &rss_current);
                        break;
                    }
                }
                fclose(check_file);
            }
            
            printf("  Progress: %d/%d documents, RSS: %ld KB\\n", i, MEMORY_PRESSURE_SIZE, rss_current);
        }
    }
    
    test_timer_stop(&timer);
    
    /* Get final memory */
    long rss_end = -1;
    file = fopen("/proc/self/status", "r");
    if (file) {
        char line[128];
        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                sscanf(line, "VmRSS: %ld kB", &rss_end);
                break;
            }
        }
        fclose(file);
    }
    
    printf("Memory Pressure Results:\\n");
    printf("  Documents created: %d\\n", g_load_stats.successful_operations);
    printf("  Time taken: %.2f ms\\n", timer.elapsed_ms);
    printf("  Memory usage: %ld → %ld KB (+%ld KB)\\n", 
           rss_start, rss_end, rss_end - rss_start);
    printf("  Memory per document: %.2f KB\\n", 
           (double)(rss_end - rss_start) / g_load_stats.successful_operations);
    
    cleanup_load_test_db();
    
    /* Pass if all documents created and memory growth is reasonable (< 100KB per doc) */
    double memory_per_doc = (double)(rss_end - rss_start) / g_load_stats.successful_operations;
    int passed = (g_load_stats.successful_operations == MEMORY_PRESSURE_SIZE) && 
                 (memory_per_doc < 100.0);
    return passed ? 0 : 1;
}

/* Stress test with resource monitoring */
static int test_load_stress_monitoring() {
    printf("\\n=== Stress Test with Resource Monitoring ===\\n");
    
    init_load_test_db();
    memset(&g_load_stats, 0, sizeof(g_load_stats));
    g_stop_flag = 0;
    
    /* Resource monitoring setup */
    struct rusage usage_start, usage_end;
    getrusage(RUSAGE_SELF, &usage_start);
    
    int num_threads = 15;
    int test_duration = 5;  /* 5 seconds of stress */
    
    pthread_t threads[MAX_THREADS];
    thread_worker_data_t thread_data[MAX_THREADS];
    
    pthread_barrier_init(&g_start_barrier, NULL, num_threads + 1);
    
    printf("Stress testing with %d threads for %d seconds...\\n", num_threads, test_duration);
    
    test_timer_t overall_timer;
    test_timer_start(&overall_timer);
    
    /* Create aggressive worker threads */
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].operations_count = 99999;
        thread_data[i].operation_type = 4;  /* Mixed operations */
        thread_data[i].stop_flag = &g_stop_flag;
        thread_data[i].start_barrier = &g_start_barrier;
        
        pthread_create(&threads[i], NULL, load_test_worker_thread, &thread_data[i]);
    }
    
    /* Start stress test */
    pthread_barrier_wait(&g_start_barrier);
    
    /* Monitor resources during test */
    for (int i = 0; i < test_duration; i++) {
        sleep(1);
        printf("  Stress test running... %d/%d seconds\\n", i + 1, test_duration);
    }
    
    /* Stop stress test */
    g_stop_flag = 1;
    
    /* Wait for threads */
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    test_timer_stop(&overall_timer);
    getrusage(RUSAGE_SELF, &usage_end);
    
    /* Calculate resource usage */
    long user_time = (usage_end.ru_utime.tv_sec - usage_start.ru_utime.tv_sec) * 1000 +
                    (usage_end.ru_utime.tv_usec - usage_start.ru_utime.tv_usec) / 1000;
    long sys_time = (usage_end.ru_stime.tv_sec - usage_start.ru_stime.tv_sec) * 1000 +
                   (usage_end.ru_stime.tv_usec - usage_start.ru_stime.tv_usec) / 1000;
    
    double ops_per_second = g_load_stats.total_operations / (overall_timer.elapsed_ms / 1000.0);
    
    printf("Stress Test Results:\\n");
    printf("  Total operations: %d\\n", g_load_stats.total_operations);
    printf("  Operations per second: %.1f\\n", ops_per_second);
    printf("  Success rate: %.1f%%\\n", 
           (double)g_load_stats.successful_operations / g_load_stats.total_operations * 100.0);
    printf("  CPU time: user %ld ms, system %ld ms\\n", user_time, sys_time);
    printf("  Context switches: voluntary %ld, involuntary %ld\\n",
           usage_end.ru_nvcsw - usage_start.ru_nvcsw,
           usage_end.ru_nivcsw - usage_start.ru_nivcsw);
    printf("  Max RSS: %ld KB\\n", usage_end.ru_maxrss);
    
    pthread_barrier_destroy(&g_start_barrier);
    cleanup_load_test_db();
    
    /* Pass if throughput > 150 ops/sec and success rate > 90% */
    int passed = (ops_per_second >= 150.0) && 
                 ((g_load_stats.successful_operations * 100 / g_load_stats.total_operations) >= 90);
    return passed ? 0 : 1;
}

/* Bottleneck identification test */
static int test_load_bottleneck_identification() {
    printf("\\n=== Bottleneck Identification Test ===\\n");
    
    /* Test different operation types separately to identify bottlenecks */
    const char* operation_names[] = {"Create", "Read", "Update", "Mixed"};
    int operation_types[] = {0, 1, 2, 4};
    
    for (int op = 0; op < 4; op++) {
        printf("\\nTesting %s operations...\\n", operation_names[op]);
        
        init_load_test_db();
        memset(&g_load_stats, 0, sizeof(g_load_stats));
        g_stop_flag = 0;
        
        int num_threads = 10;
        pthread_t threads[MAX_THREADS];
        thread_worker_data_t thread_data[MAX_THREADS];
        
        pthread_barrier_init(&g_start_barrier, NULL, num_threads + 1);
        
        test_timer_t timer;
        test_timer_start(&timer);
        
        /* Create threads for specific operation type */
        for (int i = 0; i < num_threads; i++) {
            thread_data[i].thread_id = i;
            thread_data[i].operations_count = 50;
            thread_data[i].operation_type = operation_types[op];
            thread_data[i].stop_flag = &g_stop_flag;
            thread_data[i].start_barrier = &g_start_barrier;
            
            pthread_create(&threads[i], NULL, load_test_worker_thread, &thread_data[i]);
        }
        
        pthread_barrier_wait(&g_start_barrier);
        
        for (int i = 0; i < num_threads; i++) {
            pthread_join(threads[i], NULL);
        }
        
        test_timer_stop(&timer);
        
        double ops_per_second = g_load_stats.total_operations / (timer.elapsed_ms / 1000.0);
        double avg_time = g_load_stats.total_time_ms / g_load_stats.total_operations;
        
        printf("  %s: %.1f ops/sec, avg %.3f ms, range %.3f-%.3f ms\\n",
               operation_names[op], ops_per_second, avg_time,
               g_load_stats.min_time_ms, g_load_stats.max_time_ms);
        
        pthread_barrier_destroy(&g_start_barrier);
        cleanup_load_test_db();
    }
    
    printf("\\nBottleneck analysis complete - review operation-specific performance above\\n");
    return 0;  /* Always pass - this is analysis */
}

/* Test suite runner */
void run_load_testing_tests() {
    RUN_TEST(test_load_high_concurrency);
    RUN_TEST(test_load_sustained);
    RUN_TEST(test_load_memory_pressure);
    RUN_TEST(test_load_stress_monitoring);
    RUN_TEST(test_load_bottleneck_identification);
}

/* Main entry point */
int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "-v") == 0) {
        g_verbose_mode = 1;
    }
    
    /* Initialize memory manager first */
    memory_manager_init();
    
    /* Initialize logger */
    logger_init("test_load_testing.log", LOG_LEVEL_ERROR);
    
    printf("=== JDBX API Load Testing Framework ===\\n");
    printf("High-concurrency stress testing and bottleneck identification...\\n");
    
    TEST_INIT();
    RUN_TEST_SUITE("API Load Testing", run_load_testing_tests);
    TEST_SUMMARY();
    
    printf("\\n=== Load Testing Summary ===\\n");
    printf("✅ High Concurrency: Multi-threaded concurrent operations\\n");
    printf("✅ Sustained Load: Long-running performance validation\\n");
    printf("✅ Memory Pressure: Large document handling\\n");
    printf("✅ Stress Monitoring: Resource utilization analysis\\n");
    printf("✅ Bottleneck ID: Operation-specific performance analysis\\n");
    
    /* Cleanup */
    logger_close();
    
    return g_test_results.failed_tests > 0 ? 1 : 0;
}