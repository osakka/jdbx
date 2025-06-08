#include "api/api.h"
#include "js/js_engine.h"
#include "database/database.h"
#include "utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h> /* For PATH_MAX */

/* Global JavaScript engine */
js_engine_t *g_js_engine = NULL;

/* Declare make_path_absolute from main.c */
extern void make_path_absolute(const char* rel_path, char* abs_path, size_t abs_path_size);

/* Ensure USE_QUICKJS is defined when JavaScript is enabled */
#if !defined(DISABLE_JS) && !defined(USE_QUICKJS)
#define USE_QUICKJS
#endif

/* Initialize JavaScript engine */
void js_api_init(database_t *db) {
#ifndef DISABLE_JS
  /* Get absolute paths for JS directories */
  char functions_dir[PATH_MAX] = {0};
  char validators_dir[PATH_MAX] = {0};
  char transforms_dir[PATH_MAX] = {0};

  /* Make absolute paths */
  make_path_absolute("functions", functions_dir, sizeof(functions_dir));
  make_path_absolute("validators", validators_dir, sizeof(validators_dir));
  make_path_absolute("transforms", transforms_dir, sizeof(transforms_dir));

  /* Create functions directory if it doesn't exist */
  struct stat st = {0};
  if (stat(functions_dir, &st) == -1) {
    printf("Creating functions directory at: %s\n", functions_dir);
    mkdir(functions_dir, 0755);
  }

  /* Create validators directory if it doesn't exist */
  if (stat(validators_dir, &st) == -1) {
    printf("Creating validators directory at: %s\n", validators_dir);
    mkdir(validators_dir, 0755);
  }

  /* Create transforms directory if it doesn't exist */
  if (stat(transforms_dir, &st) == -1) {
    printf("Creating transforms directory at: %s\n", transforms_dir);
    mkdir(transforms_dir, 0755);
  }

  /* Initialize JavaScript engine - with error checking */
  if (g_js_engine != NULL) {
    fprintf(stderr, "Warning: JS engine was already initialized\n");
    return;
  }

  /* Initialize JavaScript engine */
  g_js_engine = js_engine_init(db);

  if (g_js_engine == NULL) {
    fprintf(stderr, "Warning: Failed to initialize JavaScript engine\n");
    LOG_ERROR("initialize JavaScript engine.");
  } else {
    printf("JavaScript engine initialized\n");
    LOG_INFO("JavaScript engine initialized.");
  }
#else /* JavaScript functionality disabled */
  fprintf(stderr, "JavaScript support is not available (disabled in this build)\n");
  g_js_engine = NULL;
#endif /* DISABLE_JS */
}

/* Free JavaScript engine */
void js_api_cleanup() {
#ifndef DISABLE_JS
  /* This function is kept for API compatibility, but actual cleanup
    is now performed directly in main.c to better handle memory management */

  /* If called directly, just null the pointer - actual freeing is done in main.c */
  g_js_engine = NULL;
#else /* JavaScript functionality disabled */
  /* No-op when JavaScript is disabled */
#endif /* DISABLE_JS */
}

