#include "api/api.h"
#include "database/document_storage.h"
#include "database/database.h"
#include "database/virtual_layer.h"
#include "utils/json.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * @file virtual_collections_api.c
 * @brief Virtual Collections API - Manages logical collections based on document types
 * 
 * Virtual collections are logical groupings of documents based on their 'type' and 'collection' fields.
 * All data is physically stored in a single unified collection (default/documents).
 * This API provides CRUD operations for managing virtual collection metadata.
 */

/* Map virtual collection names to document types */
static const char* get_doc_type_for_virtual_collection(const char* collection_name) {
    if (strcmp(collection_name, VIRTUAL_COLLECTION_USERS) == 0) return DOC_TYPE_NAME_USER;
    if (strcmp(collection_name, VIRTUAL_COLLECTION_ROLES) == 0) return DOC_TYPE_NAME_ROLE;
    if (strcmp(collection_name, VIRTUAL_COLLECTION_LIBRARIES) == 0) return DOC_TYPE_NAME_LIBRARY;
    if (strcmp(collection_name, VIRTUAL_COLLECTION_SESSIONS) == 0) return DOC_TYPE_NAME_SESSION;
    if (strcmp(collection_name, VIRTUAL_COLLECTION_FUNCTIONS) == 0) return DOC_TYPE_NAME_FUNCTION;
    if (strcmp(collection_name, VIRTUAL_COLLECTION_VALIDATORS) == 0) return DOC_TYPE_NAME_VALIDATOR;
    if (strcmp(collection_name, VIRTUAL_COLLECTION_TRANSFORMERS) == 0) return DOC_TYPE_NAME_TRANSFORMER;
    if (strcmp(collection_name, VIRTUAL_COLLECTION_METRICS) == 0) return DOC_TYPE_NAME_METRIC;
    if (strcmp(collection_name, VIRTUAL_COLLECTION_CONFIGS) == 0) return DOC_TYPE_NAME_CONFIG;
    if (strcmp(collection_name, VIRTUAL_COLLECTION_INDEXES) == 0) return DOC_TYPE_NAME_INDEX;
    if (strcmp(collection_name, VIRTUAL_COLLECTION_SCHEMAS) == 0) return DOC_TYPE_NAME_SCHEMA;
    
    /* For custom collections, use the collection name as doc type */
    return collection_name;
}

/**
 * List all virtual collections
 * GET /api/collections
 */
