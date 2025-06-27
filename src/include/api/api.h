#ifndef API_H
#define API_H

#include "core/server.h"
#include "database/database.h"
#include "rbac/rbac.h"
#include "rbac/jwt.h"
#include "transaction/transaction.h"
#include "utils/memory_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* API context forward declaration */
typedef struct api_context api_context_t;

/* API endpoint handler */
typedef http_response_t* (*api_handler_t)(api_context_t* ctx, http_request_t* request);

/* API route */
typedef struct api_route {
    const char* path;
    http_method_t method;
    api_handler_t handler;
    int requires_auth;
} api_route_t;

/* API context */
typedef struct api_context {
    database_t* db;
    rbac_system_t* rbac;
    const char* jwt_secret;
    transaction_manager_t* transaction_manager;
    api_route_t* routes;     /* Dynamically allocated array of API routes */
    int num_routes;          /* Number of routes */
    int max_routes;          /* Maximum number of routes */
} api_context_t;

/* API result structure for explicit checkpoint lifecycle management */
typedef struct api_result {
    http_response_t* response;
    memory_checkpoint_t* checkpoint;
} api_result_t;

/* API function prototypes */
api_context_t* api_create_context(database_t* db, rbac_system_t* rbac, const char* jwt_secret);
void api_free_context(api_context_t* ctx);
api_result_t* api_dispatch_request(api_context_t* ctx, http_request_t* request);
void api_result_free(api_result_t* result);  /* Clean API result and commit/rewind checkpoint */
int api_authenticate_request(api_context_t* ctx, http_request_t* request);
int api_authenticate_request_sliding(api_context_t* ctx, http_request_t* request);
char* api_extract_token(http_request_t* request);
int init_shared_api_context(void); /* Initialize shared memory for API context */

/* API handler prototypes */

/* Authentication handlers */
http_response_t* api_handle_login(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_register(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_token_refresh(api_context_t* ctx, http_request_t* request);

/* Database handlers */
http_response_t* api_handle_collections_list(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_collection_create(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_collection_drop(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_documents_query(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_document_get(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_document_create(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_document_update(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_document_delete(api_context_t* ctx, http_request_t* request);

/* RBAC handlers */
http_response_t* api_handle_users_list(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_user_get(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_user_create(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_user_update(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_user_delete(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_roles_list(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_role_get(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_role_create(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_role_update(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_role_delete(api_context_t* ctx, http_request_t* request);

/* Configuration handlers */
http_response_t* api_handle_config_get(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_config_update(api_context_t* ctx, http_request_t* request);

/* Metrics handlers */
http_response_t* api_handle_metrics_get(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_metrics_export(api_context_t* ctx, http_request_t* request);

/* System information handlers */
http_response_t* api_handle_system_info(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_log_control(api_context_t* ctx, http_request_t* request);

/* OpenAPI specification handler */
http_response_t* api_handle_openapi_spec(api_context_t* ctx, http_request_t* request);

/* Data visualization handlers */
http_response_t* api_handle_visualization_collection_stats(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_visualization_document_types(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_visualization_field_distribution(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_visualization_transaction_history(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_visualization_transaction_metrics(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_visualization_transaction_relationships(api_context_t* ctx, http_request_t* request);

/* Backup and restore handlers */
http_response_t* api_handle_backup_create(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_backup_list(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_backup_restore(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_backup_delete(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_export(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_import(api_context_t* ctx, http_request_t* request);

/* Admin auth handlers */
http_response_t* api_handle_admin_login(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_admin_test(api_context_t* ctx, http_request_t* request);

/* Schema validation handlers */
http_response_t* api_handle_schema_get(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_schema_validate(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_schema_create(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_schema_update(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_schema_delete(api_context_t* ctx, http_request_t* request);

/* Index handlers */
http_response_t* api_handle_index_list(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_index_create(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_index_get(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_index_delete(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_index_rebuild(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_index_stats(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_index_aggregate_stats(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_index_query(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_index_compound_query(api_context_t* ctx, http_request_t* request);

/* JavaScript handlers */
http_response_t* api_handle_js_query(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_js_eval(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_js_function_register(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_js_function_execute(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_js_validator_register(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_js_transformer_register(api_context_t* ctx, http_request_t* request);

/* Cache handlers */
http_response_t* api_handle_cache_stats(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_cache_configure(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_cache_clear(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_cache_invalidate(api_context_t* ctx, http_request_t* request);

/* Transaction handlers */
http_response_t* api_handle_transaction_begin(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_transaction_commit(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_transaction_rollback(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_transaction_document_operation(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_transaction_metrics(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_transaction_check_deadlocks(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_transaction_status(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_transaction_logs(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_transaction_logs_configure(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_transaction_logs_archive(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_transaction_logs_report(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_transaction_logs_document_history(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_transaction_set_isolation(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_transaction_set_timeout(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_transaction_create_savepoint(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_transaction_release_savepoint(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_transaction_rollback_to_savepoint(api_context_t* ctx, http_request_t* request);

/* Health and monitoring handlers */
http_response_t* api_handle_health_check(api_context_t* ctx, http_request_t* request);

/* Health API metrics handlers */
http_response_t* health_api_handle_metrics(api_context_t* ctx, http_request_t* request);
http_response_t* health_api_handle_metrics_available(api_context_t* ctx, http_request_t* request);
http_response_t* health_api_handle_metrics_export(api_context_t* ctx, http_request_t* request);

/* Metrics history handlers */
http_response_t* api_handle_metrics_history(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_metrics_aggregate(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_adaptive_indexing_metrics(api_context_t* ctx, http_request_t* request);

/* Health API initialization and endpoint registration */
void health_api_init(void);
void register_health_api_endpoints(api_context_t *ctx);

/* JavaScript engine initialization and cleanup */
void js_api_init(database_t* db);
void js_api_cleanup();

/* Index cleanup API initialization */
struct index_metrics_collector;
void index_cleanup_api_init(database_t* db, struct index_metrics_collector* metrics);

#endif /* API_H */