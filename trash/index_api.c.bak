#include "api/api.h"
#include "database/database.h"
#include "utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Helper function to extract collection name and other segments from path */
static int extract_path_segments(const char* path, char** collection, char** index_name) {
    if (!path || !collection || !index_name) {
        return 0;
    }
    
    /* Expected formats:
     * /api/indexes/{collection} - For listing indexes
     * /api/indexes/{collection}/{index_name} - For specific index operations
     */
    
    /* Skip "/api/indexes/" prefix */
    const char* prefix = "/api/indexes/";
    size_t prefix_len = strlen(prefix);
    
    if (strncmp(path, prefix, prefix_len) != 0) {
        return 0;
    }
    
    /* Skip prefix */
    const char* rest = path + prefix_len;
    
    /* Find the collection name (ends at / or end of string) */
    const char* slash = strchr(rest, '/');
    if (slash) {
        /* Collection followed by index name */
        *collection = strndup(rest, slash - rest);
        *index_name = strdup(slash + 1);
    } else {
        /* Only collection */
        *collection = strdup(rest);
        *index_name = NULL;
    }
    
    return 1;
}

/* Helper function to parse index type from string */
static int parse_index_type(const char* type_str) {
    if (!type_str) {
        return INDEX_TYPE_NON_UNIQUE; /* Default type */
    }
    
    if (strcasecmp(type_str, "unique") == 0) {
        return INDEX_TYPE_UNIQUE;
    } else if (strcasecmp(type_str, "text") == 0) {
        return INDEX_TYPE_TEXT;
    } else if (strcasecmp(type_str, "geo") == 0) {
        return INDEX_TYPE_GEO;
    } else {
        return INDEX_TYPE_NON_UNIQUE; /* Default */
    }
}

/* List all indexes for a collection */
http_response_t* api_handle_index_list(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract collection name */
    char* collection = NULL;
    char* index_name = NULL;
    
    if (!extract_path_segments(request->path, &collection, &index_name)) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid path format\"}", "application/json");
    }
    
    /* Free index_name as we don't need it for listing */
    if (index_name) {
        free(index_name);
    }
    
    /* Check if collection exists */
    db_collection_t* coll = db_get_collection(ctx->db, collection);
    if (!coll) {
        free(collection);
        return create_http_response(HTTP_NOT_FOUND, 
                                  "{\"error\":\"Collection not found\"}", "application/json");
    }
    
    /* Get list of indexes */
    json_value_t* indexes = db_list_indexes(ctx->db, collection);
    
    free(collection);
    
    if (!indexes) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to list indexes\"}", "application/json");
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_object_set(response, "indexes", indexes);
    json_object_set(response, "count", json_create_integer(json_array_size(indexes)));
    
    /* Serialize response */
    char* response_str = json_stringify(response);
    
    /* Free resources */
    json_free(response);
    
    /* Create HTTP response */
    http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
    
    /* Free response string */
    free(response_str);
    
    return http_response;
}

/* Create a new index */
http_response_t* api_handle_index_create(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->body) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract collection name */
    char* collection = NULL;
    char* index_name = NULL;
    
    if (!extract_path_segments(request->path, &collection, &index_name)) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid path format\"}", "application/json");
    }
    
    /* Free index_name as we'll get it from the request body */
    if (index_name) {
        free(index_name);
        index_name = NULL;
    }
    
    /* Check if collection exists */
    db_collection_t* coll = db_get_collection(ctx->db, collection);
    if (!coll) {
        free(collection);
        return create_http_response(HTTP_NOT_FOUND, 
                                  "{\"error\":\"Collection not found\"}", "application/json");
    }
    
    /* Parse request body */
    json_value_t* body = json_parse(request->body);
    if (!body || body->type != JSON_OBJECT) {
        free(collection);
        if (body) json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request body\"}", "application/json");
    }
    
    /* Extract index parameters */
    json_value_t* name_val = json_object_get(body, "name");
    json_value_t* field_val = json_object_get(body, "field");
    json_value_t* type_val = json_object_get(body, "type");
    
    if (!name_val || name_val->type != JSON_STRING || 
        !field_val || field_val->type != JSON_STRING) {
        free(collection);
        json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Index name and field are required\"}", "application/json");
    }
    
    const char* name = name_val->value.string;
    const char* field = field_val->value.string;
    const char* type_str = (type_val && type_val->type == JSON_STRING) ? 
                           type_val->value.string : "non_unique";
    
    /* Parse index type */
    index_type_t type = parse_index_type(type_str);
    
    /* Create index */
    index_t* index = db_create_index(ctx->db, collection, name, field, type);
    
    /* Free request body */
    json_free(body);
    
    if (!index) {
        free(collection);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to create index\"}", "application/json");
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_object_set(response, "success", json_create_boolean(1));
    json_object_set(response, "message", json_create_string("Index created successfully"));
    json_object_set(response, "collection", json_create_string(collection));
    json_object_set(response, "name", json_create_string(name));
    json_object_set(response, "field", json_create_string(field));
    json_object_set(response, "type", json_create_string(type_str));
    
    /* Free collection name */
    free(collection);
    
    /* Serialize response */
    char* response_str = json_stringify(response);
    
    /* Free resources */
    json_free(response);
    
    /* Create HTTP response */
    http_response_t* http_response = create_http_response(HTTP_CREATED, response_str, "application/json");
    
    /* Free response string */
    free(response_str);
    
    return http_response;
}

