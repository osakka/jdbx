/**
 * @file test_api_documents.c
 * @brief Unit tests for API documents module
 */

#include "test_framework.h"
#include "api/api_documents.h"
#include "database/document_storage.h"
#include "utils/memory_manager.h"
#include "utils/logger.h"
#include "core/server.h"
#include <unistd.h>

/* External globals */
extern logger_config_t* g_logger;

/* Mock database for testing */
typedef struct {
    json_value_t* documents;  /* Array of stored documents */
    int next_id;
} mock_database_t;

static mock_database_t* g_mock_db = NULL;

/* Initialize mock database */
static void init_mock_database() {
    g_mock_db = (mock_database_t*)BUFFER_ALLOC(sizeof(mock_database_t));
    g_mock_db->documents = json_create_array();
    g_mock_db->next_id = 1;
}

/* Clean up mock database */
static void cleanup_mock_database() {
    if (g_mock_db) {
        /* CHECKPOINT: json_free(g_mock_db->documents); */
        BUFFER_FREE(g_mock_db);
        g_mock_db = NULL;
    }
}

/* Mock storage functions */
json_value_t* storage_insert_document(database_t* db, json_value_t* document) {
    if (!g_mock_db || !document) return NULL;
    
    /* Generate ID if not present */
    if (!json_object_get(document, "uuid")) {
        char id[32];
        snprintf(id, sizeof(id), "doc-%d", g_mock_db->next_id++);
        json_object_set(document, "uuid", json_create_string(id));
    }
    
    /* Add timestamp */
    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    json_object_set(document, "created_at", json_create_string(timestamp));
    
    /* Store document */
    json_value_t* doc_copy = json_deep_copy(document);
    json_array_append(g_mock_db->documents, doc_copy);
    
    /* Return the document with ID */
    return json_deep_copy(doc_copy);
}

json_value_t* storage_get_document(database_t* db, const char* uuid) {
    if (!g_mock_db || !uuid) return NULL;
    
    for (size_t i = 0; i < json_array_size(g_mock_db->documents); i++) {
        json_value_t* doc = json_array_get(g_mock_db->documents, i);
        json_value_t* id = json_object_get(doc, "uuid");
        if (id && id->type == JSON_STRING && strcmp(id->value.string, uuid) == 0) {
            return json_deep_copy(doc);
        }
    }
    
    return NULL;
}

json_value_t* storage_update_document(database_t* db, const char* uuid, json_value_t* update) {
    if (!g_mock_db || !uuid || !update) return NULL;
    
    for (size_t i = 0; i < json_array_size(g_mock_db->documents); i++) {
        json_value_t* doc = json_array_get(g_mock_db->documents, i);
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
            
            /* Update timestamp */
            time_t now = time(NULL);
            char timestamp[32];
            strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
            json_object_set(doc, "modified_at", json_create_string(timestamp));
            
            return json_deep_copy(doc);
        }
    }
    
    return NULL;
}

int storage_delete_document(database_t* db, const char* uuid) {
    if (!g_mock_db || !uuid) return 0;
    
    for (size_t i = 0; i < json_array_size(g_mock_db->documents); i++) {
        json_value_t* doc = json_array_get(g_mock_db->documents, i);
        json_value_t* id = json_object_get(doc, "uuid");
        if (id && id->type == JSON_STRING && strcmp(id->value.string, uuid) == 0) {
            json_array_remove(g_mock_db->documents, i);
            return 1;
        }
    }
    
    return 0;
}

