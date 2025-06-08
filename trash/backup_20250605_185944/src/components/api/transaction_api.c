#include "api/api.h"
#include "transaction/transaction.h"
#include "utils/json.h"
#include "utils/json_helpers.h"
#include "core/server.h"
#include "rbac/jwt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Define ISOLATION_INVALID if not defined in transaction.h */
#ifndef ISOLATION_INVALID
#define ISOLATION_INVALID (-1)
#endif

/* Transaction error to string function prototype - implementation in transaction.c */
const char* transaction_error_to_string(int error_code);

/* Get query parameter from request */
static const char* http_request_get_param(http_request_t* request, const char* param_name) {
  if (!request || !request->query || !param_name) {
    return NULL;
  }
  
  char* query = strdup(request->query);
  if (!query) {
    return NULL;
  }
  
  char* token;
  char* rest = query;
  const char* value = NULL;
  
  while ((token = strtok_r(rest, "&", &rest))) {
    char* eq = strchr(token, '=');
    if (eq) {
      *eq = '\0';
      if (strcmp(token, param_name) == 0) {
        value = strdup(eq + 1);
        break;
      }
    }
  }
  
  free(query);
  return value;
}

/* Get path parameter from request */
static const char* http_request_get_path_param(http_request_t* request, const char* param_name) {
  /* This is a stub implementation - in a real system this would parse 
    the path template and extract parameters */
  if (!request || !request->path || !param_name) {
    return NULL;
  }
  
  /* For now, just extract transaction_id from paths like /api/transactions/{transaction_id}/ */
  if (strcmp(param_name, "transaction_id") == 0) {
    char* path = strdup(request->path);
    if (!path) {
      return NULL;
    }
    
    /* Split path by '/' */
    char* token;
    char* rest = path;
    const char* id = NULL;
    int segment = 0;
    
    while ((token = strtok_r(rest, "/", &rest))) {
      segment++;
      if (segment == 3) { /* Third segment should be the transaction ID */
        id = strdup(token);
        break;
      }
    }
    
    free(path);
    return id;
  }
  
  /* Extract collection from paths like /api/transactions/{transaction_id}/{collection}/ */
  if (strcmp(param_name, "collection") == 0) {
    char* path = strdup(request->path);
    if (!path) {
      return NULL;
    }
    
    /* Split path by '/' */
    char* token;
    char* rest = path;
    const char* collection = NULL;
    int segment = 0;
    
    while ((token = strtok_r(rest, "/", &rest))) {
      segment++;
      if (segment == 4) { /* Fourth segment should be the collection name */
        collection = strdup(token);
        break;
      }
    }
    
    free(path);
    return collection;
  }
  
  /* Extract document ID from paths like /api/transactions/{transaction_id}/{collection}/{id} */
  if (strcmp(param_name, "id") == 0) {
    char* path = strdup(request->path);
    if (!path) {
      return NULL;
    }
    
    /* Split path by '/' */
    char* token;
    char* rest = path;
    const char* doc_id = NULL;
    int segment = 0;
    
    while ((token = strtok_r(rest, "/", &rest))) {
      segment++;
      if (segment == 5) { /* Fifth segment should be the document ID */
        doc_id = strdup(token);
        break;
      }
    }
    
    free(path);
    return doc_id;
  }
  
  return NULL;
}

