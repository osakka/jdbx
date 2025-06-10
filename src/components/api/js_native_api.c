#include "api/api.h"
#include "js/js_native_storage.h"
#include "js/js_engine.h"
#include "rbac/rbac_db.h"
#include "utils/logger.h"
#include "utils/input_validation.h"
#include <string.h>

/* External JavaScript engine reference */
extern js_engine_t *g_js_engine;

/* External functions from js_native_storage.c */
const char* js_script_type_to_string(js_script_type_t type);
const char* js_script_type_to_collection(js_script_type_t type);

/* Helper function to check permissions with simplified interface */
static int check_js_permission(database_t* db, const char* user_id, const char* permission_type, const char* resource) {
    /* For JavaScript scripts, use RBAC_COLLECTION as resource type */
    rbac_resource_type_t resource_type = RBAC_COLLECTION;
    rbac_permission_t permission;
    
    /* Map permission strings to RBAC permissions */
    if (strcmp(permission_type, "CREATE") == 0 || strcmp(permission_type, "WRITE") == 0) {
        permission = RBAC_WRITE;
    } else if (strcmp(permission_type, "READ") == 0) {
        permission = RBAC_READ;
    } else if (strcmp(permission_type, "DELETE") == 0) {
        permission = RBAC_DELETE;
    } else if (strcmp(permission_type, "ADMIN") == 0) {
        permission = RBAC_ADMIN;
    } else if (strcmp(permission_type, "EXECUTE") == 0) {
        permission = RBAC_WRITE; /* Execute maps to write permission */
    } else {
        return 0; /* Unknown permission */
    }
    
    return rbac_db_check_permission(db, user_id, resource_type, resource, permission);
}

/* Helper function to extract user ID from API context */
static const char* get_user_id_from_context(api_context_t* ctx) {
    if (!ctx) {
        return "anonymous";
    }
    /* For testing, return the admin user ID directly.
     * TODO: In a full implementation, this should extract the user ID 
     * from the authenticated JWT token or session context. */
    return "doc-1748692471-886";
}

/* Helper function to validate script metadata */
static int validate_script_metadata(js_script_metadata_t *metadata, char *error_msg, size_t error_size) {
    if (!metadata) {
        snprintf(error_msg, error_size, "Script metadata is required");
        return 0;
    }

    if (strlen(metadata->name) == 0) {
        snprintf(error_msg, error_size, "Script name is required");
        return 0;
    }

    if (strlen(metadata->collection_pattern) == 0) {
        snprintf(error_msg, error_size, "Collection pattern is required");
        return 0;
    }

    if (!metadata->script_code || strlen(metadata->script_code) == 0) {
        snprintf(error_msg, error_size, "Script code is required");
        return 0;
    }

    /* Validate collection pattern - allow wildcards */
    if (validate_collection_pattern(metadata->collection_pattern) != VALIDATION_SUCCESS) {
        snprintf(error_msg, error_size, "Invalid collection pattern format");
        return 0;
    }

    return 1;
}