json_value_t* storage_query_documents(database_t* db, json_value_t* query) {
    if (!g_mock_db) return NULL;
    
    json_value_t* result = json_create_object();
    json_value_t* docs = json_create_array();
    
    /* Simple query implementation - match all fields in query */
    for (size_t i = 0; i < json_array_size(g_mock_db->documents); i++) {
        json_value_t* doc = json_array_get(g_mock_db->documents, i);
        int matches = 1;
        
        if (query && query->type == JSON_OBJECT) {
            const char* key;
            json_value_t* val;
            json_object_foreach(query, key, val) {
                json_value_t* doc_val = json_object_get(doc, key);
                if (!json_values_equal(val, doc_val)) {
                    matches = 0;
                    break;
                }
            }
        }
        
        if (matches) {
            json_array_append(docs, json_deep_copy(doc));
        }
    }
    
    json_object_set(result, "documents", docs);
    json_object_set(result, "count", json_create_integer(json_array_size(docs)));
    
    return result;
}

/* Test: Document creation with flat structure */
static int test_document_create_flat() {
    init_mock_database();
    
    api_context_t* ctx = create_mock_api_context(NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    
    /* Create flat document */
    json_value_t* doc = generate_test_document("Test Document", "This is a test");
    char* body = json_stringify(doc);
    
    http_request_t* request = create_mock_request(HTTP_POST, "/api/documents", body, NULL);
    TEST_ASSERT_NOT_NULL(request);
    
    /* Call handler */
    http_response_t* response = api_handle_unified_documents_create(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_CREATED, response->status);
    
    /* Verify response */
    json_value_t* result = json_parse(response->body);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_NOT_NULL(json_object_get(result, "uuid"));
    TEST_ASSERT_NOT_NULL(json_object_get(result, "created_at"));
    
    /* Cleanup */
    /* CHECKPOINT: json_free(result); */
    /* CHECKPOINT: json_free(doc); */
    BUFFER_FREE(body);
    free_http_response(response);
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    cleanup_mock_database();
    
    return 0;
}

/* Test: Document creation with nested structure */
static int test_document_create_nested() {
    init_mock_database();
    
    api_context_t* ctx = create_mock_api_context(NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    
    /* Create nested structure */
    json_value_t* wrapper = json_create_object();
    json_value_t* doc = generate_test_document("Nested Doc", "Nested content");
    json_object_set(wrapper, "library", json_create_string("test"));
    json_object_set(wrapper, "collection", json_create_string("documents"));
    json_object_set(wrapper, "document", doc);
    
    char* body = json_stringify(wrapper);
    
    http_request_t* request = create_mock_request(HTTP_POST, "/api/documents", body, NULL);
    TEST_ASSERT_NOT_NULL(request);
    
    /* Call handler */
    http_response_t* response = api_handle_unified_documents_create(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_CREATED, response->status);
    
    /* Verify response */
    json_value_t* result = json_parse(response->body);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_NOT_NULL(json_object_get(result, "uuid"));
    
    /* Cleanup */
    /* CHECKPOINT: json_free(result); */
    /* CHECKPOINT: json_free(wrapper); */
    BUFFER_FREE(body);
    free_http_response(response);
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    cleanup_mock_database();
    
    return 0;
}

/* Test: Document auto-population of fields */
static int test_document_auto_populate() {
    init_mock_database();
    
    api_context_t* ctx = create_mock_api_context(NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    
    /* Create document without type and owner */
    json_value_t* doc = json_create_object();
    json_object_set(doc, "title", json_create_string("Auto-populate test"));
    
    char* body = json_stringify(doc);
    
    http_request_t* request = create_mock_request(HTTP_POST, "/api/documents", body, NULL);
    TEST_ASSERT_NOT_NULL(request);
    
    /* Call handler */
    http_response_t* response = api_handle_unified_documents_create(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_CREATED, response->status);
    
    /* Verify auto-populated fields */
    json_value_t* result = json_parse(response->body);
    TEST_ASSERT_NOT_NULL(result);
    
    json_value_t* type = json_object_get(result, "type");
    TEST_ASSERT_NOT_NULL(type);
    TEST_ASSERT_STR_EQ("document", json_get_string(type));
    
    json_value_t* owner = json_object_get(result, "owner");
    TEST_ASSERT_NOT_NULL(owner);
    TEST_ASSERT_STR_EQ("user", json_get_string(owner));
    
    /* Cleanup */
    /* CHECKPOINT: json_free(result); */
    /* CHECKPOINT: json_free(doc); */
    BUFFER_FREE(body);
    free_http_response(response);
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    cleanup_mock_database();
    
    return 0;
}

/* Test: Document retrieval */
static int test_document_get() {
    init_mock_database();
    
    api_context_t* ctx = create_mock_api_context(NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    
    /* Insert a document first */
    json_value_t* doc = generate_test_document("Get Test", "Content to retrieve");
    json_value_t* inserted = storage_insert_document(ctx->db, doc);
    TEST_ASSERT_NOT_NULL(inserted);
    
    json_value_t* id_val = json_object_get(inserted, "uuid");
    TEST_ASSERT_NOT_NULL(id_val);
    const char* doc_id = json_get_string(id_val);
    
    /* Create request with ID in path */
    char path[256];
    snprintf(path, sizeof(path), "/api/documents/%s", doc_id);
    
    http_request_t* request = create_mock_request(HTTP_GET, path, NULL, NULL);
    TEST_ASSERT_NOT_NULL(request);
    
    /* Call handler */
    http_response_t* response = api_handle_unified_document_get(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_OK, response->status);
    
    /* Verify response */
    json_value_t* result = json_parse(response->body);
    TEST_ASSERT_NOT_NULL(result);
    
    json_value_t* title = json_object_get(result, "title");
    TEST_ASSERT_NOT_NULL(title);
    TEST_ASSERT_STR_EQ("Get Test", json_get_string(title));
    
    /* Cleanup */
    /* CHECKPOINT: json_free(result); */
    /* CHECKPOINT: json_free(inserted); */
    /* CHECKPOINT: json_free(doc); */
    free_http_response(response);
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    cleanup_mock_database();
    
    return 0;
}

/* Test: Document not found */
static int test_document_get_not_found() {
    init_mock_database();
    
    api_context_t* ctx = create_mock_api_context(NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    
    http_request_t* request = create_mock_request(HTTP_GET, "/api/documents/nonexistent", NULL, NULL);
    TEST_ASSERT_NOT_NULL(request);
    
    /* Call handler */
    http_response_t* response = api_handle_unified_document_get(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_NOT_FOUND, response->status);
    
    /* Cleanup */
    free_http_response(response);
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    cleanup_mock_database();
    
    return 0;
}

/* Test: Document update */
static int test_document_update() {
    init_mock_database();
    
    api_context_t* ctx = create_mock_api_context(NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    
    /* Insert a document first */
    json_value_t* doc = generate_test_document("Update Test", "Original content");
    json_value_t* inserted = storage_insert_document(ctx->db, doc);
    TEST_ASSERT_NOT_NULL(inserted);
    
    json_value_t* id_val = json_object_get(inserted, "uuid");
    const char* doc_id = json_get_string(id_val);
    
    /* Create update */
    json_value_t* update = json_create_object();
    json_object_set(update, "title", json_create_string("Updated Title"));
    json_object_set(update, "content", json_create_string("Updated content"));
    
    char* body = json_stringify(update);
    
    /* Create request */
    char path[256];
    snprintf(path, sizeof(path), "/api/documents/%s", doc_id);
    
    http_request_t* request = create_mock_request(HTTP_PUT, path, body, NULL);
    TEST_ASSERT_NOT_NULL(request);
    
    /* Call handler */
    http_response_t* response = api_handle_unified_document_update(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_OK, response->status);
    
    /* Verify update */
    json_value_t* result = json_parse(response->body);
    TEST_ASSERT_NOT_NULL(result);
    
    json_value_t* title = json_object_get(result, "title");
    TEST_ASSERT_NOT_NULL(title);
    TEST_ASSERT_STR_EQ("Updated Title", json_get_string(title));
    
    TEST_ASSERT_NOT_NULL(json_object_get(result, "modified_at"));
    
    /* Cleanup */
    /* CHECKPOINT: json_free(result); */
    /* CHECKPOINT: json_free(update); */
    /* CHECKPOINT: json_free(inserted); */
    /* CHECKPOINT: json_free(doc); */
    BUFFER_FREE(body);
    free_http_response(response);
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    cleanup_mock_database();
    
    return 0;
}

/* Test: Document deletion */
static int test_document_delete() {
    init_mock_database();
    
    api_context_t* ctx = create_mock_api_context(NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    
    /* Insert a document first */
    json_value_t* doc = generate_test_document("Delete Test", "To be deleted");
    json_value_t* inserted = storage_insert_document(ctx->db, doc);
    TEST_ASSERT_NOT_NULL(inserted);
    
    json_value_t* id_val = json_object_get(inserted, "uuid");
    const char* doc_id = json_get_string(id_val);
    
    /* Create request */
    char path[256];
    snprintf(path, sizeof(path), "/api/documents/%s", doc_id);
    
    http_request_t* request = create_mock_request(HTTP_DELETE, path, NULL, NULL);
    TEST_ASSERT_NOT_NULL(request);
    
    /* Call handler */
    http_response_t* response = api_handle_unified_document_delete(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_OK, response->status);
    
    /* Verify deletion */
    json_value_t* check = storage_get_document(ctx->db, doc_id);
    TEST_ASSERT_NULL(check);
    
    /* Cleanup */
    /* CHECKPOINT: json_free(inserted); */
    /* CHECKPOINT: json_free(doc); */
    free_http_response(response);
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    cleanup_mock_database();
    
    return 0;
}

/* Test: Document query with filters */
static int test_document_query() {
    init_mock_database();
    
    api_context_t* ctx = create_mock_api_context(NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    
    /* Insert multiple documents */
    for (int i = 0; i < 5; i++) {
        json_value_t* doc = json_create_object();
        char title[32];
        snprintf(title, sizeof(title), "Doc %d", i);
        json_object_set(doc, "title", json_create_string(title));
        json_object_set(doc, "type", json_create_string(i % 2 == 0 ? "even" : "odd"));
        json_object_set(doc, "library", json_create_string("test"));
        storage_insert_document(ctx->db, doc);
        /* CHECKPOINT: json_free(doc); */
    }
    
    /* Query for even documents */
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string("even"));
    char* body = json_stringify(query);
    
    http_request_t* request = create_mock_request(HTTP_POST, "/api/documents/query", body, NULL);
    TEST_ASSERT_NOT_NULL(request);
    
    /* Call handler */
    http_response_t* response = api_handle_unified_documents_query(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_OK, response->status);
    
    /* Verify results */
    json_value_t* result = json_parse(response->body);
    TEST_ASSERT_NOT_NULL(result);
    
    json_value_t* docs = json_object_get(result, "documents");
    TEST_ASSERT_NOT_NULL(docs);
    TEST_ASSERT_EQ(3, json_array_size(docs));  /* 0, 2, 4 are even */
    
    /* Cleanup */
    /* CHECKPOINT: json_free(result); */
    /* CHECKPOINT: json_free(query); */
    BUFFER_FREE(body);
    free_http_response(response);
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    cleanup_mock_database();
    
    return 0;
}

/* Test: Query with URL parameters */
static int test_document_query_url_params() {
    init_mock_database();
    
    api_context_t* ctx = create_mock_api_context(NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    
    /* Insert test documents */
    json_value_t* doc1 = generate_test_document("User Doc", "User content");
    json_object_set(doc1, "type", json_create_string("user"));
    storage_insert_document(ctx->db, doc1);
    
    json_value_t* doc2 = generate_test_document("System Doc", "System content");
    json_object_set(doc2, "type", json_create_string("system"));
    storage_insert_document(ctx->db, doc2);
    
    /* Query via URL parameters */
    http_request_t* request = create_mock_request(HTTP_GET, "/api/documents?type=user", NULL, NULL);
    TEST_ASSERT_NOT_NULL(request);
    
    /* Call handler */
    http_response_t* response = api_handle_unified_documents_query(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_OK, response->status);
    
    /* Verify results */
    json_value_t* result = json_parse(response->body);
    TEST_ASSERT_NOT_NULL(result);
    
    json_value_t* docs = json_object_get(result, "documents");
    TEST_ASSERT_NOT_NULL(docs);
    TEST_ASSERT_EQ(1, json_array_size(docs));
    
    /* Cleanup */
    /* CHECKPOINT: json_free(result); */
    /* CHECKPOINT: json_free(doc1); */
    /* CHECKPOINT: json_free(doc2); */
    free_http_response(response);
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    cleanup_mock_database();
    
    return 0;
}

/* Test: Invalid request handling */
static int test_document_invalid_requests() {
    api_context_t* ctx = create_mock_api_context(NULL);
    
    /* Test: NULL context */
    http_request_t* request = create_mock_request(HTTP_POST, "/api/documents", "{}", NULL);
    http_response_t* response = api_handle_unified_documents_create(NULL, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_BAD_REQUEST, response->status);
    free_http_response(response);
    
    /* Test: NULL request */
    response = api_handle_unified_documents_create(ctx, NULL);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_BAD_REQUEST, response->status);
    free_http_response(response);
    
    /* Test: Invalid JSON */
    destroy_mock_request(request);
    request = create_mock_request(HTTP_POST, "/api/documents", "invalid json", NULL);
    response = api_handle_unified_documents_create(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_BAD_REQUEST, response->status);
    free_http_response(response);
    
    /* Test: Empty path for get */
    destroy_mock_request(request);
    request = create_mock_request(HTTP_GET, "/api/documents/", NULL, NULL);
    response = api_handle_unified_document_get(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_BAD_REQUEST, response->status);
    free_http_response(response);
    
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    
    return 0;
}

/* Test: Performance of document operations */
static int test_document_performance_helper() {
    api_context_t* ctx = create_mock_api_context(NULL);
    
    /* Create document */
    json_value_t* doc = generate_test_document("Perf Test", "Performance content");
    char* body = json_stringify(doc);
    http_request_t* request = create_mock_request(HTTP_POST, "/api/documents", body, NULL);
    
    http_response_t* response = api_handle_unified_documents_create(ctx, request);
    
    /* CHECKPOINT: json_free(doc); */
    BUFFER_FREE(body);
    free_http_response(response);
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    
    return 0;
}

static int test_document_performance() {
    init_mock_database();
    
    perf_test_t perf = {
        .name = "Document create",
        .iterations = 1000,
        .target_ms = 50.0,  /* 0.05ms per operation */
    };
    
    run_performance_test(&perf, test_document_performance_helper);
    
    cleanup_mock_database();
    
    return perf.passed ? 0 : 1;
}

/* Test suite runner */
void run_api_documents_tests() {
    RUN_TEST(test_document_create_flat);
    RUN_TEST(test_document_create_nested);
    RUN_TEST(test_document_auto_populate);
    RUN_TEST(test_document_get);
    RUN_TEST(test_document_get_not_found);
    RUN_TEST(test_document_update);
    RUN_TEST(test_document_delete);
    RUN_TEST(test_document_query);
    RUN_TEST(test_document_query_url_params);
    RUN_TEST(test_document_invalid_requests);
    RUN_TEST(test_document_performance);
}

/* Main entry point */
int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "-v") == 0) {
        g_verbose_mode = 1;
    }
    
    /* Initialize memory manager first */
    memory_manager_init();
    
    /* Initialize logger */
    logger_init("test_api_documents.log", LOG_LEVEL_ERROR);
    
    TEST_INIT();
    RUN_TEST_SUITE("API Documents Module Tests", run_api_documents_tests);
    TEST_SUMMARY();
    
    /* Cleanup */
    logger_close();
    
    return g_test_results.failed_tests > 0 ? 1 : 0;
}