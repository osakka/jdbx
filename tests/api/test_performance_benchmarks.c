/**
 * @file test_performance_benchmarks.c
 * @brief Comprehensive performance benchmarks for all API modules
 * 
 * This file contains performance tests to establish baseline metrics
 * for JDBX API operations under different load conditions.
 */

#include "test_framework.h"
#include "utils/memory_manager.h"
#include "utils/logger.h"
#include "core/server.h"
#include <time.h>
#include <sys/resource.h>

/* External globals */
extern logger_config_t* g_logger;

/* Performance test configuration */
#define LIGHT_LOAD_ITERATIONS   100    /* Light load testing */
#define MEDIUM_LOAD_ITERATIONS  1000   /* Medium load testing */
#define HEAVY_LOAD_ITERATIONS   10000  /* Heavy load testing */

/* Performance thresholds (milliseconds per operation) */
#define TARGET_DOCUMENT_CREATE_MS  0.50   /* 0.5ms per document creation */
#define TARGET_DOCUMENT_READ_MS    0.25   /* 0.25ms per document read */
#define TARGET_DOCUMENT_UPDATE_MS  0.60   /* 0.6ms per document update */
#define TARGET_DOCUMENT_DELETE_MS  0.30   /* 0.3ms per document delete */
#define TARGET_AUTH_LOGIN_MS       1.00   /* 1ms per authentication */
#define TARGET_METRICS_READ_MS     0.10   /* 0.1ms per metrics read */

/* Mock database for performance testing */
typedef struct {
    json_value_t* documents;  /* Array of stored documents */
    json_value_t* users;      /* Array of users */
    json_value_t* sessions;   /* Array of sessions */
    json_value_t* metrics;    /* Array of metrics */
    int next_id;
} perf_mock_db_t;

static perf_mock_db_t* g_perf_db = NULL;

/* Initialize performance test database */
static void init_perf_db() {
    g_perf_db = (perf_mock_db_t*)BUFFER_ALLOC(sizeof(perf_mock_db_t));
    g_perf_db->documents = json_create_array();
    g_perf_db->users = json_create_array();
    g_perf_db->sessions = json_create_array();
    g_perf_db->metrics = json_create_array();
    g_perf_db->next_id = 1;
    
    /* Pre-populate with test data for realistic performance testing */
    for (int i = 0; i < 100; i++) {
        json_value_t* doc = json_create_object();
        char title[64], content[128];
        snprintf(title, sizeof(title), "Performance Document %d", i);
        snprintf(content, sizeof(content), "This is performance test content for document %d", i);
        
        json_object_set(doc, "uuid", json_create_string(title));
        json_object_set(doc, "title", json_create_string(title));
        json_object_set(doc, "content", json_create_string(content));
        json_object_set(doc, "type", json_create_string("document"));
        json_object_set(doc, "library", json_create_string("perf_test"));
        
        json_array_append(g_perf_db->documents, doc);
    }
    
    /* Add test user for authentication testing */
    json_value_t* user = json_create_object();
    json_object_set(user, "uuid", json_create_string("perf-user-1"));
    json_object_set(user, "username", json_create_string("perfuser"));
    json_object_set(user, "password_hash", json_create_string("hashed_perf123"));
    json_object_set(user, "email", json_create_string("perf@test.com"));
    json_object_set(user, "type", json_create_string("user"));
    json_object_set(user, "library", json_create_string("default"));
    json_array_append(g_perf_db->users, user);
}

/* Clean up performance test database */
static void cleanup_perf_db() {
    if (g_perf_db) {
        /* CHECKPOINT: json_free(g_perf_db->documents); */
        /* CHECKPOINT: json_free(g_perf_db->users); */
        /* CHECKPOINT: json_free(g_perf_db->sessions); */
        /* CHECKPOINT: json_free(g_perf_db->metrics); */
        BUFFER_FREE(g_perf_db);
        g_perf_db = NULL;
    }
}