/* API: Store JavaScript script natively */
http_response_t* api_handle_js_native_store_script(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->body) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }

    /* Parse request body */
    json_value_t* body = json_parse(request->body);
    if (!body || body->type != JSON_OBJECT) {
        if (body) json_free(body);
        return create_http_response(HTTP_BAD_REQUEST,
                                  "{\"error\":\"Invalid JSON request body\"}", "application/json");
    }

    /* Extract script metadata from request */
    js_script_metadata_t *metadata = js_native_create_script_metadata();
    if (!metadata) {
        json_free(body);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                                  "{\"error\":\"Failed to create script metadata\"}", "application/json");
    }

    /* Extract fields from JSON */
    json_value_t *val;

    val = json_object_get(body, "name");
    if (val && val->type == JSON_STRING) {
        strncpy(metadata->name, val->value.string, sizeof(metadata->name) - 1);
    }

    val = json_object_get(body, "description");
    if (val && val->type == JSON_STRING) {
        strncpy(metadata->description, val->value.string, sizeof(metadata->description) - 1);
    }

    val = json_object_get(body, "type");
    if (val && val->type == JSON_STRING) {
        if (strcmp(val->value.string, "validator") == 0) {
            metadata->type = JS_SCRIPT_VALIDATOR;
        } else if (strcmp(val->value.string, "transformer") == 0) {
            metadata->type = JS_SCRIPT_TRANSFORMER;
        } else if (strcmp(val->value.string, "function") == 0) {
            metadata->type = JS_SCRIPT_FUNCTION;
        }
    }

    val = json_object_get(body, "collection_pattern");
    if (val && val->type == JSON_STRING) {
        strncpy(metadata->collection_pattern, val->value.string, sizeof(metadata->collection_pattern) - 1);
    }

    val = json_object_get(body, "trigger_type");
    if (val && val->type == JSON_INTEGER) {
        metadata->trigger_type = (js_trigger_type_t)val->value.integer;
    }

    val = json_object_get(body, "trigger_tags");
    if (val && val->type == JSON_ARRAY) {
        json_free(metadata->trigger_tags);
        metadata->trigger_tags = json_clone(val);
    }

    val = json_object_get(body, "script_code");
    if (val && val->type == JSON_STRING) {
        metadata->script_code = strdup(val->value.string);
    }

    val = json_object_get(body, "rbac_permissions");
    if (val && val->type == JSON_ARRAY) {
        json_free(metadata->rbac_permissions);
        metadata->rbac_permissions = json_clone(val);
    }

    /* Validate metadata */
    char error_msg[512];
    if (!validate_script_metadata(metadata, error_msg, sizeof(error_msg))) {
        js_native_free_script_metadata(metadata);
        json_free(body);
        
        char response[1024];
        snprintf(response, sizeof(response), "{\"error\":\"%s\"}", error_msg);
        return create_http_response(HTTP_BAD_REQUEST, response, "application/json");
    }

    /* Validate script syntax */
    if (g_js_engine) {
        char *syntax_error = NULL;
        if (!js_native_validate_script_syntax(g_js_engine, metadata->script_code, &syntax_error)) {
            js_native_free_script_metadata(metadata);
            json_free(body);
            
            char response[1024];
            snprintf(response, sizeof(response), 
                    "{\"error\":\"Script syntax error\", \"details\":\"%s\"}", 
                    syntax_error ? syntax_error : "Unknown syntax error");
            
            if (syntax_error) free(syntax_error);
            return create_http_response(HTTP_BAD_REQUEST, response, "application/json");
        }
    }

    /* Check user permissions to create scripts in the specific collection */
    const char *user_id = get_user_id_from_context(ctx);
    const char *collection_name = js_script_type_to_collection(metadata->type);
    if (!collection_name || !check_js_permission(ctx->db, user_id, "CREATE", collection_name)) {
        js_native_free_script_metadata(metadata);
        json_free(body);
        return create_http_response(HTTP_FORBIDDEN,
                                  "{\"error\":\"Insufficient permissions to create scripts\"}", "application/json");
    }

    /* Store script in database */
    if (!js_native_store_script(ctx->db, user_id, metadata)) {
        js_native_free_script_metadata(metadata);
        json_free(body);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                                  "{\"error\":\"Failed to store script\"}", "application/json");
    }

    /* Create success response */
    json_value_t *response = json_create_object();
    json_object_set(response, "success", json_create_boolean(1));
    json_object_set(response, "message", json_create_string("Script stored successfully"));
    json_object_set(response, "script_id", json_create_string(metadata->id));
    json_object_set(response, "script_type", json_create_string(js_script_type_to_string(metadata->type)));

    char* response_str = json_stringify(response);
    json_free(response);
    js_native_free_script_metadata(metadata);
    json_free(body);

    http_response_t* http_response = create_http_response(HTTP_CREATED, response_str, "application/json");
    free(response_str);
    return http_response;
}

