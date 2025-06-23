/**
 * @file test_framework.h
 * @brief JDBX API Testing Framework
 * 
 * Provides comprehensive testing infrastructure for API modules including
 * test harness, assertion macros, mocking utilities, and performance
 * measurement capabilities.
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include "api/api.h"
#include "utils/json.h"
#include "utils/buffer_pool.h"

/* Test result tracking */
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
    int skipped_tests;
    double total_time_ms;
} test_results_t;

/* Test fixture for setup/teardown */
typedef struct {
    void* (*setup)(void);
    void (*teardown)(void* context);
    void* context;
} test_fixture_t;

/* Performance measurement */
typedef struct {
    struct timeval start_time;
    struct timeval end_time;
    double elapsed_ms;
} test_timer_t;

/* Global test state */
extern test_results_t g_test_results;
extern int g_verbose_mode;

/* Test macros */
#define TEST_INIT() do { \
    memset(&g_test_results, 0, sizeof(test_results_t)); \
    printf("=== JDBX API Test Framework ===\n"); \
} while(0)

#define TEST_SUMMARY() do { \
    printf("\n=== Test Summary ===\n"); \
    printf("Total:   %d\n", g_test_results.total_tests); \
    printf("Passed:  %d\n", g_test_results.passed_tests); \
    printf("Failed:  %d\n", g_test_results.failed_tests); \
    printf("Skipped: %d\n", g_test_results.skipped_tests); \
    printf("Time:    %.2f ms\n", g_test_results.total_time_ms); \
    printf("Result:  %s\n", g_test_results.failed_tests == 0 ? "SUCCESS" : "FAILURE"); \
} while(0)

#define RUN_TEST(test_func) do { \
    printf("Running %s... ", #test_func); \
    fflush(stdout); \
    test_timer_t timer; \
    test_timer_start(&timer); \
    int result = test_func(); \
    test_timer_stop(&timer); \
    g_test_results.total_tests++; \
    g_test_results.total_time_ms += timer.elapsed_ms; \
    if (result == 0) { \
        g_test_results.passed_tests++; \
        printf("PASS (%.2f ms)\n", timer.elapsed_ms); \
    } else if (result == -1) { \
        g_test_results.skipped_tests++; \
        printf("SKIP\n"); \
    } else { \
        g_test_results.failed_tests++; \
        printf("FAIL (%.2f ms)\n", timer.elapsed_ms); \
    } \
} while(0)

#define RUN_TEST_SUITE(suite_name, suite_func) do { \
    printf("\n--- %s ---\n", suite_name); \
    suite_func(); \
} while(0)

/* Assertion macros */
#define TEST_ASSERT(condition) do { \
    if (!(condition)) { \
        printf("\n  Assertion failed: %s\n", #condition); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return 1; \
    } \
} while(0)

#define TEST_ASSERT_MSG(condition, msg) do { \
    if (!(condition)) { \
        printf("\n  Assertion failed: %s\n", #condition); \
        printf("  Message: %s\n", msg); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return 1; \
    } \
} while(0)

#define TEST_ASSERT_EQ(expected, actual) do { \
    if ((expected) != (actual)) { \
        printf("\n  Assertion failed: %s == %s\n", #expected, #actual); \
        printf("  Expected: %ld, Actual: %ld\n", (long)(expected), (long)(actual)); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return 1; \
    } \
} while(0)

#define TEST_ASSERT_STR_EQ(expected, actual) do { \
    if (strcmp((expected), (actual)) != 0) { \
        printf("\n  Assertion failed: %s == %s\n", #expected, #actual); \
        printf("  Expected: \"%s\"\n", (expected)); \
        printf("  Actual:   \"%s\"\n", (actual)); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return 1; \
    } \
} while(0)

#define TEST_ASSERT_NULL(ptr) do { \
    if ((ptr) != NULL) { \
        printf("\n  Assertion failed: %s == NULL\n", #ptr); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return 1; \
    } \
} while(0)

#define TEST_ASSERT_NOT_NULL(ptr) do { \
    if ((ptr) == NULL) { \
        printf("\n  Assertion failed: %s != NULL\n", #ptr); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return 1; \
    } \
} while(0)

/* Mock helpers */
typedef struct {
    const char* username;
    const char* token;
    const char* library;
    json_value_t* roles;
    int is_admin;
} mock_auth_context_t;

/* Function prototypes */
void test_timer_start(test_timer_t* timer);
void test_timer_stop(test_timer_t* timer);

/* Mock HTTP request/response creation */
http_request_t* create_mock_request(http_method_t method, const char* path, 
                                   const char* body, const char* auth_header);
void destroy_mock_request(http_request_t* request);

/* Mock API context creation */
api_context_t* create_mock_api_context(mock_auth_context_t* auth);
void destroy_mock_api_context(api_context_t* ctx);

/* JSON comparison utilities */
int json_values_equal(json_value_t* v1, json_value_t* v2);

/* Test data generators */
json_value_t* generate_test_document(const char* title, const char* content);
json_value_t* generate_test_user(const char* username, const char* email);
json_value_t* generate_test_role(const char* name, json_value_t* permissions);

/* Performance testing helpers */
typedef struct {
    const char* name;
    int iterations;
    double target_ms;
    double actual_ms;
    int passed;
} perf_test_t;

void run_performance_test(perf_test_t* test, int (*test_func)(void));

#endif /* TEST_FRAMEWORK_H */