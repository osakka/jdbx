#include "jsondb/api/api.h"
#include "jsondb/database/database.h"
#include "jsondb/utils/json.h"
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
        /* List all schemas */
        json_value_t* collections = db_list_collections(ctx->db);
        if (!collections) {
            return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                      "{\"error\":\"Failed to list collections\"}", "application/json");
        }
        
        /* Create schemas array */
        json_value_t* schemas = json_create_array();
        if (!schemas) {
            json_free(collections);
            return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                      "{\"error\":\"Failed to create schemas array\"}", "application/json");
        }
        
        /* Iterate through collections and get their schemas */
        for (size_t i = 0; i < json_array_size(collections); i++) {
            json_value_t* coll_name_val = json_array_get(collections, i);
            if (!coll_name_val || coll_name_val->type != JSON_STRING) {
                continue;
            }
            
            /* Get schema for collection */
            schema_t* schema = db_get_schema(ctx->db, coll_name_val->value.string);
            if (schema) {
                /* Convert schema to JSON */
                json_value_t* schema_json = db_schema_to_json(schema);
                if (schema_json) {
                    /* Add collection name */
                    json_object_set(schema_json, "collection", json_create_string(coll_name_val->value.string));
                    /* Add schema to array */
                    json_array_append(schemas, schema_json);
                }
            }
        }
        
        /* Create response */
        json_value_t* response = json_create_object();
        json_object_set(response, "schemas", schemas);
        
        /* Serialize response */
        char* response_str = json_stringify(response);
        
        /* Free resources */
        json_free(response);
        json_free(collections);
        
        return create_http_response(HTTP_OK, response_str, "application/json");
    } else if (strncmp(path, "/api/schemas/", 13) == 0) {
        /* Get schema for specific collection */
        collection_name = strdup(path + 13);
        
        /* Get schema for collection */
        schema_t* schema = db_get_schema(ctx->db, collection_name);
        if (!schema) {
            free(collection_name);
            return create_http_response(HTTP_NOT_FOUND, 
                                      "{\"error\":\"Schema not found\"}", "application/json");
        }
        
        /* Convert schema to JSON */
        json_value_t* schema_json = db_schema_to_json(schema);
        if (!schema_json) {
            free(collection_name);
            return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                      "{\"error\":\"Failed to convert schema to JSON\"}", "application/json");
        }
        
        /* Add collection name */
        json_object_set(schema_json, "collection", json_create_string(collection_name));
        
        /* Serialize schema */
        char* schema_str = json_stringify(schema_json);
        
        /* Free resources */
        json_free(schema_json);
        free(collection_name);
        
        return create_http_response(HTTP_OK, schema_str, "application/json");
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
    
    /* Create schema from request body */
    schema_t* schema = db_schema_from_json(body);
    if (!schema) {
        json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid schema definition\"}", "application/json");
    }
    
    /* Attach schema to collection */
    if (!db_attach_schema(ctx->db, collection_name, schema)) {
        db_free_schema(schema);
        json_free(body);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to attach schema to collection\"}", "application/json");
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_object_set(response, "message", json_create_string("Schema created successfully"));
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
    
    /* Create schema from request body */
    schema_t* schema = db_schema_from_json(body);
    if (!schema) {
        json_free(body);
        free(collection_name);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid schema definition\"}", "application/json");
    }
    
    /* Attach schema to collection (this will replace the existing schema) */
    if (!db_attach_schema(ctx->db, collection_name, schema)) {
        db_free_schema(schema);
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
    
    /* Detach schema from collection */
    if (!db_detach_schema(ctx->db, collection_name)) {
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
    
    /* Get schema for collection */
    schema_t* schema = db_get_schema(ctx->db, collection_name);
    if (!schema) {
        json_free(body);
        return create_http_response(HTTP_NOT_FOUND, 
                                  "{\"error\":\"Schema not found for collection\"}", "application/json");
    }
    
    /* Validate document against schema */
    schema_validation_result_t result = db_validate_document(schema, document_val);
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_object_set(response, "valid", json_create_boolean(result.is_valid));
    
    if (!result.is_valid) {
        json_object_set(response, "error_field", json_create_string(result.error_field ? result.error_field : "unknown"));
        json_object_set(response, "error_message", json_create_string(result.error_message ? result.error_message : "Unknown validation error"));
    }
    
    /* Serialize response */
    char* response_str = json_stringify(response);
    
    /* Free resources */
    json_free(response);
    json_free(body);
    
    if (result.error_field) free(result.error_field);
    if (result.error_message) free(result.error_message);
    
    return create_http_response(HTTP_OK, response_str, "application/json");
}