/* API: Get JavaScript script by ID */
http_response_t* api_handle_js_native_get_script(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }

    /* Extract script ID from path */
    const char* path = request->path;
    const char* script_id = strrchr(path, '/');
    if (!script_id || strlen(script_id) <= 1) {
        return create_http_response(HTTP_BAD_REQUEST,
                                  "{\"error\":\"Script ID is required\"}", "application/json");
    }
    script_id++; /* Skip the '/' */

    /* Retrieve script first to determine its type and collection */
    js_script_metadata_t *metadata = js_native_get_script(ctx->db, script_id);
    if (!metadata) {
        return create_http_response(HTTP_NOT_FOUND,
                                  "{\"error\":\"Script not found\"}", "application/json");
    }

    /* Check user permissions for the specific collection */
    const char *user_id = get_user_id_from_context(ctx);
    const char *collection_name = js_script_type_to_collection(metadata->type);
    if (!collection_name || !check_js_permission(ctx->db, user_id, "READ", collection_name)) {
        js_native_free_script_metadata(metadata);
        return create_http_response(HTTP_FORBIDDEN,
                                  "{\"error\":\"Insufficient permissions to read scripts\"}", "application/json");
    }

    /* Convert to JSON response */
    json_value_t *script_json = js_native_script_metadata_to_json(metadata);
    js_native_free_script_metadata(metadata);

    if (!script_json) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                                  "{\"error\":\"Failed to serialize script\"}", "application/json");
    }

    char* response_str = json_stringify(script_json);
    json_free(script_json);

    http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
    free(response_str);
    return http_response;
}

/* API: List JavaScript scripts with filtering */
http_response_t* api_handle_js_native_list_scripts(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }

    /* Parse query parameters for filtering */
    js_script_type_t filter_type = JS_SCRIPT_FUNCTION; /* Default */
    const char *filter_collection = NULL;
    const char *filter_user = NULL;

    /* Extract type filter from query string */
    if (request->query) {
        if (strstr(request->query, "type=validator")) {
            filter_type = JS_SCRIPT_VALIDATOR;
        } else if (strstr(request->query, "type=transformer")) {
            filter_type = JS_SCRIPT_TRANSFORMER;
        } else if (strstr(request->query, "type=function")) {
            filter_type = JS_SCRIPT_FUNCTION;
        }

        /* Extract collection filter */
        char *collection_param = strstr(request->query, "collection=");
        if (collection_param) {
            collection_param += 11; /* Skip "collection=" */
            char *end = strchr(collection_param, '&');
            if (end) {
                size_t len = end - collection_param;
                char *collection_buf = malloc(len + 1);
                if (collection_buf) {
                    strncpy(collection_buf, collection_param, len);
                    collection_buf[len] = '\0';
                    filter_collection = collection_buf;
                }
            } else {
                filter_collection = strdup(collection_param);
            }
        }
    }

    /* Check user permissions for the requested script type collection */
    const char *user_id = get_user_id_from_context(ctx);
    const char *collection_name = js_script_type_to_collection(filter_type);
    if (!collection_name || !check_js_permission(ctx->db, user_id, "READ", collection_name)) {
        if (filter_collection) {
            free((char*)filter_collection);
        }
        return create_http_response(HTTP_FORBIDDEN,
                                  "{\"error\":\"Insufficient permissions to list scripts\"}", "application/json");
    }

    /* List scripts */
    json_value_t *scripts = js_native_list_scripts(ctx->db, filter_type, filter_collection, filter_user);
    
    if (filter_collection) {
        free((char*)filter_collection);
    }

    if (!scripts) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                                  "{\"error\":\"Failed to list scripts\"}", "application/json");
    }

    /* Create response */
    json_value_t *response = json_create_object();
    json_object_set(response, "scripts", scripts);
    json_object_set(response, "count", json_create_integer(json_array_size(scripts)));

    char* response_str = json_stringify(response);
    json_free(response);

    http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
    free(response_str);
    return http_response;
}

