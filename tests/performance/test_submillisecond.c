#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <stdint.h>
#include "database/database.h"
#include "utils/json.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"

/* High-resolution timer for sub-millisecond measurements */
static double get_time_us() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000.0 + ts.tv_nsec / 1000.0;
}

/* Measure single operation latency */
typedef struct {
    double min_us;
    double max_us;
    double avg_us;
    double p50_us;
    double p90_us;
    double p99_us;
    int count;
} latency_stats_t;

void update_latency_stats(latency_stats_t* stats, double* latencies, int count) {
    if (count == 0) return;
    
    /* Sort latencies for percentiles */
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (latencies[j] > latencies[j + 1]) {
                double temp = latencies[j];
                latencies[j] = latencies[j + 1];
                latencies[j + 1] = temp;
            }
        }
    }
    
    stats->min_us = latencies[0];
    stats->max_us = latencies[count - 1];
    
    double sum = 0;
    for (int i = 0; i < count; i++) {
        sum += latencies[i];
    }
    stats->avg_us = sum / count;
    
    stats->p50_us = latencies[count / 2];
    stats->p90_us = latencies[(int)(count * 0.90)];
    stats->p99_us = latencies[(int)(count * 0.99)];
    stats->count = count;
}

void print_latency_stats(const char* operation, latency_stats_t* stats) {
    printf("\n%s Latency Statistics (%d operations):\n", operation, stats->count);
    printf("  Min: %.2f μs\n", stats->min_us);
    printf("  Avg: %.2f μs\n", stats->avg_us);
    printf("  P50: %.2f μs\n", stats->p50_us);
    printf("  P90: %.2f μs\n", stats->p90_us);
    printf("  P99: %.2f μs\n", stats->p99_us);
    printf("  Max: %.2f μs\n", stats->max_us);
    
    /* Check if sub-millisecond */
    if (stats->p99_us < 1000) {
        printf("  ✓ SUB-MILLISECOND P99 ACHIEVED!\n");
    } else {
        printf("  ✗ P99 is %.2f ms (target: < 1ms)\n", stats->p99_us / 1000);
    }
}