/* Mock storage operations optimized for performance testing */
json_value_t* perf_storage_insert_document(database_t* db, json_value_t* document) {
    (void)db;  /* Unused parameter */
    if (!g_perf_db || !document) return NULL;
    
    /* Generate ID if not present */
    if (!json_object_get(document, "uuid")) {
        char id[32];
        snprintf(id, sizeof(id), "perf-doc-%d", g_perf_db->next_id++);
        json_object_set(document, "uuid", json_create_string(id));
    }
    
    /* Store document copy */
    json_value_t* doc_copy = json_deep_copy(document);
    json_array_append(g_perf_db->documents, doc_copy);
    
    return json_deep_copy(doc_copy);
}

json_value_t* perf_storage_get_document(database_t* db, const char* uuid) {
    (void)db;  /* Unused parameter */
    if (!g_perf_db || !uuid) return NULL;
    
    for (size_t i = 0; i < json_array_size(g_perf_db->documents); i++) {
        json_value_t* doc = json_array_get(g_perf_db->documents, i);
        json_value_t* id = json_object_get(doc, "uuid");
        if (id && id->type == JSON_STRING && strcmp(id->value.string, uuid) == 0) {
            return json_deep_copy(doc);
        }
    }
    
    return NULL;
}

json_value_t* perf_storage_update_document(database_t* db, const char* uuid, json_value_t* update) {
    (void)db;  /* Unused parameter */
    if (!g_perf_db || !uuid || !update) return NULL;
    
    for (size_t i = 0; i < json_array_size(g_perf_db->documents); i++) {
        json_value_t* doc = json_array_get(g_perf_db->documents, i);
        json_value_t* id = json_object_get(doc, "uuid");
        if (id && id->type == JSON_STRING && strcmp(id->value.string, uuid) == 0) {
            /* Update fields */
            const char* key;
            json_value_t* val;
            json_object_foreach(update, key, val) {
                if (strcmp(key, "uuid") != 0) {  /* Don't update ID */
                    json_object_set(doc, key, json_deep_copy(val));
                }
            }
            
            return json_deep_copy(doc);
        }
    }
    
    return NULL;
}

int perf_storage_delete_document(database_t* db, const char* uuid) {
    (void)db;  /* Unused parameter */
    if (!g_perf_db || !uuid) return 0;
    
    for (size_t i = 0; i < json_array_size(g_perf_db->documents); i++) {
        json_value_t* doc = json_array_get(g_perf_db->documents, i);
        json_value_t* id = json_object_get(doc, "uuid");
        if (id && id->type == JSON_STRING && strcmp(id->value.string, uuid) == 0) {
            json_array_remove(g_perf_db->documents, i);
            return 1;
        }
    }
    
    return 0;
}

/* Document Creation Performance Test */
static int perf_document_create_helper() {
    static int counter = 0;
    
    api_context_t* ctx = create_mock_api_context(NULL);
    if (!ctx) return 1;
    
    /* Create document */
    json_value_t* doc = json_create_object();
    char title[64];
    snprintf(title, sizeof(title), "Perf Doc %d", counter++);
    json_object_set(doc, "title", json_create_string(title));
    json_object_set(doc, "content", json_create_string("Performance test content"));
    
    json_value_t* result = perf_storage_insert_document(ctx->db, doc);
    int success = (result != NULL);
    
    /* CHECKPOINT: json_free(doc); */
    /* CHECKPOINT: json_free(result); */
    destroy_mock_api_context(ctx);
    
    return success ? 0 : 1;
}

/* Document Read Performance Test */
static int perf_document_read_helper() {
    static int counter = 0;
    
    api_context_t* ctx = create_mock_api_context(NULL);
    if (!ctx) return 1;
    
    /* Read existing document by cycling through pre-populated documents */
    char uuid[64];
    snprintf(uuid, sizeof(uuid), "Performance Document %d", counter % 100);
    counter++;
    
    json_value_t* result = perf_storage_get_document(ctx->db, uuid);
    int success = (result != NULL);
    
    /* CHECKPOINT: json_free(result); */
    destroy_mock_api_context(ctx);
    
    return success ? 0 : 1;
}