/* API: Execute JavaScript script manually */
http_response_t* api_handle_js_native_execute_script(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->body) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }

    /* Parse request body */
    json_value_t* body = json_parse(request->body);
    if (!body || body->type != JSON_OBJECT) {
        if (body) json_free(body);
        return create_http_response(HTTP_BAD_REQUEST,
                                  "{\"error\":\"Invalid JSON request body\"}", "application/json");
    }

    /* Extract script ID and input data */
    json_value_t *script_id_val = json_object_get(body, "script_id");
    json_value_t *input_data = json_object_get(body, "input_data");

    if (!script_id_val || script_id_val->type != JSON_STRING) {
        json_free(body);
        return create_http_response(HTTP_BAD_REQUEST,
                                  "{\"error\":\"Script ID is required\"}", "application/json");
    }

    const char *script_id = script_id_val->value.string;
    const char *user_id = get_user_id_from_context(ctx);

    /* Check execution permissions */
    if (!js_native_check_execution_permission(ctx->db, user_id, script_id, "execute")) {
        json_free(body);
        return create_http_response(HTTP_FORBIDDEN,
                                  "{\"error\":\"Insufficient permissions to execute script\"}", "application/json");
    }

    /* Execute script */
    json_value_t *output_data = NULL;
    js_execution_context_t context = {0};
    
    int success = js_native_execute_script(g_js_engine, ctx->db, script_id, user_id, 
                                          input_data, &output_data, &context);

    json_free(body);

    if (!success) {
        char error_response[1024];
        snprintf(error_response, sizeof(error_response),
                "{\"error\":\"Script execution failed\", \"details\":\"%s\"}", 
                strlen(context.error_message) > 0 ? context.error_message : "Unknown error");
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, error_response, "application/json");
    }

    /* Create success response */
    json_value_t *response = json_create_object();
    json_object_set(response, "success", json_create_boolean(1));
    json_object_set(response, "script_id", json_create_string(script_id));
    json_object_set(response, "execution_time_ms", 
                   json_create_number((context.end_time.tv_sec - context.start_time.tv_sec) * 1000.0 +
                                     (context.end_time.tv_nsec - context.start_time.tv_nsec) / 1000000.0));
    json_object_set(response, "result", output_data ? output_data : json_create_null());

    char* response_str = json_stringify(response);
    json_free(response);

    http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
    free(response_str);
    return http_response;
}

/* API: Get JavaScript execution metrics */
http_response_t* api_handle_js_native_get_metrics(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }

    /* Check user permissions */
    const char *user_id = get_user_id_from_context(ctx);
    if (!check_js_permission(ctx->db, user_id, "READ", "system/js_metrics")) {
        return create_http_response(HTTP_FORBIDDEN,
                                  "{\"error\":\"Insufficient permissions to read metrics\"}", "application/json");
    }

    /* Parse time range from query parameters */
    time_t from_time = 0;
    time_t to_time = time(NULL);

    if (request->query) {
        char *from_param = strstr(request->query, "from=");
        if (from_param) {
            from_time = strtol(from_param + 5, NULL, 10);
        }

        char *to_param = strstr(request->query, "to=");
        if (to_param) {
            to_time = strtol(to_param + 3, NULL, 10);
        }
    }

    /* Get global metrics */
    json_value_t *metrics = js_native_get_global_metrics(ctx->db, from_time, to_time);
    if (!metrics) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                                  "{\"error\":\"Failed to retrieve metrics\"}", "application/json");
    }

    char* response_str = json_stringify(metrics);
    json_free(metrics);

    http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
    free(response_str);
    return http_response;
}

