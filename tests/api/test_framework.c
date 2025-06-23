/**
 * @file test_framework.c
 * @brief JDBX API Testing Framework Implementation
 */

#include "test_framework.h"
#include "database/database.h"
#include "rbac/rbac.h"
#include <stdarg.h>
#include <pthread.h>

/* Global test state */
test_results_t g_test_results = {0};
int g_verbose_mode = 0;

/* Timer implementation */
void test_timer_start(test_timer_t* timer) {
    gettimeofday(&timer->start_time, NULL);
}

void test_timer_stop(test_timer_t* timer) {
    gettimeofday(&timer->end_time, NULL);
    
    long seconds = timer->end_time.tv_sec - timer->start_time.tv_sec;
    long microseconds = timer->end_time.tv_usec - timer->start_time.tv_usec;
    timer->elapsed_ms = (seconds * 1000.0) + (microseconds / 1000.0);
}

/* Mock HTTP request creation */
http_request_t* create_mock_request(http_method_t method, const char* path, 
                                   const char* body, const char* auth_header) {
    http_request_t* request = (http_request_t*)BUFFER_ALLOC(sizeof(http_request_t));
    if (!request) return NULL;
    
    memset(request, 0, sizeof(http_request_t));
    request->method = method;
    request->path = BUFFER_STRDUP(path);
    
    if (body) {
        request->body = BUFFER_STRDUP(body);
        request->content_length = strlen(body);
        request->content_type = BUFFER_STRDUP("application/json");
    }
    
    if (auth_header) {
        request->authorization = BUFFER_STRDUP(auth_header);
    }
    
    /* Parse query string from path */
    char* path_copy = BUFFER_STRDUP(path);
    char* query_start = strchr(path_copy, '?');
    if (query_start) {
        *query_start = '\0';
        request->query = BUFFER_STRDUP(query_start + 1);
        /* Update path without query */
        BUFFER_FREE((void*)request->path);
        request->path = BUFFER_STRDUP(path_copy);
    }
    
    BUFFER_FREE(path_copy);
    return request;
}

void destroy_mock_request(http_request_t* request) {
    if (!request) return;
    
    if (request->path) BUFFER_FREE((void*)request->path);
    if (request->body) BUFFER_FREE((void*)request->body);
    if (request->content_type) BUFFER_FREE((void*)request->content_type);
    if (request->authorization) BUFFER_FREE((void*)request->authorization);
    if (request->query) BUFFER_FREE((void*)request->query);
    
    
    BUFFER_FREE(request);
}

/* Mock API context creation */
api_context_t* create_mock_api_context(mock_auth_context_t* auth) {
    api_context_t* ctx = (api_context_t*)BUFFER_ALLOC(sizeof(api_context_t));
    if (!ctx) return NULL;
    
    memset(ctx, 0, sizeof(api_context_t));
    
    /* Create mock database */
    ctx->db = (database_t*)BUFFER_ALLOC(sizeof(database_t));
    if (!ctx->db) {
        BUFFER_FREE(ctx);
        return NULL;
    }
    memset(ctx->db, 0, sizeof(database_t));
    
    /* Create mock RBAC system */
    ctx->rbac = (rbac_system_t*)BUFFER_ALLOC(sizeof(rbac_system_t));
    if (!ctx->rbac) {
        BUFFER_FREE(ctx->db);
        BUFFER_FREE(ctx);
        return NULL;
    }
    memset(ctx->rbac, 0, sizeof(rbac_system_t));
    ctx->rbac->db = ctx->db;
    
    /* Set up authentication context if provided */
    if (auth) {
        /* Store authentication state in a way the handlers can access */
        /* This would typically be stored in the request or session */
        /* For testing, we'll set up the JWT secret to match */
        if (auth->token) {
            ctx->jwt_secret = BUFFER_STRDUP("test-jwt-secret");
        }
    }
    
    return ctx;
}

void destroy_mock_api_context(api_context_t* ctx) {
    if (!ctx) return;
    
    if (ctx->db) BUFFER_FREE(ctx->db);
    if (ctx->rbac) BUFFER_FREE(ctx->rbac);
    BUFFER_FREE(ctx);
}