http_response_t* api_handle_virtual_collections_list(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                     "{\"error\":\"Database not initialized\"}", "application/json");
    }
    
    /* Check if library parameter is provided in query string */
    char* target_library_copy = NULL;
    const char* target_library = NULL;
    if (request && request->query) {
        /* Parse library parameter from query string */
        char* query_copy = BUFFER_STRDUP(request->query);
        if (query_copy) {
            char* param = strtok(query_copy, "&");
            while (param != NULL) {
                if (strncmp(param, "library=", 8) == 0) {
                    target_library_copy = BUFFER_STRDUP(param + 8);
                    target_library = target_library_copy;
                    break;
                }
                param = strtok(NULL, "&");
            }
            BUFFER_FREE(query_copy);
        }
    }
    
    /* Query collection documents from unified storage */
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string(DOC_TYPE_NAME_COLLECTION));
    
    /* Filter by library if specified */
    if (target_library) {
        json_object_set(query, "library", json_create_string(target_library));
    }
    
    /* Use virtual layer to query collection documents - single source of truth */
    json_value_t* results = virtual_query(ctx->db, DOC_TYPE_NAME_COLLECTION, target_library ? target_library : "default", "configs", query);
    /* CHECKPOINT: json_free(query); */
    
    json_value_t* response = json_create_object();
    json_value_t* virtual_collections_array = json_create_array();
    
    if (results) {
        json_value_t* collection_docs = json_object_get(results, "documents");
        if (collection_docs && collection_docs->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_array_size(collection_docs); i++) {
                json_value_t* coll_doc = json_array_get(collection_docs, i);
                json_value_t* name_val = json_object_get(coll_doc, "name");
                json_value_t* library_val = json_object_get(coll_doc, "library");
                
                if (name_val && library_val && 
                    name_val->type == JSON_STRING && library_val->type == JSON_STRING) {
                    
                    const char* coll_name = name_val->value.string;
                    const char* virtual_library = library_val->value.string;
                    
                    /* Create virtual collection info */
                    json_value_t* virtual_coll = json_create_object();
                    json_object_set(virtual_coll, "name", json_create_string(coll_name));
                    json_object_set(virtual_coll, "library", json_create_string(virtual_library));
                    
                    /* Build virtual path for compatibility */
                    char virtual_path[512];
                    snprintf(virtual_path, sizeof(virtual_path), "%s/%s", virtual_library, coll_name);
                    json_object_set(virtual_coll, "path", json_create_string(virtual_path));
                    
                    /* Count documents in this virtual collection */
                    const char* doc_type = get_doc_type_for_virtual_collection(coll_name);
                    json_value_t* count_query = json_create_object();
                    json_object_set(count_query, "type", json_create_string(doc_type));
                    json_object_set(count_query, "library", json_create_string(virtual_library));
                    
                    /* Use virtual layer to count documents - single source of truth */
                    json_value_t* count_results = virtual_query(ctx->db, doc_type, virtual_library, coll_name, count_query);
                    size_t doc_count = 0;
                    if (count_results) {
                        json_value_t* docs = json_object_get(count_results, "documents");
                        if (docs && docs->type == JSON_ARRAY) {
                            doc_count = json_array_size(docs);
                        }
                        /* CHECKPOINT: json_free(count_results); */
                    }
                    /* CHECKPOINT: json_free(count_query); */
                    
                    json_object_set(virtual_coll, "document_count", json_create_number(doc_count));
                    
                    /* Add metadata from collection document */
                    json_value_t* created_at = json_object_get(coll_doc, "created_at");
                    if (created_at) {
                        json_object_set(virtual_coll, "created_at", json_clone(created_at));
                    }
                    
                    json_array_append(virtual_collections_array, virtual_coll);
                }
            }
        }
        /* CHECKPOINT: json_free(results); */
    }
    
    /* Add default virtual collections if they don't exist yet */
    const char* default_collections[] = {
        VIRTUAL_COLLECTION_USERS, VIRTUAL_COLLECTION_ROLES, VIRTUAL_COLLECTION_SESSIONS,
        VIRTUAL_COLLECTION_METRICS, VIRTUAL_COLLECTION_CONFIGS, NULL
    };
    
    /* Determine which library to show default collections for */
    const char* show_library = target_library ? target_library : "default";
    
    for (int i = 0; default_collections[i] != NULL; i++) {
        /* Check if this collection already exists in our results */
        int found = 0;
        for (size_t j = 0; j < json_array_size(virtual_collections_array); j++) {
            json_value_t* existing = json_array_get(virtual_collections_array, j);
            json_value_t* existing_name = json_object_get(existing, "name");
            json_value_t* existing_library = json_object_get(existing, "library");
            if (existing_name && existing_name->type == JSON_STRING &&
                existing_library && existing_library->type == JSON_STRING &&
                strcmp(existing_name->value.string, default_collections[i]) == 0 &&
                strcmp(existing_library->value.string, show_library) == 0) {
                found = 1;
                break;
            }
        }
        
        if (!found) {
            /* Add default virtual collection for the target library */
            json_value_t* default_coll = json_create_object();
            json_object_set(default_coll, "name", json_create_string(default_collections[i]));
            json_object_set(default_coll, "library", json_create_string(show_library));
            
            char default_path[512];
            snprintf(default_path, sizeof(default_path), "%s/%s", show_library, default_collections[i]);
            json_object_set(default_coll, "path", json_create_string(default_path));
            
            /* Count documents in this default virtual collection */
            const char* doc_type = get_doc_type_for_virtual_collection(default_collections[i]);
            json_value_t* count_query = json_create_object();
            json_object_set(count_query, "type", json_create_string(doc_type));
            json_object_set(count_query, "library", json_create_string(show_library));
            
            /* Use virtual layer to count documents - single source of truth */
            json_value_t* count_results = virtual_query(ctx->db, doc_type, show_library, default_collections[i], count_query);
            size_t doc_count = 0;
            if (count_results) {
                json_value_t* docs = json_object_get(count_results, "documents");
                if (docs && docs->type == JSON_ARRAY) {
                    doc_count = json_array_size(docs);
                }
                /* CHECKPOINT: json_free(count_results); */
            }
            /* CHECKPOINT: json_free(count_query); */
            
            json_object_set(default_coll, "document_count", json_create_number(doc_count));
            
            json_array_append(virtual_collections_array, default_coll);
        }
    }
    
    json_object_set(response, "collections", virtual_collections_array);
    
    char* response_str = json_stringify(response);
    /* CHECKPOINT: json_free(response); */
    
    /* Cleanup target library copy */
    if (target_library_copy) {
        BUFFER_FREE(target_library_copy);
    }
    
    return create_http_response(HTTP_OK, response_str, "application/json");
}