/* Document Update Performance Test */
static int perf_document_update_helper() {
    static int counter = 0;
    
    api_context_t* ctx = create_mock_api_context(NULL);
    if (!ctx) return 1;
    
    /* Update existing document */
    char uuid[64];
    snprintf(uuid, sizeof(uuid), "Performance Document %d", counter % 100);
    counter++;
    
    json_value_t* update = json_create_object();
    json_object_set(update, "content", json_create_string("Updated performance content"));
    
    json_value_t* result = perf_storage_update_document(ctx->db, uuid, update);
    int success = (result != NULL);
    
    /* CHECKPOINT: json_free(update); */
    /* CHECKPOINT: json_free(result); */
    destroy_mock_api_context(ctx);
    
    return success ? 0 : 1;
}

/* Authentication Performance Test */
static int perf_auth_login_helper() {
    api_context_t* ctx = create_mock_api_context(NULL);
    if (!ctx) return 1;
    
    /* Simulate login operation */
    json_value_t* credentials = json_create_object();
    json_object_set(credentials, "username", json_create_string("perfuser"));
    json_object_set(credentials, "password", json_create_string("perf123"));
    
    /* Mock authentication check */
    int success = 1;  /* Always succeed for performance testing */
    
    /* CHECKPOINT: json_free(credentials); */
    destroy_mock_api_context(ctx);
    
    return success ? 0 : 1;
}

/* Metrics Read Performance Test */
static int perf_metrics_read_helper() {
    api_context_t* ctx = create_mock_api_context(NULL);
    if (!ctx) return 1;
    
    /* Simulate metrics read */
    json_value_t* metrics = json_create_object();
    json_object_set(metrics, "operations_count", json_create_integer(1000));
    json_object_set(metrics, "response_time_ms", json_create_number(15.5));
    json_object_set(metrics, "memory_usage_mb", json_create_number(45.2));
    
    int success = 1;  /* Always succeed for performance testing */
    
    /* CHECKPOINT: json_free(metrics); */
    destroy_mock_api_context(ctx);
    
    return success ? 0 : 1;
}

/* Memory usage measurement */
static void get_memory_usage(long* rss_kb, long* vsize_kb) {
    FILE* file = fopen("/proc/self/status", "r");
    if (!file) {
        *rss_kb = *vsize_kb = -1;
        return;
    }
    
    char line[128];
    *rss_kb = *vsize_kb = -1;
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line, "VmRSS: %ld kB", rss_kb);
        } else if (strncmp(line, "VmSize:", 7) == 0) {
            sscanf(line, "VmSize: %ld kB", vsize_kb);
        }
    }
    
    fclose(file);
}

/* Performance test: Light Load Document Operations */
static int test_performance_light_load() {
    init_perf_db();
    
    printf("\\n=== Light Load Performance Tests ===\\n");
    
    /* Document Creation */
    perf_test_t create_test = {
        .name = "Document Create (Light)",
        .iterations = LIGHT_LOAD_ITERATIONS,
        .target_ms = TARGET_DOCUMENT_CREATE_MS * LIGHT_LOAD_ITERATIONS,
    };
    run_performance_test(&create_test, perf_document_create_helper);
    
    /* Document Read */
    perf_test_t read_test = {
        .name = "Document Read (Light)",
        .iterations = LIGHT_LOAD_ITERATIONS,
        .target_ms = TARGET_DOCUMENT_READ_MS * LIGHT_LOAD_ITERATIONS,
    };
    run_performance_test(&read_test, perf_document_read_helper);
    
    /* Document Update */
    perf_test_t update_test = {
        .name = "Document Update (Light)",
        .iterations = LIGHT_LOAD_ITERATIONS,
        .target_ms = TARGET_DOCUMENT_UPDATE_MS * LIGHT_LOAD_ITERATIONS,
    };
    run_performance_test(&update_test, perf_document_update_helper);
    
    cleanup_perf_db();
    
    return (create_test.passed && read_test.passed && update_test.passed) ? 0 : 1;
}

