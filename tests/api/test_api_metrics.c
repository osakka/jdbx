/**
 * @file test_api_metrics.c
 * @brief Unit tests for API metrics module
 */

#include "test_framework.h"
#include "api/api_metrics.h"
#include "utils/metrics.h"
#include "utils/memory_manager.h"
#include "utils/logger.h"
#include "core/server.h"  /* For free_http_response */

/* External globals */
extern logger_config_t* g_logger;

/* Mock metrics registry for testing */
static metrics_registry_t* create_mock_metrics_registry() {
    metrics_registry_t* registry = metrics_registry_create();
    if (!registry) return NULL;
    
    /* Add some test metrics */
    metric_t* requests = metrics_create_counter(registry, "server_requests_total", "Total server requests");
    metric_t* connections = metrics_create_gauge(registry, "server_connections_active", "Active connections");
    metric_t* queries = metrics_create_counter(registry, "db_queries_total", "Total database queries");
    metric_t* memory = metrics_create_gauge(registry, "system_memory_used", "Memory usage in bytes");
    
    /* Set some values */
    if (requests) metrics_counter_inc(requests, 100);
    if (connections) metrics_gauge_set(connections, 5);
    if (queries) metrics_counter_inc(queries, 250);
    if (memory) metrics_gauge_set(memory, 1048576);
    
    return registry;
}