/**
 * Create a virtual collection
 * POST /api/collections
 */
http_response_t* api_handle_virtual_collection_create(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !request || !request->body) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Parse request body */
    json_value_t* body = json_parse(request->body);
    if (!body || body->type != JSON_OBJECT) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid JSON body\"}", "application/json");
    }
    
    /* Extract collection name and virtual library */
    json_value_t* name_val = json_object_get(body, "name");
    json_value_t* library_val = json_object_get(body, "library");
    
    if (!name_val || name_val->type != JSON_STRING) {
        /* CHECKPOINT: json_free(body); */
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Collection name required\"}", "application/json");
    }
    
    const char* collection_name = name_val->value.string;
    const char* virtual_library = library_val && library_val->type == JSON_STRING ? 
                                  library_val->value.string : "default";
    
    /* Validate collection name */
    if (strlen(collection_name) == 0) {
        /* CHECKPOINT: json_free(body); */
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid collection name\"}", "application/json");
    }
    
    /* Check if virtual collection already exists */
    json_value_t* exists_query = json_create_object();
    json_object_set(exists_query, "type", json_create_string(DOC_TYPE_NAME_COLLECTION));
    json_object_set(exists_query, "name", json_create_string(collection_name));
    json_object_set(exists_query, "library", json_create_string(virtual_library));
    
    /* Use virtual layer to check if collection exists - single source of truth */
    json_value_t* exists_results = virtual_query(ctx->db, DOC_TYPE_NAME_COLLECTION, virtual_library, "configs", exists_query);
    /* CHECKPOINT: json_free(exists_query); */
    
    if (exists_results) {
        json_value_t* existing_docs = json_object_get(exists_results, "documents");
        if (existing_docs && existing_docs->type == JSON_ARRAY && json_array_size(existing_docs) > 0) {
            /* CHECKPOINT: json_free(exists_results); */
            /* CHECKPOINT: json_free(body); */
            return create_http_response(HTTP_CONFLICT,
                         "{\"error\":\"Virtual collection already exists\"}", "application/json");
        }
        /* CHECKPOINT: json_free(exists_results); */
    }
    
    /* Create virtual collection document */
    json_value_t* coll_doc = json_create_object();
    json_object_set(coll_doc, "type", json_create_string(DOC_TYPE_NAME_COLLECTION));
    json_object_set(coll_doc, "name", json_create_string(collection_name));
    json_object_set(coll_doc, "library", json_create_string(virtual_library));
    json_object_set(coll_doc, "collection", json_create_string(VIRTUAL_COLLECTION_CONFIGS));
    json_object_set(coll_doc, "owner", json_create_string("admin"));
    
    /* Add timestamps */
    time_t now = time(NULL);
    char timestamp[64];
    struct tm* utc_tm = gmtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", utc_tm);
    json_object_set(coll_doc, "created_at", json_create_string(timestamp));
    json_object_set(coll_doc, "modified_at", json_create_string(timestamp));
    
    /* Add optional schema from request */
    json_value_t* schema_val = json_object_get(body, "schema");
    if (schema_val) {
        json_object_set(coll_doc, "schema", json_clone(schema_val));
    }
    
    /* Insert virtual collection document using virtual layer - single source of truth */
    json_value_t* result = virtual_insert(ctx->db, DOC_TYPE_NAME_COLLECTION, virtual_library, "configs", coll_doc, "admin");
    /* CHECKPOINT: json_free(coll_doc); */
    /* CHECKPOINT: json_free(body); */
    
    if (!result) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                     "{\"error\":\"Failed to create virtual collection\"}", "application/json");
    }
    
    /* Create success response */
    json_value_t* response = json_create_object();
    json_object_set(response, "success", json_create_boolean(1));
    json_object_set(response, "name", json_create_string(collection_name));
    json_object_set(response, "library", json_create_string(virtual_library));
    json_object_set(response, "message", json_create_string("Virtual collection created successfully"));
    
    char* response_str = json_stringify(response);
    /* CHECKPOINT: json_free(response); */
    /* CHECKPOINT: json_free(result); */
    
    return create_http_response(HTTP_CREATED, response_str, "application/json");
}