/* Get details for a specific index */
http_response_t* api_handle_index_get(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract collection and index name */
    char* collection = NULL;
    char* index_name = NULL;
    
    if (!extract_path_segments(request->path, &collection, &index_name) || !index_name) {
        if (collection) free(collection);
        if (index_name) free(index_name);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid path format\"}", "application/json");
    }
    
    /* Get index */
    index_t* index = db_get_index(ctx->db, collection, index_name);
    if (!index) {
        free(collection);
        free(index_name);
        return create_http_response(HTTP_NOT_FOUND, 
                                  "{\"error\":\"Index not found\"}", "application/json");
    }
    
    /* Convert index to JSON */
    json_value_t* index_json = db_index_to_json(index);
    if (!index_json) {
        free(collection);
        free(index_name);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to get index details\"}", "application/json");
    }
    
    /* Set collection name */
    json_object_set(index_json, "collection", json_create_string(collection));
    
    /* Serialize response */
    char* response_str = json_stringify(index_json);
    
    /* Free resources */
    json_free(index_json);
    free(collection);
    free(index_name);
    
    /* Create HTTP response */
    http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
    
    /* Free response string */
    free(response_str);
    
    return http_response;
}

/* Delete an index */
http_response_t* api_handle_index_delete(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract collection and index name */
    char* collection = NULL;
    char* index_name = NULL;
    
    if (!extract_path_segments(request->path, &collection, &index_name) || !index_name) {
        if (collection) free(collection);
        if (index_name) free(index_name);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid path format\"}", "application/json");
    }
    
    /* Check if index exists */
    index_t* index = db_get_index(ctx->db, collection, index_name);
    if (!index) {
        free(collection);
        free(index_name);
        return create_http_response(HTTP_NOT_FOUND, 
                                  "{\"error\":\"Index not found\"}", "application/json");
    }
    
    /* Drop index */
    int result = db_drop_index(ctx->db, collection, index_name);
    if (!result) {
        free(collection);
        free(index_name);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to drop index\"}", "application/json");
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_object_set(response, "success", json_create_boolean(1));
    json_object_set(response, "message", json_create_string("Index dropped successfully"));
    json_object_set(response, "collection", json_create_string(collection));
    json_object_set(response, "name", json_create_string(index_name));
    
    /* Free collection and index name */
    free(collection);
    free(index_name);
    
    /* Serialize response */
    char* response_str = json_stringify(response);
    
    /* Free resources */
    json_free(response);
    
    /* Create HTTP response */
    http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
    
    /* Free response string */
    free(response_str);
    
    return http_response;
}

/* Rebuild an index */
http_response_t* api_handle_index_rebuild(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract collection and index name */
    char* collection = NULL;
    char* index_name = NULL;
    
    if (!extract_path_segments(request->path, &collection, &index_name)) {
        if (collection) free(collection);
        if (index_name) free(index_name);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid path format\"}", "application/json");
    }
    
    /* If no index name provided, rebuild all indexes for the collection */
    int result;
    if (!index_name) {
        result = db_rebuild_all_indexes(ctx->db, collection);
    } else {
        /* Check if index exists */
        index_t* index = db_get_index(ctx->db, collection, index_name);
        if (!index) {
            free(collection);
            free(index_name);
            return create_http_response(HTTP_NOT_FOUND, 
                                      "{\"error\":\"Index not found\"}", "application/json");
        }
        
        /* Rebuild index */
        result = db_rebuild_index(ctx->db, collection, index_name);
    }
    
    if (!result) {
        free(collection);
        if (index_name) free(index_name);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to rebuild index\"}", "application/json");
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_object_set(response, "success", json_create_boolean(1));
    
    if (index_name) {
        json_object_set(response, "message", json_create_string("Index rebuilt successfully"));
        json_object_set(response, "collection", json_create_string(collection));
        json_object_set(response, "name", json_create_string(index_name));
    } else {
        json_object_set(response, "message", json_create_string("All indexes rebuilt successfully"));
        json_object_set(response, "collection", json_create_string(collection));
    }
    
    /* Free collection and index name */
    free(collection);
    if (index_name) free(index_name);
    
    /* Serialize response */
    char* response_str = json_stringify(response);
    
    /* Free resources */
    json_free(response);
    
    /* Create HTTP response */
    http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
    
    /* Free response string */
    free(response_str);
    
    return http_response;
}

/* Get index statistics */
http_response_t* api_handle_index_stats(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract collection and index name */
    char* collection = NULL;
    char* index_name = NULL;
    
    if (!extract_path_segments(request->path, &collection, &index_name) || !index_name) {
        if (collection) free(collection);
        if (index_name) free(index_name);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid path format\"}", "application/json");
    }
    
    /* Get index stats */
    json_value_t* stats = db_index_stats(ctx->db, collection, index_name);
    if (!stats) {
        free(collection);
        free(index_name);
        return create_http_response(HTTP_NOT_FOUND, 
                                  "{\"error\":\"Index not found or statistics not available\"}", 
                                  "application/json");
    }
    
    /* Add collection name */
    json_object_set(stats, "collection", json_create_string(collection));
    
    /* Serialize response */
    char* response_str = json_stringify(stats);
    
    /* Free resources */
    json_free(stats);
    free(collection);
    free(index_name);
    
    /* Create HTTP response */
    http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
    
    /* Free response string */
    free(response_str);
    
    return http_response;
}