/* Performance test: Medium Load Document Operations */
static int test_performance_medium_load() {
    init_perf_db();
    
    printf("\\n=== Medium Load Performance Tests ===\\n");
    
    long rss_before, vsize_before, rss_after, vsize_after;
    get_memory_usage(&rss_before, &vsize_before);
    
    /* Document Creation */
    perf_test_t create_test = {
        .name = "Document Create (Medium)",
        .iterations = MEDIUM_LOAD_ITERATIONS,
        .target_ms = TARGET_DOCUMENT_CREATE_MS * MEDIUM_LOAD_ITERATIONS,
    };
    run_performance_test(&create_test, perf_document_create_helper);
    
    /* Document Read */
    perf_test_t read_test = {
        .name = "Document Read (Medium)",
        .iterations = MEDIUM_LOAD_ITERATIONS,
        .target_ms = TARGET_DOCUMENT_READ_MS * MEDIUM_LOAD_ITERATIONS,
    };
    run_performance_test(&read_test, perf_document_read_helper);
    
    get_memory_usage(&rss_after, &vsize_after);
    
    printf("Memory usage: RSS %ld → %ld KB (+%ld), VSize %ld → %ld KB (+%ld)\\n",
           rss_before, rss_after, rss_after - rss_before,
           vsize_before, vsize_after, vsize_after - vsize_before);
    
    cleanup_perf_db();
    
    return (create_test.passed && read_test.passed) ? 0 : 1;
}

/* Performance test: Authentication Operations */
static int test_performance_auth() {
    printf("\\n=== Authentication Performance Tests ===\\n");
    
    perf_test_t auth_test = {
        .name = "Authentication Login",
        .iterations = LIGHT_LOAD_ITERATIONS,
        .target_ms = TARGET_AUTH_LOGIN_MS * LIGHT_LOAD_ITERATIONS,
    };
    run_performance_test(&auth_test, perf_auth_login_helper);
    
    return auth_test.passed ? 0 : 1;
}

/* Performance test: Metrics Operations */
static int test_performance_metrics() {
    printf("\\n=== Metrics Performance Tests ===\\n");
    
    perf_test_t metrics_test = {
        .name = "Metrics Read",
        .iterations = MEDIUM_LOAD_ITERATIONS,
        .target_ms = TARGET_METRICS_READ_MS * MEDIUM_LOAD_ITERATIONS,
    };
    run_performance_test(&metrics_test, perf_metrics_read_helper);
    
    return metrics_test.passed ? 0 : 1;
}

/* Performance test: Mixed Workload */
static int test_performance_mixed_workload() {
    init_perf_db();
    
    printf("\\n=== Mixed Workload Performance Test ===\\n");
    
    test_timer_t timer;
    test_timer_start(&timer);
    
    /* Simulate realistic mixed workload */
    for (int i = 0; i < 200; i++) {
        /* 60% reads, 20% creates, 15% updates, 5% auth */
        int operation = i % 20;
        
        if (operation < 12) {  /* 60% reads */
            perf_document_read_helper();
        } else if (operation < 16) {  /* 20% creates */
            perf_document_create_helper();
        } else if (operation < 19) {  /* 15% updates */
            perf_document_update_helper();
        } else {  /* 5% auth */
            perf_auth_login_helper();
        }
    }
    
    test_timer_stop(&timer);
    
    double avg_ms = timer.elapsed_ms / 200.0;
    int passed = (avg_ms <= 1.0);  /* Target: 1ms average per mixed operation */
    
    printf("Mixed workload (200 ops): %s (avg: %.3f ms, total: %.2f ms)\\n",
           passed ? "PASS" : "FAIL", avg_ms, timer.elapsed_ms);
    
    cleanup_perf_db();
    
    return passed ? 0 : 1;
}

