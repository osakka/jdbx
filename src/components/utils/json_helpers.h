#ifndef JSON_HELPERS_H
#define JSON_HELPERS_H

#include "utils/json.h"
#include "core/server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Get all keys from a JSON object and store them in the given array.
 *
 * @param obj The JSON object
 * @param keys Array to store the object keys (must be pre-allocated)
 * @param max_keys Maximum number of keys to retrieve (size of keys array)
 * @return Number of keys retrieved
 */
int json_object_keys(json_value_t* obj, char** keys, int max_keys);

/**
 * ===========================================================================
 * JSON Response Helpers - Functions for creating HTTP responses from JSON
 * ===========================================================================
 */

/**
 * Helper function to create an HTTP response from a JSON string.
 * Note: This function does NOT take ownership of json_str.
 * 
 * @param json_str JSON string (not modified)
 * @param status_code HTTP status code
 * @return HTTP response
 */
static inline http_response_t* http_response_json_string(const char* json_str, int status_code) {
    if (!json_str) {
        return http_response_error("Invalid JSON string", 500);
    }
    
    http_response_t* response = (http_response_t*)malloc(sizeof(http_response_t));
    if (!response) {
        return NULL;
    }
    
    response->status = status_code;
    response->body = strdup(json_str);
    response->content_type = strdup("application/json");
    response->content_length = strlen(json_str);
    response->headers = NULL;
    response->num_headers = 0;
    
    return response;
}

/**
 * Helper for creating a http_response_json from stringified JSON.
 * This should be used when we've already stringified the JSON and need to
 * parse it back to a JSON value for creating the response.
 * IMPORTANT: The function takes ownership of json_str and frees it.
 * 
 * @param json_str JSON string (will be freed by this function)
 * @param status_code HTTP status code
 * @return HTTP response
 */
static inline http_response_t* http_response_from_json_string(char* json_str, int status_code) {
    if (!json_str) {
        return http_response_error("Invalid JSON data", 500);
    }
    
    /* Parse the JSON string back to a json_value_t */
    json_value_t* json = json_parse(json_str);
    if (!json) {
        free(json_str);
        return http_response_error("Failed to parse JSON", 500);
    }
    
    /* Create response using the standard function */
    http_response_t* response = http_response_json(json, status_code);
    
    /* Free temporary objects */
    json_free(json);
    free(json_str);
    
    return response;
}

/**
 * Create a JSON success response with status message and data.
 * Takes ownership of the data object and frees it.
 * 
 * @param message Success message
 * @param data JSON data (ownership is taken, will be freed)
 * @param status_code HTTP status code (defaults to 200)
 * @return HTTP response
 */
static inline http_response_t* http_response_success_with_data(const char* message, json_value_t* data, int status_code) {
    if (!data) {
        return http_response_error("Invalid data", 500);
    }
    
    json_value_t* response_obj = json_create_object();
    if (!response_obj) {
        json_free(data);
        return http_response_error("Failed to create response", 500);
    }
    
    json_object_set(response_obj, "status", json_create_string("success"));
    
    if (message) {
        json_object_set(response_obj, "message", json_create_string(message));
    }
    
    json_object_set(response_obj, "data", data);  /* Takes ownership of data */
    
    http_response_t* response = http_response_json(response_obj, status_code);
    
    json_free(response_obj);
    
    return response;
}

/**
 * Create a JSON error response with user-friendly error message and optional details.
 * 
 * @param message Error message (user-friendly)
 * @param error_code Optional error code (can be NULL)
 * @param status_code HTTP status code
 * @return HTTP response
 */
static inline http_response_t* http_response_error_detailed(const char* message, const char* error_code, int status_code) {
    json_value_t* error_obj = json_create_object();
    if (!error_obj) {
        return http_response_error("Internal server error", 500);
    }
    
    json_object_set(error_obj, "status", json_create_string("error"));
    json_object_set(error_obj, "message", json_create_string(message ? message : "Unknown error"));
    
    if (error_code) {
        json_object_set(error_obj, "code", json_create_string(error_code));
    }
    
    http_response_t* response = http_response_json(error_obj, status_code);
    
    json_free(error_obj);
    
    return response;
}