/* API handler for starting a new transaction */
http_response_t* api_handle_transaction_begin(api_context_t* ctx, http_request_t* request) {
  transaction_manager_t* manager = ctx->db->transaction_manager;
  
  if (!manager) {
    return http_response_error("Transaction manager not available", 400);
  }
  
  /* Parse isolation level from query parameters */
  const char* isolation_str = http_request_get_param(request, "isolation_level");
  isolation_level_t isolation = ISOLATION_READ_COMMITTED; /* Default isolation level */
  
  if (isolation_str) {
    isolation = isolation_level_from_string(isolation_str);
    if (isolation == ISOLATION_INVALID) {
      return http_response_error("Invalid isolation level", 400);
    }
  }
  
  /* Parse timeout from query parameters */
  const char* timeout_str = http_request_get_param(request, "timeout");
  int timeout_ms = 0; /* Default: no timeout */
  
  if (timeout_str) {
    timeout_ms = atoi(timeout_str);
    if (timeout_ms < 0) {
      return http_response_error("Timeout must be a non-negative integer", 400);
    }
  }
  
  /* Extract user ID from JWT token */
  const char* user_id = "system"; /* Default user ID */
  char* extracted_user_id = NULL;
  
  /* Extract token from authorization header */
  char* token = api_extract_token(request);
  if (token) {
    /* Decode the JWT token to get user information */
    jwt_token_t* decoded = jwt_decode(token);
    if (decoded && decoded->payload && decoded->payload->sub) {
      extracted_user_id = strdup(decoded->payload->sub);
      if (extracted_user_id) {
        user_id = extracted_user_id;
      }
    }
    if (decoded) {
      jwt_free(decoded);
    }
    free(token);
  }
  
  transaction_t* transaction = transaction_begin(manager, isolation, user_id);
  
  /* Clean up extracted user ID */
  if (extracted_user_id) {
    free(extracted_user_id);
  }
  if (!transaction) {
    return http_response_error("Failed to start transaction", 500);
  }
  
  /* Set timeout if specified */
  if (timeout_ms > 0) {
    transaction_set_timeout(transaction, timeout_ms);
  }
  
  /* Create JSON response with transaction ID */
  json_value_t* json = transaction_to_json(transaction);
  if (!json) {
    return http_response_error("Failed to create transaction details", 500);
  }
  
  return http_response_json(json, 200);
}

/* API handler for committing a transaction */
http_response_t* api_handle_transaction_commit(api_context_t* ctx, http_request_t* request) {
  transaction_manager_t* manager = ctx->db->transaction_manager;
  
  if (!manager) {
    return http_response_error("Transaction manager not available", 400);
  }
  
  /* Get transaction ID from path */
  const char* tx_id = http_request_get_path_param(request, "transaction_id");
  if (!tx_id) {
    return http_response_error("Transaction ID required", 400);
  }
  
  /* Look up transaction */
  transaction_t* transaction = transaction_manager_get_transaction(manager, tx_id);
  if (!transaction) {
    return http_response_error("Transaction not found", 404);
  }
  
  /* Commit transaction */
  int result = transaction_commit(manager, transaction);
  if (result != 0) {
    char error_msg[256];
    const char* error_str = transaction_error_to_string(result);
    snprintf(error_msg, sizeof(error_msg), "Failed to commit transaction: %s", error_str);
    return http_response_error(error_msg, 500);
  }
  
  /* Create success response */
  json_value_t* json = json_create_object();
  if (!json) {
    return http_response_error("Failed to create response", 500);
  }
  
  json_object_set(json, "success", json_create_boolean(1));
  json_object_set(json, "transaction_id", json_create_string(tx_id));
  json_object_set(json, "status", json_create_string("committed"));
  
  return http_response_json(json, 200);
}

/* API handler for rolling back a transaction */
http_response_t* api_handle_transaction_rollback(api_context_t* ctx, http_request_t* request) {
  transaction_manager_t* manager = ctx->db->transaction_manager;
  
  if (!manager) {
    return http_response_error("Transaction manager not available", 400);
  }
  
  /* Get transaction ID from path */
  const char* tx_id = http_request_get_path_param(request, "transaction_id");
  if (!tx_id) {
    return http_response_error("Transaction ID required", 400);
  }
  
  /* Look up transaction */
  transaction_t* transaction = transaction_manager_get_transaction(manager, tx_id);
  if (!transaction) {
    return http_response_error("Transaction not found", 404);
  }
  
  /* Rollback transaction */
  int result = transaction_rollback(manager, transaction);
  if (result != 0) {
    char error_msg[256];
    const char* error_str = transaction_error_to_string(result);
    snprintf(error_msg, sizeof(error_msg), "Failed to rollback transaction: %s", error_str);
    return http_response_error(error_msg, 500);
  }
  
  /* Create success response */
  json_value_t* json = json_create_object();
  if (!json) {
    return http_response_error("Failed to create response", 500);
  }
  
  json_object_set(json, "success", json_create_boolean(1));
  json_object_set(json, "transaction_id", json_create_string(tx_id));
  json_object_set(json, "status", json_create_string("rolled back"));
  
  return http_response_json(json, 200);
}