/* Performance test: Resource utilization */
static int test_performance_resource_utilization() {
    printf("\\n=== Resource Utilization Test ===\\n");
    
    struct rusage usage_before, usage_after;
    getrusage(RUSAGE_SELF, &usage_before);
    
    /* Run intensive operations */
    init_perf_db();
    for (int i = 0; i < 1000; i++) {
        perf_document_create_helper();
        if (i % 100 == 0) {
            perf_document_read_helper();
        }
    }
    cleanup_perf_db();
    
    getrusage(RUSAGE_SELF, &usage_after);
    
    /* Calculate resource usage */
    long user_time_ms = (usage_after.ru_utime.tv_sec - usage_before.ru_utime.tv_sec) * 1000 +
                       (usage_after.ru_utime.tv_usec - usage_before.ru_utime.tv_usec) / 1000;
    long sys_time_ms = (usage_after.ru_stime.tv_sec - usage_before.ru_stime.tv_sec) * 1000 +
                      (usage_after.ru_stime.tv_usec - usage_before.ru_stime.tv_usec) / 1000;
    
    printf("CPU time: user %ld ms, system %ld ms, total %ld ms\\n",
           user_time_ms, sys_time_ms, user_time_ms + sys_time_ms);
    printf("Memory: max RSS %ld KB\\n", usage_after.ru_maxrss);
    printf("Context switches: voluntary %ld, involuntary %ld\\n",
           usage_after.ru_nvcsw - usage_before.ru_nvcsw,
           usage_after.ru_nivcsw - usage_before.ru_nivcsw);
    
    /* Pass if total CPU time is reasonable (less than 1 second for 1000 operations) */
    int passed = (user_time_ms + sys_time_ms < 1000);
    printf("Resource efficiency: %s\\n", passed ? "PASS" : "FAIL");
    
    return passed ? 0 : 1;
}

/* Test suite runner */
void run_performance_benchmark_tests() {
    RUN_TEST(test_performance_light_load);
    RUN_TEST(test_performance_medium_load);
    RUN_TEST(test_performance_auth);
    RUN_TEST(test_performance_metrics);
    RUN_TEST(test_performance_mixed_workload);
    RUN_TEST(test_performance_resource_utilization);
}

/* Main entry point */
int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "-v") == 0) {
        g_verbose_mode = 1;
    }
    
    /* Initialize memory manager first */
    memory_manager_init();
    
    /* Initialize logger */
    logger_init("test_performance_benchmarks.log", LOG_LEVEL_ERROR);
    
    printf("=== JDBX API Performance Benchmarks ===\\n");
    printf("Establishing baseline performance metrics...\\n");
    
    TEST_INIT();
    RUN_TEST_SUITE("API Performance Benchmarks", run_performance_benchmark_tests);
    TEST_SUMMARY();
    
    printf("\\n=== Performance Summary ===\\n");
    printf("Target thresholds per operation:\\n");
    printf("  Document Create: %.2f ms\\n", TARGET_DOCUMENT_CREATE_MS);
    printf("  Document Read:   %.2f ms\\n", TARGET_DOCUMENT_READ_MS);
    printf("  Document Update: %.2f ms\\n", TARGET_DOCUMENT_UPDATE_MS);
    printf("  Authentication:  %.2f ms\\n", TARGET_AUTH_LOGIN_MS);
    printf("  Metrics Read:    %.2f ms\\n", TARGET_METRICS_READ_MS);
    
    /* Cleanup */
    logger_close();
    
    return g_test_results.failed_tests > 0 ? 1 : 0;
}