/* JSON comparison */
int json_values_equal(json_value_t* v1, json_value_t* v2) {
    if (!v1 && !v2) return 1;
    if (!v1 || !v2) return 0;
    if (v1->type != v2->type) return 0;
    
    switch (v1->type) {
        case JSON_NULL:
            return 1;
            
        case JSON_BOOLEAN:
            return v1->value.boolean == v2->value.boolean;
            
        case JSON_NUMBER:
            return v1->value.number == v2->value.number;
            
        case JSON_INTEGER:
            return v1->value.integer == v2->value.integer;
            
        case JSON_STRING:
            return strcmp(v1->value.string, v2->value.string) == 0;
            
        case JSON_ARRAY:
            if (json_array_size(v1) != json_array_size(v2)) return 0;
            for (size_t i = 0; i < json_array_size(v1); i++) {
                if (!json_values_equal(json_array_get(v1, i), json_array_get(v2, i))) {
                    return 0;
                }
            }
            return 1;
            
        case JSON_OBJECT:
            if (json_object_size(v1) != json_object_size(v2)) return 0;
            json_value_t* keys = json_object_get_keys(v1);
            if (!keys) return 0;
            
            for (size_t i = 0; i < json_array_size(keys); i++) {
                json_value_t* key = json_array_get(keys, i);
                const char* key_str = json_get_string(key);
                
                json_value_t* val1 = json_object_get(v1, key_str);
                json_value_t* val2 = json_object_get(v2, key_str);
                
                if (!json_values_equal(val1, val2)) {
                    /* CHECKPOINT: json_free(keys); */
                    return 0;
                }
            }
            /* CHECKPOINT: json_free(keys); */
            return 1;
    }
    
    return 0;
}

/* Test data generators */
json_value_t* generate_test_document(const char* title, const char* content) {
    json_value_t* doc = json_create_object();
    if (!doc) return NULL;
    
    json_object_set(doc, "title", json_create_string(title));
    json_object_set(doc, "content", json_create_string(content));
    json_object_set(doc, "type", json_create_string("document"));
    json_object_set(doc, "library", json_create_string("default"));
    
    /* Add timestamp */
    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    json_object_set(doc, "created_at", json_create_string(timestamp));
    
    return doc;
}

json_value_t* generate_test_user(const char* username, const char* email) {
    json_value_t* user = json_create_object();
    if (!user) return NULL;
    
    json_object_set(user, "username", json_create_string(username));
    json_object_set(user, "email", json_create_string(email));
    json_object_set(user, "type", json_create_string("user"));
    json_object_set(user, "library", json_create_string("system"));
    
    return user;
}

json_value_t* generate_test_role(const char* name, json_value_t* permissions) {
    json_value_t* role = json_create_object();
    if (!role) return NULL;
    
    json_object_set(role, "name", json_create_string(name));
    json_object_set(role, "type", json_create_string("role"));
    json_object_set(role, "library", json_create_string("system"));
    
    if (permissions) {
        json_object_set(role, "permissions", permissions);
    } else {
        json_object_set(role, "permissions", json_create_array());
    }
    
    return role;
}

/* Performance testing */
void run_performance_test(perf_test_t* test, int (*test_func)(void)) {
    printf("Performance test: %s (%d iterations, target: %.2f ms)\n", 
           test->name, test->iterations, test->target_ms);
    
    test_timer_t timer;
    test_timer_start(&timer);
    
    for (int i = 0; i < test->iterations; i++) {
        int result = test_func();
        if (result != 0) {
            printf("  FAIL: Test failed on iteration %d\n", i + 1);
            test->passed = 0;
            return;
        }
    }
    
    test_timer_stop(&timer);
    test->actual_ms = timer.elapsed_ms;
    
    double avg_ms = test->actual_ms / test->iterations;
    test->passed = (avg_ms <= test->target_ms);
    
    printf("  Result: %s (avg: %.2f ms, total: %.2f ms)\n",
           test->passed ? "PASS" : "FAIL", avg_ms, test->actual_ms);
}