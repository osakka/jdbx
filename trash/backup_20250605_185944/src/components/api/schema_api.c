#include "api/api.h"
#include "database/database.h"
#include "utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Get schema for a collection */
http_response_t* api_handle_schema_get(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Extract collection name from path */
  const char* path = request->path;
  char* collection_name = NULL;
  
  if (strcmp(path, "/api/schemas") == 0) {
    /* List all JSON schemas */
    json_value_t* schemas_list = db_list_json_schemas(ctx->db);
    if (!schemas_list) {
      return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                   "{\"error\":\"Failed to list schemas\"}", "application/json");
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_object_set(response, "schemas", schemas_list);
    
    /* Serialize response */
    char* response_str = json_stringify(response);
    
    /* Free resources */
    json_free(response);
    
    return create_http_response(HTTP_OK, response_str, "application/json");
  } else if (strncmp(path, "/api/schemas/", 13) == 0) {
    /* Get schema for specific collection */
    collection_name = strdup(path + 13);
    
    /* Get JSON schema for collection */
    json_value_t* schema = db_get_json_schema(ctx->db, collection_name);
    if (!schema) {
      free(collection_name);
      return create_http_response(HTTP_NOT_FOUND, 
                   "{\"error\":\"Schema not found\"}", "application/json");
    }
    
    /* Create response object */
    json_value_t* response = json_create_object();
    json_object_set(response, "collection", json_create_string(collection_name));
    json_object_set(response, "schema", schema);
    
    /* Serialize response */
    char* response_str = json_stringify(response);
    
    /* Free resources */
    json_free(response);
    free(collection_name);
    
    return create_http_response(HTTP_OK, response_str, "application/json");
  } else {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid path\"}", "application/json");
  }
}

/* Create a new schema */
http_response_t* api_handle_schema_create(api_context_t* ctx, http_request_t* request) {
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
  
  /* Extract collection name */
  json_value_t* collection_val = json_object_get(body, "collection");
  if (!collection_val || collection_val->type != JSON_STRING) {
    json_free(body);
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Collection name is required\"}", "application/json");
  }
  
  const char* collection_name = collection_val->value.string;
  
  /* Check if collection exists */
  db_collection_t* collection = db_get_collection(ctx->db, collection_name);
  if (!collection) {
    json_free(body);
    return create_http_response(HTTP_NOT_FOUND, 
                 "{\"error\":\"Collection not found\"}", "application/json");
  }
  
  /* Extract schema from request body */
  json_value_t* schema_val = json_object_get(body, "schema");
  if (!schema_val || schema_val->type != JSON_OBJECT) {
    json_free(body);
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Schema definition is required\"}", "application/json");
  }
  
  /* Store JSON schema */
  if (!db_store_json_schema(ctx->db, collection_name, schema_val)) {
    json_free(body);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to store schema\"}", "application/json");
  }
  
  /* Create response */
  json_value_t* response = json_create_object();
  json_object_set(response, "message", json_create_string("Schema created"));
  json_object_set(response, "collection", json_create_string(collection_name));
  
  /* Serialize response */
  char* response_str = json_stringify(response);
  
  /* Free resources */
  json_free(response);
  json_free(body);
  
  return create_http_response(HTTP_CREATED, response_str, "application/json");
}