/* Test: Get all metrics */
static int test_metrics_get_all() {
    /* Create mock context and request */
    api_context_t* ctx = create_mock_api_context(NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    
    http_request_t* request = create_mock_request(HTTP_GET, "/api/metrics", NULL, NULL);
    TEST_ASSERT_NOT_NULL(request);
    
    /* Create mock metrics registry */
    metrics_registry_t* old_registry = g_metrics_registry;
    g_metrics_registry = create_mock_metrics_registry();
    TEST_ASSERT_NOT_NULL(g_metrics_registry);
    
    /* Call handler */
    http_response_t* response = api_handle_metrics_get(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_OK, response->status);
    
    /* Parse response */
    json_value_t* result = json_parse(response->body);
    TEST_ASSERT_NOT_NULL(result);
    
    /* Verify structure */
    json_value_t* metrics = json_object_get(result, "metrics");
    TEST_ASSERT_NOT_NULL(metrics);
    TEST_ASSERT_EQ(JSON_ARRAY, metrics->type);
    
#ifndef TOOLS_BUILD
    TEST_ASSERT(json_array_size(metrics) >= 4);
#else
    /* In tools build, metrics array is empty */
    TEST_ASSERT_EQ(0, json_array_size(metrics));
#endif
    
    /* Cleanup */
    /* CHECKPOINT: json_free(result); */
    free_http_response(response);
    metrics_registry_free(g_metrics_registry);
    g_metrics_registry = old_registry;
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    
    return 0;
}

/* Test: Filter metrics by type */
static int test_metrics_filter_by_type() {
    api_context_t* ctx = create_mock_api_context(NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    
    http_request_t* request = create_mock_request(HTTP_GET, "/api/metrics?type=counter", NULL, NULL);
    TEST_ASSERT_NOT_NULL(request);
    
    metrics_registry_t* old_registry = g_metrics_registry;
    g_metrics_registry = create_mock_metrics_registry();
    TEST_ASSERT_NOT_NULL(g_metrics_registry);
    
    http_response_t* response = api_handle_metrics_get(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_OK, response->status);
    
    json_value_t* result = json_parse(response->body);
    TEST_ASSERT_NOT_NULL(result);
    
    json_value_t* metrics = json_object_get(result, "metrics");
    TEST_ASSERT_NOT_NULL(metrics);
    
    /* Verify only counters returned */
    for (size_t i = 0; i < json_array_size(metrics); i++) {
        json_value_t* metric = json_array_get(metrics, i);
        json_value_t* type = json_object_get(metric, "type");
        TEST_ASSERT_NOT_NULL(type);
        TEST_ASSERT_STR_EQ("counter", json_get_string(type));
    }
    
    /* CHECKPOINT: json_free(result); */
    free_http_response(response);
    metrics_registry_free(g_metrics_registry);
    g_metrics_registry = old_registry;
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    
    return 0;
}

/* Test: Filter metrics by name */
static int test_metrics_filter_by_name() {
    api_context_t* ctx = create_mock_api_context(NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    
    http_request_t* request = create_mock_request(HTTP_GET, "/api/metrics?name=server", NULL, NULL);
    TEST_ASSERT_NOT_NULL(request);
    
    metrics_registry_t* old_registry = g_metrics_registry;
    g_metrics_registry = create_mock_metrics_registry();
    TEST_ASSERT_NOT_NULL(g_metrics_registry);
    
    http_response_t* response = api_handle_metrics_get(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_OK, response->status);
    
    json_value_t* result = json_parse(response->body);
    TEST_ASSERT_NOT_NULL(result);
    
    json_value_t* metrics = json_object_get(result, "metrics");
    TEST_ASSERT_NOT_NULL(metrics);
    
    /* Verify only server metrics returned */
    for (size_t i = 0; i < json_array_size(metrics); i++) {
        json_value_t* metric = json_array_get(metrics, i);
        json_value_t* name = json_object_get(metric, "name");
        TEST_ASSERT_NOT_NULL(name);
        const char* name_str = json_get_string(name);
        TEST_ASSERT(strstr(name_str, "server") != NULL);
    }
    
    /* CHECKPOINT: json_free(result); */
    free_http_response(response);
    metrics_registry_free(g_metrics_registry);
    g_metrics_registry = old_registry;
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    
    return 0;
}

/* Test: Get metrics in Prometheus format */
static int test_metrics_prometheus_format() {
    api_context_t* ctx = create_mock_api_context(NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    
    http_request_t* request = create_mock_request(HTTP_GET, "/metrics", NULL, NULL);
    TEST_ASSERT_NOT_NULL(request);
    
    metrics_registry_t* old_registry = g_metrics_registry;
    g_metrics_registry = create_mock_metrics_registry();
    TEST_ASSERT_NOT_NULL(g_metrics_registry);
    
    http_response_t* response = api_handle_metrics(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_OK, response->status);
    TEST_ASSERT_STR_EQ("text/plain", response->content_type);
    
    /* Verify Prometheus format */
#ifndef TOOLS_BUILD
    TEST_ASSERT(strstr(response->body, "# HELP") != NULL);
    TEST_ASSERT(strstr(response->body, "# TYPE") != NULL);
    TEST_ASSERT(strstr(response->body, "server_requests_total 100") != NULL);
    TEST_ASSERT(strstr(response->body, "server_connections_active 5") != NULL);
#else
    /* In tools build, we get empty metrics */
    TEST_ASSERT_STR_EQ("# Empty metrics\n", response->body);
#endif
    
    free_http_response(response);
    metrics_registry_free(g_metrics_registry);
    g_metrics_registry = old_registry;
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    
    return 0;
}

/* Test: Export metrics to file */
static int test_metrics_export() {
    api_context_t* ctx = create_mock_api_context(NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    
    /* Create export request */
    json_value_t* body = json_create_object();
    json_object_set(body, "export_path", json_create_string("/tmp/test_metrics.json"));
    char* body_str = json_stringify(body);
    
    http_request_t* request = create_mock_request(HTTP_POST, "/api/metrics/export", body_str, NULL);
    TEST_ASSERT_NOT_NULL(request);
    
    metrics_registry_t* old_registry = g_metrics_registry;
    g_metrics_registry = create_mock_metrics_registry();
    TEST_ASSERT_NOT_NULL(g_metrics_registry);
    
    http_response_t* response = api_handle_metrics_export(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    
#ifndef TOOLS_BUILD
    TEST_ASSERT_EQ(HTTP_OK, response->status);
    /* Verify export file created */
    TEST_ASSERT(access("/tmp/test_metrics.json", F_OK) == 0);
#else
    /* In tools build, export is not available */
    TEST_ASSERT_EQ(HTTP_INTERNAL_SERVER_ERROR, response->status);
#endif
    
    /* Cleanup */
    unlink("/tmp/test_metrics.json");
    free_http_response(response);
    metrics_registry_free(g_metrics_registry);
    g_metrics_registry = old_registry;
    BUFFER_FREE(body_str);
    /* CHECKPOINT: json_free(body); */
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    
    return 0;
}

/* Test: List available metrics */
static int test_metrics_available() {
    api_context_t* ctx = create_mock_api_context(NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    
    http_request_t* request = create_mock_request(HTTP_GET, "/api/metrics/available", NULL, NULL);
    TEST_ASSERT_NOT_NULL(request);
    
    metrics_registry_t* old_registry = g_metrics_registry;
    g_metrics_registry = create_mock_metrics_registry();
    TEST_ASSERT_NOT_NULL(g_metrics_registry);
    
    http_response_t* response = api_handle_metrics_available(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_OK, response->status);
    
    json_value_t* result = json_parse(response->body);
    TEST_ASSERT_NOT_NULL(result);
    
    /* Verify categorized structure */
    TEST_ASSERT_NOT_NULL(json_object_get(result, "counters"));
    TEST_ASSERT_NOT_NULL(json_object_get(result, "gauges"));
    TEST_ASSERT_NOT_NULL(json_object_get(result, "timers"));
    TEST_ASSERT_NOT_NULL(json_object_get(result, "histograms"));
    
    /* CHECKPOINT: json_free(result); */
    free_http_response(response);
    metrics_registry_free(g_metrics_registry);
    g_metrics_registry = old_registry;
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    
    return 0;
}

/* Test: Invalid request handling */
static int test_metrics_invalid_request() {
    /* Test NULL context */
    http_request_t* request = create_mock_request(HTTP_GET, "/api/metrics", NULL, NULL);
    http_response_t* response = api_handle_metrics_get(NULL, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_BAD_REQUEST, response->status);
    free_http_response(response);
    
    /* Test NULL request */
    api_context_t* ctx = create_mock_api_context(NULL);
    response = api_handle_metrics_get(ctx, NULL);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_BAD_REQUEST, response->status);
    free_http_response(response);
    
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    
    return 0;
}

/* Test: Performance of metrics retrieval */
static int test_metrics_performance_helper() {
    api_context_t* ctx = create_mock_api_context(NULL);
    http_request_t* request = create_mock_request(HTTP_GET, "/api/metrics", NULL, NULL);
    
    http_response_t* response = api_handle_metrics_get(ctx, request);
    free_http_response(response);
    
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    return 0;
}

static int test_metrics_performance() {
    metrics_registry_t* old_registry = g_metrics_registry;
    g_metrics_registry = create_mock_metrics_registry();
    
    /* Add more metrics for performance testing */
    for (int i = 0; i < 100; i++) {
        char name[64];
        snprintf(name, sizeof(name), "test_metric_%d", i);
        metric_t* m = metrics_create_counter(g_metrics_registry, name, "Test metric");
        if (m) metrics_counter_inc(m, i);
    }
    
    perf_test_t perf = {
        .name = "Metrics retrieval",
        .iterations = 1000,
        .target_ms = 100.0,  /* 0.1ms per request average */
    };
    
    run_performance_test(&perf, test_metrics_performance_helper);
    
    metrics_registry_free(g_metrics_registry);
    g_metrics_registry = old_registry;
    
    return perf.passed ? 0 : 1;
}

/* Test suite runner */
void run_api_metrics_tests() {
    RUN_TEST(test_metrics_get_all);
    RUN_TEST(test_metrics_filter_by_type);
    RUN_TEST(test_metrics_filter_by_name);
    RUN_TEST(test_metrics_prometheus_format);
    RUN_TEST(test_metrics_export);
    RUN_TEST(test_metrics_available);
    RUN_TEST(test_metrics_invalid_request);
    RUN_TEST(test_metrics_performance);
}

/* Main entry point */
int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "-v") == 0) {
        g_verbose_mode = 1;
    }
    
    /* Initialize memory manager first */
    memory_manager_init();
    
    /* Initialize logger */
    logger_init("test_api_metrics.log", LOG_LEVEL_ERROR);
    
    TEST_INIT();
    RUN_TEST_SUITE("API Metrics Module Tests", run_api_metrics_tests);
    TEST_SUMMARY();
    
    /* Cleanup */
    logger_close();
    
    return g_test_results.failed_tests > 0 ? 1 : 0;
}