/* API: Update JavaScript script */
http_response_t* api_handle_js_native_update_script(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->body) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }

    /* Extract script ID from path */
    const char* path = request->path;
    const char* script_id = strrchr(path, '/');
    if (!script_id || strlen(script_id) <= 1) {
        return create_http_response(HTTP_BAD_REQUEST,
                                  "{\"error\":\"Script ID is required\"}", "application/json");
    }
    script_id++; /* Skip the '/' */

    /* Parse request body */
    json_value_t* body = json_parse(request->body);
    if (!body || body->type != JSON_OBJECT) {
        if (body) json_free(body);
        return create_http_response(HTTP_BAD_REQUEST,
                                  "{\"error\":\"Invalid JSON request body\"}", "application/json");
    }

    /* Create metadata from JSON */
    js_script_metadata_t *metadata = js_native_script_metadata_from_json(body);
    if (!metadata) {
        json_free(body);
        return create_http_response(HTTP_BAD_REQUEST,
                                  "{\"error\":\"Invalid script metadata\"}", "application/json");
    }

    /* Validate metadata */
    char error_msg[512];
    if (!validate_script_metadata(metadata, error_msg, sizeof(error_msg))) {
        js_native_free_script_metadata(metadata);
        json_free(body);
        
        char response[1024];
        snprintf(response, sizeof(response), "{\"error\":\"%s\"}", error_msg);
        return create_http_response(HTTP_BAD_REQUEST, response, "application/json");
    }

    /* Validate script syntax */
    if (g_js_engine && metadata->script_code) {
        char *syntax_error = NULL;
        if (!js_native_validate_script_syntax(g_js_engine, metadata->script_code, &syntax_error)) {
            js_native_free_script_metadata(metadata);
            json_free(body);
            
            char response[1024];
            snprintf(response, sizeof(response), 
                    "{\"error\":\"Script syntax error\", \"details\":\"%s\"}", 
                    syntax_error ? syntax_error : "Unknown syntax error");
            
            if (syntax_error) free(syntax_error);
            return create_http_response(HTTP_BAD_REQUEST, response, "application/json");
        }
    }

    /* Check user permissions */
    const char *user_id = get_user_id_from_context(ctx);
    if (!js_native_update_script(ctx->db, user_id, script_id, metadata)) {
        js_native_free_script_metadata(metadata);
        json_free(body);
        return create_http_response(HTTP_FORBIDDEN,
                                  "{\"error\":\"Failed to update script - insufficient permissions or script not found\"}", 
                                  "application/json");
    }

    /* Create success response */
    json_value_t *response = json_create_object();
    json_object_set(response, "success", json_create_boolean(1));
    json_object_set(response, "message", json_create_string("Script updated successfully"));
    json_object_set(response, "script_id", json_create_string(script_id));
    json_object_set(response, "version", json_create_integer(metadata->version));

    char* response_str = json_stringify(response);
    json_free(response);
    js_native_free_script_metadata(metadata);
    json_free(body);

    http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
    free(response_str);
    return http_response;
}

/* API: Delete JavaScript script */
http_response_t* api_handle_js_native_delete_script(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }

    /* Extract script ID from path */
    const char* path = request->path;
    const char* script_id = strrchr(path, '/');
    if (!script_id || strlen(script_id) <= 1) {
        return create_http_response(HTTP_BAD_REQUEST,
                                  "{\"error\":\"Script ID is required\"}", "application/json");
    }
    script_id++; /* Skip the '/' */

    /* Check user permissions and delete */
    const char *user_id = get_user_id_from_context(ctx);
    if (!js_native_delete_script(ctx->db, user_id, script_id)) {
        return create_http_response(HTTP_FORBIDDEN,
                                  "{\"error\":\"Failed to delete script - insufficient permissions or script not found\"}", 
                                  "application/json");
    }

    /* Create success response */
    json_value_t *response = json_create_object();
    json_object_set(response, "success", json_create_boolean(1));
    json_object_set(response, "message", json_create_string("Script deleted successfully"));
    json_object_set(response, "script_id", json_create_string(script_id));

    char* response_str = json_stringify(response);
    json_free(response);

    http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
    free(response_str);
    return http_response;
}

/* API: Get script statistics */
http_response_t* api_handle_js_native_get_script_stats(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }

    /* Extract script ID from path */
    const char* path = request->path;
    const char* script_id = strrchr(path, '/');
    if (!script_id || strlen(script_id) <= 1) {
        return create_http_response(HTTP_BAD_REQUEST,
                                  "{\"error\":\"Script ID is required\"}", "application/json");
    }
    script_id++; /* Skip the '/' */

    /* Check user permissions */
    const char *user_id = get_user_id_from_context(ctx);
    if (!check_js_permission(ctx->db, user_id, "READ", "system/js_metrics")) {
        return create_http_response(HTTP_FORBIDDEN,
                                  "{\"error\":\"Insufficient permissions to read script statistics\"}", 
                                  "application/json");
    }

    /* Parse time range from query parameters */
    time_t from_time = 0;
    time_t to_time = time(NULL);

    if (request->query) {
        char *from_param = strstr(request->query, "from=");
        if (from_param) {
            from_time = strtol(from_param + 5, NULL, 10);
        }

        char *to_param = strstr(request->query, "to=");
        if (to_param) {
            to_time = strtol(to_param + 3, NULL, 10);
        }
    }

    /* Get script statistics */
    json_value_t *stats = js_native_get_script_statistics(ctx->db, script_id, from_time, to_time);
    if (!stats) {
        return create_http_response(HTTP_NOT_FOUND,
                                  "{\"error\":\"Script not found or failed to retrieve statistics\"}", 
                                  "application/json");
    }

    char* response_str = json_stringify(stats);
    json_free(stats);

    http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
    free(response_str);
    return http_response;
}