int main() {
    logger_init("/tmp/test_submillisecond.log", LOG_LEVEL_INFO);
    
    /* Create database with pre-warmed data */
    database_t* db = database_create("test_perf");
    assert(db != NULL);
    
    printf("JSONdb Sub-millisecond Performance Test\n");
    printf("=====================================\n");
    
    /* Pre-populate with data to test realistic scenarios */
    const int warmup_docs = 1000;
    printf("\nWarming up with %d documents...\n", warmup_docs);
    
    for (int i = 0; i < warmup_docs; i++) {
        json_value_t* doc = json_create_object();
        json_object_set(doc, "id", json_create_integer(i));
        json_object_set(doc, "name", json_create_string("Test Document"));
        json_object_set(doc, "value", json_create_integer(i * 100));
        json_object_set(doc, "indexed", json_create_boolean(1));
        
        char* doc_id = db_insert_document(db, "test_collection", doc);
        if (!doc_id) {
            printf("Failed to insert warmup document %d\n", i);
            json_free(doc);
            break;
        }
        buffer_pool_free_safe(doc_id);
        json_free(doc);
    }
    
    /* Test 1: Insert Performance */
    printf("\n1. INSERT PERFORMANCE TEST\n");
    const int insert_count = 1000;
    double* insert_latencies = malloc(insert_count * sizeof(double));
    
    for (int i = 0; i < insert_count; i++) {
        json_t* doc = json_object();
        json_object_set(doc, "test_id", json_integer(i));
        json_object_set(doc, "data", json_string("Performance test document"));
        json_object_set(doc, "timestamp", json_integer(time(NULL)));
        
        double start = get_time_us();
        char* doc_id = db_insert_document(db, "perf_collection", doc);
        double end = get_time_us();
        
        if (doc_id) {
            insert_latencies[i] = end - start;
            free(doc_id);
        } else {
            insert_latencies[i] = -1;
        }
        json_decref(doc);
    }
    
    latency_stats_t insert_stats = {0};
    update_latency_stats(&insert_stats, insert_latencies, insert_count);
    print_latency_stats("INSERT", &insert_stats);
    
    /* Test 2: Point Query Performance (by ID) */
    printf("\n2. POINT QUERY PERFORMANCE TEST\n");
    const int query_count = 1000;
    double* query_latencies = malloc(query_count * sizeof(double));
    
    /* Get some document IDs to query */
    json_t* id_query = json_object();
    json_object_set(id_query, "test_id", json_object());
    json_object_set(json_object_get(id_query, "test_id"), "$exists", json_boolean(1));
    
    json_t* sample_docs = db_query_documents(db, "perf_collection", id_query, 0, 100);
    json_decref(id_query);
    
    if (sample_docs && json_array_size(sample_docs) > 0) {
        for (int i = 0; i < query_count; i++) {
            /* Pick a random document */
            int idx = i % json_array_size(sample_docs);
            json_t* sample = json_array_get(sample_docs, idx);
            const char* doc_id = json_string_value(json_object_get(sample, "uuid"));
            
            double start = get_time_us();
            json_t* result = db_get_document(db, "perf_collection", doc_id);
            double end = get_time_us();
            
            if (result) {
                query_latencies[i] = end - start;
                json_decref(result);
            } else {
                query_latencies[i] = -1;
            }
        }
        
        latency_stats_t query_stats = {0};
        update_latency_stats(&query_stats, query_latencies, query_count);
        print_latency_stats("POINT QUERY", &query_stats);
    }
    if (sample_docs) json_decref(sample_docs);
    
    /* Test 3: Update Performance */
    printf("\n3. UPDATE PERFORMANCE TEST\n");
    const int update_count = 1000;
    double* update_latencies = malloc(update_count * sizeof(double));
    
    /* Get documents to update */
    json_t* update_query = json_object();
    json_t* update_docs = db_query_documents(db, "perf_collection", update_query, 0, update_count);
    json_decref(update_query);
    
    if (update_docs && json_array_size(update_docs) > 0) {
        for (int i = 0; i < json_array_size(update_docs) && i < update_count; i++) {
            json_t* doc = json_array_get(update_docs, i);
            const char* doc_id = json_string_value(json_object_get(doc, "uuid"));
            
            json_t* update = json_object();
            json_object_set(update, "updated", json_boolean(1));
            json_object_set(update, "update_time", json_integer(time(NULL)));
            
            double start = get_time_us();
            int rc = db_update_document(db, "perf_collection", doc_id, update);
            double end = get_time_us();
            
            if (rc == 0) {
                update_latencies[i] = end - start;
            } else {
                update_latencies[i] = -1;
            }
            json_decref(update);
        }
        
        latency_stats_t update_stats = {0};
        update_latency_stats(&update_stats, update_latencies, 
                           json_array_size(update_docs) < update_count ? 
                           json_array_size(update_docs) : update_count);
        print_latency_stats("UPDATE", &update_stats);
    }
    if (update_docs) json_decref(update_docs);
    
    /* Test 4: Range Query Performance */
    printf("\n4. RANGE QUERY PERFORMANCE TEST\n");
    const int range_count = 100;
    double* range_latencies = malloc(range_count * sizeof(double));
    
    for (int i = 0; i < range_count; i++) {
        json_t* range_query = json_object();
        json_t* value_range = json_object();
        json_object_set(value_range, "$gte", json_integer(i * 10));
        json_object_set(value_range, "$lt", json_integer((i + 1) * 10));
        json_object_set(range_query, "value", value_range);
        
        double start = get_time_us();
        json_t* results = db_query_documents(db, "test_collection", range_query, 0, 10);
        double end = get_time_us();
        
        if (results) {
            range_latencies[i] = end - start;
            json_decref(results);
        } else {
            range_latencies[i] = -1;
        }
        json_decref(range_query);
    }
    
    latency_stats_t range_stats = {0};
    update_latency_stats(&range_stats, range_latencies, range_count);
    print_latency_stats("RANGE QUERY", &range_stats);
    
    /* Summary */
    printf("\n\nPERFORMANCE SUMMARY\n");
    printf("==================\n");
    printf("Sub-millisecond operations:\n");
    
    int sub_ms_count = 0;
    if (insert_stats.p99_us < 1000) {
        printf("  ✓ INSERT\n");
        sub_ms_count++;
    }
    if (query_stats.p99_us < 1000) {
        printf("  ✓ POINT QUERY\n");
        sub_ms_count++;
    }
    if (update_stats.p99_us < 1000) {
        printf("  ✓ UPDATE\n");
        sub_ms_count++;
    }
    if (range_stats.p99_us < 1000) {
        printf("  ✓ RANGE QUERY\n");
        sub_ms_count++;
    }
    
    printf("\nOverall: %d/4 operations achieve sub-millisecond P99 latency\n", sub_ms_count);
    
    /* Cleanup */
    free(insert_latencies);
    free(query_latencies);
    free(update_latencies);
    free(range_latencies);
    
    db_destroy(db);
    buffer_pool_cleanup();
    logger_close();
    
    return (sub_ms_count == 4) ? 0 : 1;
}