/* Handle JavaScript query request */
http_response_t* api_handle_js_query(api_context_t* ctx, http_request_t* request) {
#ifndef DISABLE_JS
  if (!ctx || !request || !request->body) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid request\"}", "application/json");
  }

  /* Parse request body */
  json_value_t* body = json_parse(request->body);
  if (!body || body->type != JSON_OBJECT) {
    if (body) json_free(body);
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid request body\"}", "application/json");
  }

  /* Extract collection and query script */
  json_value_t* collection_val = json_object_get(body, "collection");
  json_value_t* query_val = json_object_get(body, "query");

  if (!collection_val || collection_val->type != JSON_STRING ||
    !query_val || query_val->type != JSON_STRING) {
    json_free(body);
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Collection and query are required\"}", "application/json");
  }

  const char* collection = collection_val->value.string;
  const char* query = query_val->value.string;

  /* Execute JavaScript query */
  json_value_t* result = js_execute_query(g_js_engine, collection, query);

  /* Free request body */
  json_free(body);

  if (!result) {
    const char* error = js_get_last_error(g_js_engine);
    char error_response[1024];
    snprintf(error_response, sizeof(error_response),
        "{\"error\":\"Query execution failed\", \"details\":\"%s\"}",
        error ? error : "Unknown error");

    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 error_response, "application/json");
  }

  /* Create response */
  json_value_t* response = json_create_object();
  json_object_set(response, "documents", result);
  json_object_set(response, "count", json_create_integer(json_array_size(result)));

  /* Serialize response */
  char* response_str = json_stringify(response);

  /* Free resources */
  json_free(response);

  /* Create HTTP response */
  http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");

  /* Free response string */
  free(response_str);

  return http_response;
#else /* JavaScript functionality disabled */
  /* Return error when JavaScript is disabled */
  return create_http_response(HTTP_NOT_IMPLEMENTED,
               "{\"error\":\"JavaScript functionality is not available in this build\"}",
               "application/json");
#endif /* DISABLE_JS */
}

/* Handle user function registration */
http_response_t* api_handle_js_function_register(api_context_t* ctx, http_request_t* request) {
#ifndef DISABLE_JS
  if (!ctx || !request || !request->body) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid request\"}", "application/json");
  }

  /* Parse request body */
  json_value_t* body = json_parse(request->body);
  if (!body || body->type != JSON_OBJECT) {
    if (body) json_free(body);
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid request body\"}", "application/json");
  }

  /* Extract function name and code */
  json_value_t* name_val = json_object_get(body, "name");
  json_value_t* code_val = json_object_get(body, "code");

  if (!name_val || name_val->type != JSON_STRING ||
    !code_val || code_val->type != JSON_STRING) {
    json_free(body);
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Function name and code are required\"}", "application/json");
  }

  const char* name = name_val->value.string;
  const char* code = code_val->value.string;

  /* Register function */
  int result = js_register_user_function(g_js_engine, name, code);

  /* Free request body */
  json_free(body);

  if (!result) {
    const char* error = js_get_last_error(g_js_engine);
    char error_response[1024];
    snprintf(error_response, sizeof(error_response),
        "{\"error\":\"Function registration failed\", \"details\":\"%s\"}",
        error ? error : "Unknown error");

    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 error_response, "application/json");
  }

  /* Create response */
  json_value_t* response = json_create_object();
  json_object_set(response, "success", json_create_boolean(1));
  json_object_set(response, "message", json_create_string("Function registered successfully"));
  json_object_set(response, "name", json_create_string(name));

  /* Serialize response */
  char* response_str = json_stringify(response);

  /* Free resources */
  json_free(response);

  /* Create HTTP response */
  http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");

  /* Free response string */
  free(response_str);

  return http_response;
#else /* JavaScript functionality disabled */
  /* JavaScript functionality not available */
  return create_http_response(HTTP_NOT_IMPLEMENTED,
               "{\"error\":\"JavaScript functionality is not available in this build\"}",
               "application/json");
#endif /* DISABLE_JS */
}