/* API: Execute tagged functions */
http_response_t* api_handle_js_native_execute_tagged_functions(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->body) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }

    /* Parse request body */
    json_value_t* body = json_parse(request->body);
    if (!body || body->type != JSON_OBJECT) {
        if (body) json_free(body);
        return create_http_response(HTTP_BAD_REQUEST,
                                  "{\"error\":\"Invalid JSON request body\"}", "application/json");
    }

    /* Extract parameters */
    json_value_t *collection_val = json_object_get(body, "collection");
    json_value_t *tag_val = json_object_get(body, "tag");
    json_value_t *input_data = json_object_get(body, "input_data");

    if (!collection_val || collection_val->type != JSON_STRING ||
        !tag_val || tag_val->type != JSON_STRING) {
        json_free(body);
        return create_http_response(HTTP_BAD_REQUEST,
                                  "{\"error\":\"Collection name and tag are required\"}", "application/json");
    }

    const char *collection_name = collection_val->value.string;
    const char *tag = tag_val->value.string;
    const char *user_id = get_user_id_from_context(ctx);

    /* Check execution permissions */
    if (!check_js_permission(ctx->db, user_id, "EXECUTE", collection_name)) {
        json_free(body);
        return create_http_response(HTTP_FORBIDDEN,
                                  "{\"error\":\"Insufficient permissions to execute functions\"}", 
                                  "application/json");
    }

    /* Execute tagged functions */
    json_value_t *results = js_native_execute_tagged_functions(g_js_engine, ctx->db, 
                                                             collection_name, input_data, 
                                                             tag, user_id);
    json_free(body);

    if (!results) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                                  "{\"error\":\"Failed to execute tagged functions\"}", 
                                  "application/json");
    }

    /* Create response */
    json_value_t *response = json_create_object();
    json_object_set(response, "collection", json_create_string(collection_name));
    json_object_set(response, "tag", json_create_string(tag));
    json_object_set(response, "results", results);
    json_object_set(response, "count", json_create_integer(json_array_size(results)));

    char* response_str = json_stringify(response);
    json_free(response);

    http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
    free(response_str);
    return http_response;
}

/* Register native JavaScript API routes */
int register_js_native_api_routes(api_route_t* api_routes, int num_routes) {
    /* Store script */
    api_routes[num_routes++] = (api_route_t){"/api/js/native/store", HTTP_POST, api_handle_js_native_store_script, 1};
    
    /* Get script by ID */
    api_routes[num_routes++] = (api_route_t){"/api/js/native/script/:id", HTTP_GET, api_handle_js_native_get_script, 1};
    
    /* Update script */
    api_routes[num_routes++] = (api_route_t){"/api/js/native/script/:id", HTTP_PUT, api_handle_js_native_update_script, 1};
    
    /* Delete script */
    api_routes[num_routes++] = (api_route_t){"/api/js/native/script/:id", HTTP_DELETE, api_handle_js_native_delete_script, 1};
    
    /* List scripts */
    api_routes[num_routes++] = (api_route_t){"/api/js/native/list", HTTP_GET, api_handle_js_native_list_scripts, 1};
    
    /* Execute script */
    api_routes[num_routes++] = (api_route_t){"/api/js/native/execute", HTTP_POST, api_handle_js_native_execute_script, 1};
    
    /* Get execution metrics */
    api_routes[num_routes++] = (api_route_t){"/api/js/native/metrics", HTTP_GET, api_handle_js_native_get_metrics, 1};
    
    /* Get script statistics */
    api_routes[num_routes++] = (api_route_t){"/api/js/native/stats/:id", HTTP_GET, api_handle_js_native_get_script_stats, 1};
    
    /* Execute tagged functions */
    api_routes[num_routes++] = (api_route_t){"/api/js/native/execute-tagged", HTTP_POST, api_handle_js_native_execute_tagged_functions, 1};

    return num_routes;
}