/**
 * Delete a virtual collection
 * DELETE /api/collections/{library}/{collection}
 */
http_response_t* api_handle_virtual_collection_drop(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !request) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract virtual library and collection from path */
    const char* path = request->path;
    if (strncmp(path, "/api/collections/", 17) != 0) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid path\"}", "application/json");
    }
    
    path += 17; /* Skip "/api/collections/" */
    
    /* Parse library/collection from path */
    const char* slash = strchr(path, '/');
    if (!slash) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid path format\"}", "application/json");
    }
    
    size_t lib_len = slash - path;
    char virtual_library[256], collection_name[256];
    strncpy(virtual_library, path, lib_len);
    virtual_library[lib_len] = '\0';
    strcpy(collection_name, slash + 1);
    
    /* Prevent deletion of system virtual collections */
    if (strcmp(collection_name, VIRTUAL_COLLECTION_USERS) == 0 ||
        strcmp(collection_name, VIRTUAL_COLLECTION_ROLES) == 0 ||
        strcmp(collection_name, VIRTUAL_COLLECTION_SESSIONS) == 0) {
        return create_http_response(HTTP_FORBIDDEN,
                     "{\"error\":\"Cannot delete system virtual collection\"}", "application/json");
    }
    
    /* Find the virtual collection document */
    json_value_t* find_query = json_create_object();
    json_object_set(find_query, "type", json_create_string(DOC_TYPE_NAME_COLLECTION));
    json_object_set(find_query, "name", json_create_string(collection_name));
    json_object_set(find_query, "library", json_create_string(virtual_library));
    
    /* Use virtual layer to find collection document - single source of truth */
    json_value_t* find_results = virtual_query(ctx->db, DOC_TYPE_NAME_COLLECTION, virtual_library, "configs", find_query);
    /* CHECKPOINT: json_free(find_query); */
    
    if (!find_results) {
        return create_http_response(HTTP_NOT_FOUND,
                     "{\"error\":\"Virtual collection not found\"}", "application/json");
    }
    
    json_value_t* coll_docs = json_object_get(find_results, "documents");
    if (!coll_docs || coll_docs->type != JSON_ARRAY || json_array_size(coll_docs) == 0) {
        /* CHECKPOINT: json_free(find_results); */
        return create_http_response(HTTP_NOT_FOUND,
                     "{\"error\":\"Virtual collection not found\"}", "application/json");
    }
    
    /* Get the collection document UUID */
    json_value_t* coll_doc = json_array_get(coll_docs, 0);
    json_value_t* uuid_val = json_object_get(coll_doc, "uuid");
    if (!uuid_val || uuid_val->type != JSON_STRING) {
        /* CHECKPOINT: json_free(find_results); */
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                     "{\"error\":\"Invalid collection document\"}", "application/json");
    }
    
    const char* collection_uuid = uuid_val->value.string;
    /* CHECKPOINT: json_free(find_results); */
    
    /* Delete the virtual collection document using virtual layer - single source of truth */
    if (!virtual_delete(ctx->db, collection_uuid)) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                     "{\"error\":\"Failed to delete virtual collection\"}", "application/json");
    }
    
    /* Note: We don't delete the actual documents in the virtual collection
     * This allows recovery and is safer for production systems */
    
    /* Create success response */
    json_value_t* response = json_create_object();
    json_object_set(response, "success", json_create_boolean(1));
    json_object_set(response, "name", json_create_string(collection_name));
    json_object_set(response, "library", json_create_string(virtual_library));
    json_object_set(response, "message", json_create_string("Virtual collection deleted successfully"));
    
    char* response_str = json_stringify(response);
    /* CHECKPOINT: json_free(response); */
    
    return create_http_response(HTTP_OK, response_str, "application/json");
}