/* Handle user function execution */
http_response_t* api_handle_js_function_execute(api_context_t* ctx, http_request_t* request) {
#ifndef DISABLE_JS
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid request\"}", "application/json");
  }

  /* Extract function name from path */
  const char* path = request->path;
  if (strncmp(path, "/api/js/functions/", 18) != 0) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid path format\"}", "application/json");
  }

  const char* name = path + 18;

  /* Parse request body as arguments */
  json_value_t* args = NULL;
  if (request->body && strlen(request->body) > 0) {
    args = json_parse(request->body);
    if (!args) {
      return create_http_response(HTTP_BAD_REQUEST,
                   "{\"error\":\"Invalid request body\"}", "application/json");
    }
  } else {
    /* Empty arguments */
    args = json_create_object();
  }

  /* Execute function */
  json_value_t* result = NULL;
  int success = js_call_user_function(g_js_engine, name, args, &result);

  /* Free arguments */
  json_free(args);

  if (!success) {
    const char* error = js_get_last_error(g_js_engine);
    char error_response[1024];
    snprintf(error_response, sizeof(error_response),
        "{\"error\":\"Function execution failed\", \"details\":\"%s\"}",
        error ? error : "Unknown error");

    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 error_response, "application/json");
  }

  /* Create response */
  json_value_t* response = json_create_object();
  json_object_set(response, "success", json_create_boolean(1));
  json_object_set(response, "result", result ? result : json_create_null());

  /* Serialize response */
  char* response_str = json_stringify(response);

  /* Free resources */
  json_free(response);

  /* Create HTTP response */
  http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");

  /* Free response string */
  free(response_str);

  return http_response;
#else /* JavaScript functionality disabled */
  /* JavaScript functionality not available */
  return create_http_response(HTTP_NOT_IMPLEMENTED,
               "{\"error\":\"JavaScript functionality is not available in this build\"}",
               "application/json");
#endif /* DISABLE_JS */
}

/* Handle validator registration */
http_response_t* api_handle_js_validator_register(api_context_t* ctx, http_request_t* request) {
#ifndef DISABLE_JS
  if (!ctx || !request || !request->body) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid request\"}", "application/json");
  }

  /* Parse request body */
  json_value_t* body = json_parse(request->body);
  if (!body || body->type != JSON_OBJECT) {
    if (body) json_free(body);
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid request body\"}", "application/json");
  }

  /* Extract collection and validator code */
  json_value_t* collection_val = json_object_get(body, "collection");
  json_value_t* code_val = json_object_get(body, "code");

  if (!collection_val || collection_val->type != JSON_STRING ||
    !code_val || code_val->type != JSON_STRING) {
    json_free(body);
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Collection name and validator code are required\"}", "application/json");
  }

  const char* collection = collection_val->value.string;
  const char* code = code_val->value.string;

  /* Get absolute path for validator file */
  char rel_path[512];
  char validator_path[PATH_MAX];
  snprintf(rel_path, sizeof(rel_path), "validators/%s.js", collection);
  make_path_absolute(rel_path, validator_path, sizeof(validator_path));

  /* Save validator to file */
  FILE* file = fopen(validator_path, "wb");
  if (!file) {
    json_free(body);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Failed to save validator\"}", "application/json");
  }

  fwrite(code, 1, strlen(code), file);
  fclose(file);

  /* Free request body */
  json_free(body);

  /* Create response */
  json_value_t* response = json_create_object();
  json_object_set(response, "success", json_create_boolean(1));
  json_object_set(response, "message", json_create_string("Validator registered successfully"));
  json_object_set(response, "collection", json_create_string(collection));

  /* Serialize response */
  char* response_str = json_stringify(response);

  /* Free resources */
  json_free(response);

  /* Create HTTP response */
  http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");

  /* Free response string */
  free(response_str);

  return http_response;
#else /* JavaScript functionality disabled */
  /* JavaScript functionality not available */
  return create_http_response(HTTP_NOT_IMPLEMENTED,
               "{\"error\":\"JavaScript functionality is not available in this build\"}",
               "application/json");
#endif /* DISABLE_JS */
}