/* Helper for handling document operations within a transaction */
http_response_t* api_handle_transaction_document_operation(api_context_t* ctx, http_request_t* request) {
  transaction_manager_t* manager = ctx->db->transaction_manager;
  
  if (!manager) {
    return http_response_error("Transaction manager not available", 400);
  }
  
  /* Get transaction ID from path */
  const char* tx_id = http_request_get_path_param(request, "transaction_id");
  if (!tx_id) {
    return http_response_error("Transaction ID required", 400);
  }
  
  /* Get collection name from path */
  const char* collection = http_request_get_path_param(request, "collection");
  if (!collection) {
    return http_response_error("Collection name required", 400);
  }
  
  /* Look up transaction */
  transaction_t* transaction = transaction_manager_get_transaction(manager, tx_id);
  if (!transaction) {
    return http_response_error("Transaction not found", 404);
  }
  
  json_value_t* result = NULL;
  json_value_t* document = NULL;
  const char* doc_id = NULL;
  http_response_t* response = NULL;

  /* Handle different operations based on HTTP method */
  http_method_t method = request->method;

  if (method == HTTP_GET) {
    /* Handle query operation */
    json_value_t* query = NULL;

    if (request->body && strlen(request->body) > 0) {
      query = json_parse(request->body);
      if (!query) {
        return http_response_error("Invalid JSON in request body", 400);
      }
    } else {
      /* Empty query matches all documents */
      query = json_create_object();
    }

    /* Execute query in transaction */
    result = transaction_query_documents(manager, transaction, collection, query);

    /* Free query JSON */
    json_free(query);

    if (!result) {
      return http_response_error("Failed to execute query", 500);
    }

    response = http_response_json(result, 200);
  }
  else if (method == HTTP_POST) {
    /* Handle insert operation */
    if (!request->body || strlen(request->body) == 0) {
      return http_response_error("Request body required", 400);
    }

    document = json_parse(request->body);
    if (!document) {
      return http_response_error("Invalid JSON in request body", 400);
    }

    /* Insert document and create result object */
    int insert_result = transaction_insert_document(manager, transaction, collection, document);

    /* Create a result object based on the operation result */
    if (insert_result == 0) {
      result = json_create_object();
      if (result) {
        json_object_set(result, "success", json_create_boolean(1));

        /* Extract ID if available */
        json_value_t* id = json_object_get(document, "_id");
        if (id && id->type == JSON_STRING) {
          json_object_set(result, "_id", json_create_string(id->value.string));
        }

        response = http_response_json(result, 201);
      }
    }

    /* Free document JSON (it's cloned in the transaction) */
    json_free(document);

    if (!result) {
      return http_response_error("Failed to insert document", 500);
    }
  }
  else if (method == HTTP_PUT) {
    /* Handle update operation - get document ID from path */
    doc_id = http_request_get_path_param(request, "id");
    if (!doc_id) {
      return http_response_error("Document ID required", 400);
    }

    /* Parse document from request body */
    if (!request->body || strlen(request->body) == 0) {
      return http_response_error("Request body required", 400);
    }

    document = json_parse(request->body);
    if (!document) {
      return http_response_error("Invalid JSON in request body", 400);
    }

    /* Update document and create result object */
    int update_result = transaction_update_document(manager, transaction, collection, doc_id, document);

    if (update_result == 0) {
      result = json_create_object();
      if (result) {
        json_object_set(result, "success", json_create_boolean(1));
        json_object_set(result, "_id", json_create_string(doc_id));
        json_object_set(result, "updated", json_create_boolean(1));

        response = http_response_json(result, 200);
      }
    }

    /* Free document JSON */
    json_free(document);

    if (!result) {
      return http_response_error("Document not found or update failed", 404);
    }
  }
  else if (method == HTTP_DELETE) {
    /* Handle delete operation - get document ID from path */
    doc_id = http_request_get_path_param(request, "id");
    if (!doc_id) {
      return http_response_error("Document ID required", 400);
    }

    /* Delete document in transaction */
    int delete_result = transaction_delete_document(manager, transaction, collection, doc_id);
    if (!delete_result) {
      return http_response_error("Document not found or delete failed", 404);
    }

    /* Create success response */
    result = json_create_object();
    if (!result) {
      return http_response_error("Failed to create response", 500);
    }

    json_object_set(result, "success", json_create_boolean(1));
    json_object_set(result, "deleted", json_create_boolean(1));
    json_object_set(result, "_id", json_create_string(doc_id));

    response = http_response_json(result, 200);
  }
  else {
    return http_response_error("Method not allowed", 405);
  }

  return response;
}