/**
 * ===========================================================================
 * JSON Parsing Helpers - Functions for working with JSON strings
 * ===========================================================================
 */

/**
 * Helper function to parse a JSON string into a JSON value
 * 
 * @param json_str JSON string
 * @return JSON value or NULL on error
 */
static inline json_value_t* json_parse_string(const char* json_str) {
    if (!json_str) {
        return NULL;
    }
    
    return json_parse(json_str);
}

/**
 * Parse JSON from HTTP request body.
 * 
 * @param request HTTP request containing JSON body
 * @return Parsed JSON value or NULL on error
 */
static inline json_value_t* json_parse_request_body(http_request_t* request) {
    if (!request || !request->body) {
        return NULL;
    }
    
    return json_parse_string(request->body);
}

/**
 * Get a string value from a JSON object with validation.
 * 
 * @param object JSON object
 * @param key Key to retrieve
 * @param required Whether the value is required (if true and not found, returns NULL)
 * @return String value or NULL if not found or wrong type
 */
static inline const char* json_get_string_value(json_value_t* object, const char* key, int required) {
    if (!object || !key) {
        return NULL;
    }
    
    json_value_t* value = json_object_get(object, key);
    if (!value) {
        return required ? NULL : "";
    }
    
    if (json_get_type(value) != JSON_STRING) {
        return required ? NULL : "";
    }
    
    return json_get_string(value);
}

/**
 * Get a number value from a JSON object with validation.
 * 
 * @param object JSON object
 * @param key Key to retrieve
 * @param default_value Value to return if not found or wrong type
 * @return Number value or default_value if not found or wrong type
 */
static inline double json_get_number_value(json_value_t* object, const char* key, double default_value) {
    if (!object || !key) {
        return default_value;
    }
    
    json_value_t* value = json_object_get(object, key);
    if (!value) {
        return default_value;
    }
    
    if (json_get_type(value) != JSON_NUMBER) {
        return default_value;
    }
    
    return json_get_number(value);
}

/**
 * Get an integer value from a JSON object with validation.
 * 
 * @param object JSON object
 * @param key Key to retrieve
 * @param default_value Value to return if not found or wrong type
 * @return Integer value or default_value if not found or wrong type
 */
static inline int json_get_int_value(json_value_t* object, const char* key, int default_value) {
    return (int)json_get_number_value(object, key, default_value);
}

/**
 * Get a boolean value from a JSON object with validation.
 * 
 * @param object JSON object
 * @param key Key to retrieve
 * @param default_value Value to return if not found or wrong type
 * @return Boolean value or default_value if not found or wrong type
 */
static inline int json_get_bool_value(json_value_t* object, const char* key, int default_value) {
    if (!object || !key) {
        return default_value;
    }
    
    json_value_t* value = json_object_get(object, key);
    if (!value) {
        return default_value;
    }
    
    if (json_get_type(value) != JSON_BOOLEAN) {
        return default_value;
    }
    
    return json_get_boolean(value);
}

/**
 * Validate required fields in a JSON request object.
 * 
 * @param object JSON object to validate
 * @param required_fields Array of required field names
 * @param field_count Number of required fields
 * @param error_field Output parameter for the name of the missing field
 * @return 1 if all required fields are present and valid, 0 otherwise
 */
static inline int json_validate_required_fields(json_value_t* object, 
                                               const char** required_fields, 
                                               int field_count,
                                               const char** error_field) {
    if (!object || !required_fields || field_count <= 0) {
        return 0;
    }
    
    for (int i = 0; i < field_count; i++) {
        const char* field = required_fields[i];
        json_value_t* value = json_object_get(object, field);
        
        if (!value) {
            if (error_field) {
                *error_field = field;
            }
            return 0;
        }
    }
    
    return 1;
}

#endif /* JSON_HELPERS_H */