/* Update an existing schema */
http_response_t* api_handle_schema_update(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request || !request->body) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Extract collection name from path */
  const char* path = request->path;
  if (strncmp(path, "/api/schemas/", 13) != 0) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid path\"}", "application/json");
  }
  
  char* collection_name = strdup(path + 13);
  
  /* Check if collection exists */
  db_collection_t* collection = db_get_collection(ctx->db, collection_name);
  if (!collection) {
    free(collection_name);
    return create_http_response(HTTP_NOT_FOUND, 
                 "{\"error\":\"Collection not found\"}", "application/json");
  }
  
  /* Parse request body */
  json_value_t* body = json_parse(request->body);
  if (!body || body->type != JSON_OBJECT) {
    if (body) json_free(body);
    free(collection_name);
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request body\"}", "application/json");
  }
  
  /* Extract schema from request body */
  json_value_t* schema_val = json_object_get(body, "schema");
  if (!schema_val || schema_val->type != JSON_OBJECT) {
    json_free(body);
    free(collection_name);
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Schema definition is required\"}", "application/json");
  }
  
  /* Update JSON schema */
  if (!db_store_json_schema(ctx->db, collection_name, schema_val)) {
    json_free(body);
    free(collection_name);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to update schema\"}", "application/json");
  }
  
  /* Create response */
  json_value_t* response = json_create_object();
  json_object_set(response, "message", json_create_string("Schema updated successfully"));
  json_object_set(response, "collection", json_create_string(collection_name));
  
  /* Serialize response */
  char* response_str = json_stringify(response);
  
  /* Free resources */
  json_free(response);
  json_free(body);
  free(collection_name);
  
  return create_http_response(HTTP_OK, response_str, "application/json");
}

/* Delete a schema */
http_response_t* api_handle_schema_delete(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Extract collection name from path */
  const char* path = request->path;
  if (strncmp(path, "/api/schemas/", 13) != 0) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid path\"}", "application/json");
  }
  
  char* collection_name = strdup(path + 13);
  
  /* Check if collection exists */
  db_collection_t* collection = db_get_collection(ctx->db, collection_name);
  if (!collection) {
    free(collection_name);
    return create_http_response(HTTP_NOT_FOUND, 
                 "{\"error\":\"Collection not found\"}", "application/json");
  }
  
  /* Delete JSON schema */
  if (!db_delete_json_schema(ctx->db, collection_name)) {
    free(collection_name);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to delete schema\"}", "application/json");
  }
  
  /* Create response */
  json_value_t* response = json_create_object();
  json_object_set(response, "message", json_create_string("Schema deleted successfully"));
  json_object_set(response, "collection", json_create_string(collection_name));
  
  /* Serialize response */
  char* response_str = json_stringify(response);
  
  /* Free resources */
  json_free(response);
  free(collection_name);
  
  return create_http_response(HTTP_OK, response_str, "application/json");
}

/* Validate a document against a schema */
http_response_t* api_handle_schema_validate(api_context_t* ctx, http_request_t* request) {
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
  
  /* Extract collection name */
  json_value_t* collection_val = json_object_get(body, "collection");
  if (!collection_val || collection_val->type != JSON_STRING) {
    json_free(body);
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Collection name is required\"}", "application/json");
  }
  
  const char* collection_name = collection_val->value.string;
  
  /* Extract document to validate */
  json_value_t* document_val = json_object_get(body, "document");
  if (!document_val || document_val->type != JSON_OBJECT) {
    json_free(body);
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Document to validate is required\"}", "application/json");
  }
  
  /* Get JSON schema for collection */
  json_value_t* schema = db_get_json_schema(ctx->db, collection_name);
  if (!schema) {
    json_free(body);
    return create_http_response(HTTP_NOT_FOUND, 
                 "{\"error\":\"Schema not found for collection\"}", "application/json");
  }
  
  /* Validate document against JSON schema */
  char* error_msg = NULL;
  int is_valid = db_validate_json_schema(schema, document_val, &error_msg);
  
  /* Create response */
  json_value_t* response = json_create_object();
  json_object_set(response, "valid", json_create_boolean(is_valid));
  
  if (!is_valid && error_msg) {
    json_object_set(response, "error", json_create_string(error_msg));
  }
  
  /* Serialize response */
  char* response_str = json_stringify(response);
  
  /* Free resources */
  json_free(response);
  json_free(body);
  json_free(schema);
  
  if (error_msg) free(error_msg);
  
  return create_http_response(HTTP_OK, response_str, "application/json");
}