/* Handle transformer registration */
http_response_t* api_handle_js_transformer_register(api_context_t* ctx, http_request_t* request) {
#ifndef DISABLE_JS
  if (!ctx || !request || !request->body) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid request\"}", "application/json");
  }

  /* Parse request body */
  json_value_t* body = json_parse(request->body);
  if (!body || body->type != JSON_OBJECT) {
    if (body) json_free(body);
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid request body\"}", "application/json");
  }

  /* Extract collection and transformer code */
  json_value_t* collection_val = json_object_get(body, "collection");
  json_value_t* code_val = json_object_get(body, "code");

  if (!collection_val || collection_val->type != JSON_STRING ||
    !code_val || code_val->type != JSON_STRING) {
    json_free(body);
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Collection name and transformer code are required\"}", "application/json");
  }

  const char* collection = collection_val->value.string;
  const char* code = code_val->value.string;

  /* Get absolute path for transformer file */
  char rel_path[512];
  char transformer_path[PATH_MAX];
  snprintf(rel_path, sizeof(rel_path), "transforms/%s.js", collection);
  make_path_absolute(rel_path, transformer_path, sizeof(transformer_path));

  /* Save transformer to file */
  FILE* file = fopen(transformer_path, "wb");
  if (!file) {
    json_free(body);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Failed to save transformer\"}", "application/json");
  }

  fwrite(code, 1, strlen(code), file);
  fclose(file);

  /* Free request body */
  json_free(body);

  /* Create response */
  json_value_t* response = json_create_object();
  json_object_set(response, "success", json_create_boolean(1));
  json_object_set(response, "message", json_create_string("Transformer registered successfully"));
  json_object_set(response, "collection", json_create_string(collection));

  /* Serialize response */
  char* response_str = json_stringify(response);

  /* Free resources */
  json_free(response);

  /* Create HTTP response */
  http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");

  /* Free response string */
  free(response_str);

  return http_response;
#else /* JavaScript functionality disabled */
  /* JavaScript functionality not available */
  return create_http_response(HTTP_NOT_IMPLEMENTED,
               "{\"error\":\"JavaScript functionality is not available in this build\"}",
               "application/json");
#endif /* DISABLE_JS */
}

/* Handle JavaScript code evaluation */
http_response_t* api_handle_js_eval(api_context_t* ctx, http_request_t* request) {
#ifndef DISABLE_JS
  if (!ctx || !request || !request->body) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid request\"}", "application/json");
  }

  /* Parse request body */
  json_value_t* body = json_parse(request->body);
  if (!body || body->type != JSON_OBJECT) {
    if (body) json_free(body);
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid request body\"}", "application/json");
  }

  /* Extract code */
  json_value_t* code_val = json_object_get(body, "code");

  if (!code_val || code_val->type != JSON_STRING) {
    json_free(body);
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"JavaScript code is required\"}", "application/json");
  }

  const char* code = code_val->value.string;

  /* Check if JS engine is initialized */
  if (!g_js_engine) {
    json_free(body);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"JavaScript engine not initialized\"}", "application/json");
  }

  /* Evaluate JavaScript code */
  char* result_str = NULL;
  int success = js_engine_eval(g_js_engine, code, &result_str);

  /* Free request body */
  json_free(body);

  if (!success) {
    const char* error = js_get_last_error(g_js_engine);
    char error_response[1024];
    snprintf(error_response, sizeof(error_response),
        "{\"error\":\"JavaScript evaluation failed\", \"details\":\"%s\"}",
        error ? error : "Unknown error");

    if (result_str) free(result_str);

    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 error_response, "application/json");
  }

  /* Create response */
  json_value_t* result = NULL;
  if (result_str && strlen(result_str) > 0) {
    result = json_parse(result_str);
  }

  json_value_t* response = json_create_object();
  json_object_set(response, "success", json_create_boolean(1));
  json_object_set(response, "result", result ? result : json_create_null());

  /* Serialize response */
  char* response_str = json_stringify(response);

  /* Free resources */
  if (result) json_free(result);
  if (result_str) free(result_str);
  json_free(response);

  /* Create HTTP response */
  http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");

  /* Free response string */
  free(response_str);

  return http_response;
#else /* JavaScript functionality disabled */
  /* JavaScript functionality not available */
  return create_http_response(HTTP_NOT_IMPLEMENTED,
               "{\"error\":\"JavaScript functionality is not available in this build\"}",
               "application/json");
#endif /* DISABLE_JS */
}