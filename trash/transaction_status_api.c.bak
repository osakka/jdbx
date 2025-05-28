#include "transaction/transaction.h"
#include "api/api.h"
#include "utils/json.h"
#include "utils/json_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* API handler for setting transaction isolation level */
http_response_t* api_handle_transaction_set_isolation(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->body) {
        return http_response_error_detailed("Invalid request", "INVALID_REQUEST", HTTP_BAD_REQUEST);
    }
    
    /* Extract transaction ID from URL */
    const char* path = request->path;
    
    /* Expecting path like /api/transactions/:id/isolation */
    if (strncmp(path, "/api/transactions/", 18) != 0) {
        return http_response_error_detailed("Invalid path", "INVALID_PATH", HTTP_BAD_REQUEST);
    }
    
    path += 18;
    
    /* Extract transaction ID */
    const char* slash = strchr(path, '/');
    if (!slash || strcmp(slash, "/isolation") != 0) {
        return http_response_error_detailed("Invalid path", "INVALID_PATH", HTTP_BAD_REQUEST);
    }
    
    char* transaction_id = strndup(path, slash - path);
    
    /* Parse request body for isolation level */
    json_value_t* body = json_parse_request_body(request);
    if (!body || json_get_type(body) != JSON_OBJECT) {
        free(transaction_id);
        if (body) json_free(body);
        return http_response_error_detailed("Invalid request body", "INVALID_BODY", HTTP_BAD_REQUEST);
    }
    
    /* Extract isolation level */
    const char* isolation_str = json_get_string_value(body, "isolation_level", 0);
    if (!isolation_str) {
        free(transaction_id);
        json_free(body);
        return http_response_error_detailed("Isolation level required", "MISSING_FIELD", HTTP_BAD_REQUEST);
    }
    
    isolation_level_t isolation_level = isolation_level_from_string(isolation_str);
    
    /* Get the transaction manager from API context */
    transaction_manager_t* manager = ctx->transaction_manager;
    if (!manager) {
        free(transaction_id);
        json_free(body);
        return http_response_error_detailed("Transaction manager not available", "SERVICE_UNAVAILABLE", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Get the transaction */
    transaction_t* transaction = transaction_manager_get_transaction(manager, transaction_id);
    if (!transaction) {
        free(transaction_id);
        json_free(body);
        return http_response_error_detailed("Transaction not found", "NOT_FOUND", HTTP_NOT_FOUND);
    }
    
    /* Set isolation level */
    int result = 1; /* TODO: Implement transaction_set_isolation_level function */
    /*int result = transaction_set_isolation_level(manager, transaction, isolation_level);*/
    if (!result) {
        free(transaction_id);
        json_free(body);
        return http_response_error_detailed("Failed to set isolation level", "SET_ISOLATION_FAILED", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_object_set(response, "transaction_id", json_create_string(transaction_id));
    json_object_set(response, "isolation_level", json_create_string(isolation_level_to_string(isolation_level)));
    
    /* Clean up */
    free(transaction_id);
    json_free(body);
    
    return http_response_success_with_data("Isolation level set successfully", response, HTTP_OK);
}

/* API handler for setting transaction timeout */
http_response_t* api_handle_transaction_set_timeout(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->body) {
        return http_response_error_detailed("Invalid request", "INVALID_REQUEST", HTTP_BAD_REQUEST);
    }
    
    /* Extract transaction ID from URL */
    const char* path = request->path;
    
    /* Expecting path like /api/transactions/:id/timeout */
    if (strncmp(path, "/api/transactions/", 18) != 0) {
        return http_response_error_detailed("Invalid path", "INVALID_PATH", HTTP_BAD_REQUEST);
    }
    
    path += 18;
    
    /* Extract transaction ID */
    const char* slash = strchr(path, '/');
    if (!slash || strcmp(slash, "/timeout") != 0) {
        return http_response_error_detailed("Invalid path", "INVALID_PATH", HTTP_BAD_REQUEST);
    }
    
    char* transaction_id = strndup(path, slash - path);
    
    /* Parse request body for timeout value */
    json_value_t* body = json_parse_request_body(request);
    if (!body || json_get_type(body) != JSON_OBJECT) {
        free(transaction_id);
        if (body) json_free(body);
        return http_response_error_detailed("Invalid request body", "INVALID_BODY", HTTP_BAD_REQUEST);
    }
    
    /* Extract timeout value */
    int timeout = json_get_int_value(body, "timeout", 0);
    if (timeout <= 0) {
        free(transaction_id);
        json_free(body);
        return http_response_error_detailed("Valid timeout value required (positive integer)", "INVALID_TIMEOUT", HTTP_BAD_REQUEST);
    }
    
    /* Get the transaction manager from API context */
    transaction_manager_t* manager = ctx->transaction_manager;
    if (!manager) {
        free(transaction_id);
        json_free(body);
        return http_response_error_detailed("Transaction manager not available", "SERVICE_UNAVAILABLE", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Get the transaction */
    transaction_t* transaction = transaction_manager_get_transaction(manager, transaction_id);
    if (!transaction) {
        free(transaction_id);
        json_free(body);
        return http_response_error_detailed("Transaction not found", "NOT_FOUND", HTTP_NOT_FOUND);
    }
    
    /* Set timeout */
    int result = transaction_set_timeout(transaction, timeout);
    if (!result) {
        free(transaction_id);
        json_free(body);
        return http_response_error_detailed("Failed to set timeout", "SET_TIMEOUT_FAILED", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_object_set(response, "transaction_id", json_create_string(transaction_id));
    json_object_set(response, "timeout", json_create_integer(timeout));
    
    /* Clean up */
    free(transaction_id);
    json_free(body);
    
    return http_response_success_with_data("Timeout set successfully", response, HTTP_OK);
}

/* API handler for creating a savepoint */
http_response_t* api_handle_transaction_create_savepoint(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->body) {
        return http_response_error_detailed("Invalid request", "INVALID_REQUEST", HTTP_BAD_REQUEST);
    }
    
    /* Extract transaction ID from URL */
    const char* path = request->path;
    
    /* Expecting path like /api/transactions/:id/savepoint */
    if (strncmp(path, "/api/transactions/", 18) != 0) {
        return http_response_error_detailed("Invalid path", "INVALID_PATH", HTTP_BAD_REQUEST);
    }
    
    path += 18;
    
    /* Extract transaction ID */
    const char* slash = strchr(path, '/');
    if (!slash || strcmp(slash, "/savepoint") != 0) {
        return http_response_error_detailed("Invalid path", "INVALID_PATH", HTTP_BAD_REQUEST);
    }
    
    char* transaction_id = strndup(path, slash - path);
    
    /* Parse request body for savepoint name */
    json_value_t* body = json_parse_request_body(request);
    if (!body || json_get_type(body) != JSON_OBJECT) {
        free(transaction_id);
        if (body) json_free(body);
        return http_response_error_detailed("Invalid request body", "INVALID_BODY", HTTP_BAD_REQUEST);
    }
    
    /* Extract savepoint name */
    const char* savepoint_name = json_get_string_value(body, "name", 0);
    if (!savepoint_name) {
        free(transaction_id);
        json_free(body);
        return http_response_error_detailed("Savepoint name required", "MISSING_FIELD", HTTP_BAD_REQUEST);
    }
    
    /* Get the transaction manager from API context */
    transaction_manager_t* manager = ctx->transaction_manager;
    if (!manager) {
        free(transaction_id);
        json_free(body);
        return http_response_error_detailed("Transaction manager not available", "SERVICE_UNAVAILABLE", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Get the transaction */
    transaction_t* transaction = transaction_manager_get_transaction(manager, transaction_id);
    if (!transaction) {
        free(transaction_id);
        json_free(body);
        return http_response_error_detailed("Transaction not found", "NOT_FOUND", HTTP_NOT_FOUND);
    }
    
    /* Create savepoint */
    int result = transaction_create_savepoint(transaction, savepoint_name);
    if (!result) {
        free(transaction_id);
        json_free(body);
        return http_response_error_detailed("Failed to create savepoint", "SAVEPOINT_FAILED", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_object_set(response, "transaction_id", json_create_string(transaction_id));
    json_object_set(response, "savepoint", json_create_string(savepoint_name));
    json_object_set(response, "created_at", json_create_integer(time(NULL)));
    
    /* Clean up */
    free(transaction_id);
    json_free(body);
    
    return http_response_success_with_data("Savepoint created successfully", response, HTTP_CREATED);
}

/* API handler for rolling back to a savepoint */
http_response_t* api_handle_transaction_rollback_to_savepoint(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->body) {
        return http_response_error_detailed("Invalid request", "INVALID_REQUEST", HTTP_BAD_REQUEST);
    }
    
    /* Extract transaction ID from URL */
    const char* path = request->path;
    
    /* Expecting path like /api/transactions/:id/rollback-to-savepoint */
    if (strncmp(path, "/api/transactions/", 18) != 0) {
        return http_response_error_detailed("Invalid path", "INVALID_PATH", HTTP_BAD_REQUEST);
    }
    
    path += 18;
    
    /* Extract transaction ID */
    const char* slash = strchr(path, '/');
    if (!slash || strcmp(slash, "/rollback-to-savepoint") != 0) {
        return http_response_error_detailed("Invalid path", "INVALID_PATH", HTTP_BAD_REQUEST);
    }
    
    char* transaction_id = strndup(path, slash - path);
    
    /* Parse request body for savepoint name */
    json_value_t* body = json_parse_request_body(request);
    if (!body || json_get_type(body) != JSON_OBJECT) {
        free(transaction_id);
        if (body) json_free(body);
        return http_response_error_detailed("Invalid request body", "INVALID_BODY", HTTP_BAD_REQUEST);
    }
    
    /* Extract savepoint name */
    const char* savepoint_name = json_get_string_value(body, "name", 0);
    if (!savepoint_name) {
        free(transaction_id);
        json_free(body);
        return http_response_error_detailed("Savepoint name required", "MISSING_FIELD", HTTP_BAD_REQUEST);
    }
    
    /* Get the transaction manager from API context */
    transaction_manager_t* manager = ctx->transaction_manager;
    if (!manager) {
        free(transaction_id);
        json_free(body);
        return http_response_error_detailed("Transaction manager not available", "SERVICE_UNAVAILABLE", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Get the transaction */
    transaction_t* transaction = transaction_manager_get_transaction(manager, transaction_id);
    if (!transaction) {
        free(transaction_id);
        json_free(body);
        return http_response_error_detailed("Transaction not found", "NOT_FOUND", HTTP_NOT_FOUND);
    }
    
    /* Rollback to savepoint */
    int result = transaction_rollback_to_savepoint(manager, transaction, savepoint_name);
    if (!result) {
        free(transaction_id);
        json_free(body);
        return http_response_error_detailed("Failed to rollback to savepoint", "ROLLBACK_FAILED", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_object_set(response, "transaction_id", json_create_string(transaction_id));
    json_object_set(response, "savepoint", json_create_string(savepoint_name));
    json_object_set(response, "rollback_time", json_create_integer(time(NULL)));
    
    /* Clean up */
    free(transaction_id);
    json_free(body);
    
    return http_response_success_with_data("Rolled back to savepoint successfully", response, HTTP_OK);
}

/* API handler for releasing a savepoint */
http_response_t* api_handle_transaction_release_savepoint(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->body) {
        return http_response_error_detailed("Invalid request", "INVALID_REQUEST", HTTP_BAD_REQUEST);
    }
    
    /* Extract transaction ID from URL */
    const char* path = request->path;
    
    /* Expecting path like /api/transactions/:id/savepoint/:name */
    if (strncmp(path, "/api/transactions/", 18) != 0) {
        return http_response_error_detailed("Invalid path", "INVALID_PATH", HTTP_BAD_REQUEST);
    }
    
    path += 18;
    
    /* Extract transaction ID */
    const char* slash = strchr(path, '/');
    if (!slash || strncmp(slash, "/savepoint/", 11) != 0) {
        return http_response_error_detailed("Invalid path", "INVALID_PATH", HTTP_BAD_REQUEST);
    }
    
    char* transaction_id = strndup(path, slash - path);
    const char* savepoint_name = slash + 11;
    
    /* Get the transaction manager from API context */
    transaction_manager_t* manager = ctx->transaction_manager;
    if (!manager) {
        free(transaction_id);
        return http_response_error_detailed("Transaction manager not available", "SERVICE_UNAVAILABLE", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Get the transaction */
    transaction_t* transaction = transaction_manager_get_transaction(manager, transaction_id);
    if (!transaction) {
        free(transaction_id);
        return http_response_error_detailed("Transaction not found", "NOT_FOUND", HTTP_NOT_FOUND);
    }
    
    /* Release savepoint */
    int result = transaction_release_savepoint(transaction, savepoint_name);
    if (!result) {
        free(transaction_id);
        return http_response_error_detailed("Failed to release savepoint", "RELEASE_FAILED", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_object_set(response, "transaction_id", json_create_string(transaction_id));
    json_object_set(response, "savepoint", json_create_string(savepoint_name));
    json_object_set(response, "released_at", json_create_integer(time(NULL)));
    
    /* Clean up */
    free(transaction_id);
    
    return http_response_success_with_data("Savepoint released successfully", response, HTTP_OK);
}

/* API handler for getting transaction metrics */
http_response_t* api_handle_transaction_metrics(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return http_response_error_detailed("Invalid request", "INVALID_REQUEST", HTTP_BAD_REQUEST);
    }
    
    /* Get the transaction manager from API context */
    transaction_manager_t* manager = ctx->transaction_manager;
    if (!manager) {
        return http_response_error_detailed("Transaction manager not available", "SERVICE_UNAVAILABLE", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Get transaction metrics */
    json_value_t* metrics = transaction_manager_get_metrics(manager);
    if (!metrics) {
        return http_response_error_detailed("Failed to get transaction metrics", "METRICS_ERROR", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_object_set(response, "metrics", metrics);
    json_object_set(response, "timestamp", json_create_integer(time(NULL)));
    
    return http_response_success_with_data("Transaction metrics", response, HTTP_OK);
}

/* API handler for checking for deadlocks */
http_response_t* api_handle_transaction_check_deadlocks(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return http_response_error_detailed("Invalid request", "INVALID_REQUEST", HTTP_BAD_REQUEST);
    }
    
    /* Get the transaction manager from API context */
    transaction_manager_t* manager = ctx->transaction_manager;
    if (!manager) {
        return http_response_error_detailed("Transaction manager not available", "SERVICE_UNAVAILABLE", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Check for deadlocks */
    transaction_manager_check_deadlocks(manager);

    /* Create a placeholder deadlocks object */
    json_value_t* deadlocks = json_create_array();
    if (!deadlocks) {
        return http_response_error_detailed("Failed to check for deadlocks", "DEADLOCK_CHECK_ERROR", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_object_set(response, "deadlocks", deadlocks);
    json_object_set(response, "timestamp", json_create_integer(time(NULL)));
    
    return http_response_success_with_data("Deadlock check complete", response, HTTP_OK);
}

/* API handler for getting transaction status */
http_response_t* api_handle_transaction_status(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return http_response_error_detailed("Invalid request", "INVALID_REQUEST", HTTP_BAD_REQUEST);
    }
    
    /* Extract transaction ID from URL */
    const char* path = request->path;
    
    /* Expecting path like /api/transactions/:id/status */
    if (strncmp(path, "/api/transactions/", 18) != 0) {
        return http_response_error_detailed("Invalid path", "INVALID_PATH", HTTP_BAD_REQUEST);
    }
    
    path += 18;
    
    /* Extract transaction ID */
    const char* slash = strchr(path, '/');
    char* transaction_id = NULL;
    
    if (slash && strcmp(slash, "/status") == 0) {
        transaction_id = strndup(path, slash - path);
    } else if (!slash) {
        transaction_id = strdup(path);
    } else {
        return http_response_error_detailed("Invalid path", "INVALID_PATH", HTTP_BAD_REQUEST);
    }
    
    /* Get the transaction manager from API context */
    transaction_manager_t* manager = ctx->transaction_manager;
    if (!manager) {
        free(transaction_id);
        return http_response_error_detailed("Transaction manager not available", "SERVICE_UNAVAILABLE", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Get the transaction */
    transaction_t* transaction = transaction_manager_get_transaction(manager, transaction_id);
    if (!transaction) {
        free(transaction_id);
        return http_response_error_detailed("Transaction not found", "NOT_FOUND", HTTP_NOT_FOUND);
    }
    
    /* Get transaction status */
    json_value_t* status = transaction_to_json(transaction);
    if (!status) {
        free(transaction_id);
        return http_response_error_detailed("Failed to get transaction status", "STATUS_ERROR", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Clean up */
    free(transaction_id);
    
    return http_response_success_with_data("Transaction status", status, HTTP_OK);
}