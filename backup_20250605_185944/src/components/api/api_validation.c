#include "api/api.h"
#include "utils/input_validation.h"
#include "utils/logger.h"
#include "utils/json.h"
#include "utils/json_helpers.h"
#include <string.h>
#include <stdlib.h>

/* Error response helper for validation errors */
static http_response_t* validation_error_response(validation_error_t error, const char* field) {
  const char* error_message = validation_error_string(error);
  char response_message[256];
  
  if (field) {
    snprintf(response_message, sizeof(response_message), 
         "Validation error in field '%s': %s", field, error_message);
  } else {
    snprintf(response_message, sizeof(response_message), 
         "Validation error: %s", error_message);
  }
  
  json_value_t* response = json_create_object();
  json_object_set(response, "status", json_create_string("error"));
  json_object_set(response, "message", json_create_string(response_message));

  http_response_t* http_response = http_response_json(response, 400);
  json_free(response);
  
  return http_response;
}

/**
 * Validate a collection name from the request
 * 
 * @param request The HTTP request
 * @param name_param The parameter name containing the collection name (or NULL to use URL path)
 * @param[out] collection_name Buffer to store validated collection name
 * @param collection_name_size Size of the collection name buffer
 * @return NULL on success, error response on failure
 */
http_response_t* api_validate_collection_name(http_request_t* request, 
                       const char* name_param, 
                       char* collection_name, 
                       size_t collection_name_size) {
  const char* name = NULL;
  
  if (name_param) {
    /* Would use http_request_get_param or similar - for now we can only use URL path */
    name = NULL; /* Need to implement proper query parameter extraction */
  } else {
    /* Extract from URL path - assuming /api/collection/:name pattern */
    const char* path = request->path;
    const char* collection_part = strstr(path, "/collection/");
    
    if (collection_part) {
      name = collection_part + strlen("/collection/");
    } else {
      /* Try collections endpoint */
      collection_part = strstr(path, "/collections/");
      if (collection_part) {
        name = collection_part + strlen("/collections/");
      }
    }
  }
  
  if (!name || strlen(name) == 0) {
    return validation_error_response(VALIDATION_ERROR_EMPTY_INPUT, "collection_name");
  }
  
  validation_error_t result = validate_collection_name(name);
  if (result != VALIDATION_SUCCESS) {
    return validation_error_response(result, "collection_name");
  }
  
  /* Copy the validated name to the output buffer */
  if (collection_name && collection_name_size > 0) {
    strncpy(collection_name, name, collection_name_size - 1);
    collection_name[collection_name_size - 1] = '\0';
  }
  
  return NULL; /* NULL means validation successful */
}

/**
 * Validate a document ID from the request
 * 
 * @param request The HTTP request
 * @param id_param The parameter name containing the document ID (or NULL to use URL path)
 * @param[out] document_id Buffer to store validated document ID
 * @param document_id_size Size of the document ID buffer
 * @return NULL on success, error response on failure
 */
http_response_t* api_validate_document_id(http_request_t* request, 
                     const char* id_param, 
                     char* document_id, 
                     size_t document_id_size) {
  const char* id = NULL;
  
  if (id_param) {
    /* Would use http_request_get_param or similar - for now we can only use URL path */
    id = NULL; /* Need to implement proper query parameter extraction */
  } else {
    /* Extract from URL path - assuming /api/collection/:name/document/:id pattern */
    const char* path = request->path;
    const char* document_part = strstr(path, "/document/");
    
    if (document_part) {
      id = document_part + strlen("/document/");
    } else {
      /* Try documents endpoint */
      document_part = strstr(path, "/documents/");
      if (document_part) {
        id = document_part + strlen("/documents/");
      }
    }
  }
  
  if (!id || strlen(id) == 0) {
    return validation_error_response(VALIDATION_ERROR_EMPTY_INPUT, "document_id");
  }
  
  validation_error_t result = validate_document_id(id);
  if (result != VALIDATION_SUCCESS) {
    return validation_error_response(result, "document_id");
  }
  
  /* Copy the validated ID to the output buffer */
  if (document_id && document_id_size > 0) {
    strncpy(document_id, id, document_id_size - 1);
    document_id[document_id_size - 1] = '\0';
  }
  
  return NULL; /* NULL means validation successful */
}

/**
 * Validate a JSON request body
 * 
 * @param request The HTTP request
 * @param[out] json Pointer to store the parsed JSON (if successful)
 * @return NULL on success, error response on failure
 */
http_response_t* api_validate_json_body(http_request_t* request, json_value_t** json) {
  if (!request->body || request->content_length == 0) {
    return validation_error_response(VALIDATION_ERROR_EMPTY_INPUT, "request_body");
  }
  
  json_value_t* parsed_json = NULL;
  validation_error_t result = validate_json(request->body, request->content_length, &parsed_json);
  
  if (result != VALIDATION_SUCCESS) {
    return validation_error_response(result, "request_body");
  }
  
  if (json) {
    *json = parsed_json;
  } else {
    json_free(parsed_json);
  }
  
  return NULL; /* NULL means validation successful */
}

/**
 * Validate a string parameter in a JSON object
 * 
 * @param json The JSON object to validate
 * @param field The field name to validate
 * @param min_len Minimum allowed length (0 for no minimum)
 * @param max_len Maximum allowed length
 * @param allowed_chars String containing all allowed characters, or NULL for any printable
 * @param required Whether the field is required
 * @param[out] value_out Pointer to store the validated string (if successful)
 * @return NULL on success, error response on failure
 */
http_response_t* api_validate_json_string(json_value_t* json, 
                     const char* field, 
                     size_t min_len, 
                     size_t max_len, 
                     const char* allowed_chars, 
                     int required, 
                     const char** value_out) {
  json_value_t* value_obj = json_object_get(json, field);
  const char* value = value_obj ? json_get_string(value_obj) : NULL;
  
  if (!value) {
    if (required) {
      return validation_error_response(VALIDATION_ERROR_NULL_INPUT, field);
    } else {
      /* Field is optional and not present, so no validation needed */
      if (value_out) {
        *value_out = NULL;
      }
      return NULL;
    }
  }
  
  validation_error_t result = validate_string(value, min_len, max_len, allowed_chars);
  if (result != VALIDATION_SUCCESS) {
    return validation_error_response(result, field);
  }
  
  if (value_out) {
    *value_out = value;
  }
  
  return NULL; /* NULL means validation successful */
}

/**
 * Validate a numeric parameter in a JSON object
 * 
 * @param json The JSON object to validate
 * @param field The field name to validate
 * @param min_value Minimum allowed value
 * @param max_value Maximum allowed value
 * @param required Whether the field is required
 * @param[out] value_out Pointer to store the validated number (if successful)
 * @return NULL on success, error response on failure
 */
http_response_t* api_validate_json_number(json_value_t* json, 
                     const char* field, 
                     double min_value, 
                     double max_value, 
                     int required, 
                     double* value_out) {
  json_value_t* value_obj = json_object_get(json, field);
  if (!value_obj || json_get_type(value_obj) != JSON_NUMBER) {
    if (required) {
      return validation_error_response(VALIDATION_ERROR_NULL_INPUT, field);
    } else {
      /* Field is optional and not present, so no validation needed */
      return NULL;
    }
  }
  
  double value = json_get_number(value_obj);
  validation_error_t result = validate_double_range(value, min_value, max_value);
  
  if (result != VALIDATION_SUCCESS) {
    return validation_error_response(result, field);
  }
  
  if (value_out) {
    *value_out = value;
  }
  
  return NULL; /* NULL means validation successful */
}

/**
 * Validate an integer parameter in a JSON object
 * 
 * @param json The JSON object to validate
 * @param field The field name to validate
 * @param min_value Minimum allowed value
 * @param max_value Maximum allowed value
 * @param required Whether the field is required
 * @param[out] value_out Pointer to store the validated integer (if successful)
 * @return NULL on success, error response on failure
 */
http_response_t* api_validate_json_integer(json_value_t* json, 
                     const char* field, 
                     int64_t min_value, 
                     int64_t max_value, 
                     int required, 
                     int64_t* value_out) {
  json_value_t* value_obj = json_object_get(json, field);
  if (!value_obj || json_get_type(value_obj) != JSON_NUMBER) {
    if (required) {
      return validation_error_response(VALIDATION_ERROR_NULL_INPUT, field);
    } else {
      /* Field is optional and not present, so no validation needed */
      return NULL;
    }
  }
  
  double double_value = json_get_number(value_obj);
  int64_t value = (int64_t)double_value;
  
  /* Check if the value is actually an integer */
  if (value != double_value) {
    return validation_error_response(VALIDATION_ERROR_INVALID_FORMAT, field);
  }
  
  validation_error_t result = validate_int_range(value, min_value, max_value);
  
  if (result != VALIDATION_SUCCESS) {
    return validation_error_response(result, field);
  }
  
  if (value_out) {
    *value_out = value;
  }
  
  return NULL; /* NULL means validation successful */
}

/**
 * Validate a URL parameter in a JSON object
 * 
 * @param json The JSON object to validate
 * @param field The field name to validate
 * @param required Whether the field is required
 * @param[out] value_out Pointer to store the validated URL (if successful)
 * @return NULL on success, error response on failure
 */
http_response_t* api_validate_json_url(json_value_t* json, 
                   const char* field, 
                   int required, 
                   const char** value_out) {
  json_value_t* value_obj = json_object_get(json, field);
  const char* value = value_obj ? json_get_string(value_obj) : NULL;
  
  if (!value) {
    if (required) {
      return validation_error_response(VALIDATION_ERROR_NULL_INPUT, field);
    } else {
      /* Field is optional and not present, so no validation needed */
      if (value_out) {
        *value_out = NULL;
      }
      return NULL;
    }
  }
  
  validation_error_t result = validate_url(value);
  if (result != VALIDATION_SUCCESS) {
    return validation_error_response(result, field);
  }
  
  if (value_out) {
    *value_out = value;
  }
  
  return NULL; /* NULL means validation successful */
}

/**
 * Validate an email parameter in a JSON object
 * 
 * @param json The JSON object to validate
 * @param field The field name to validate
 * @param required Whether the field is required
 * @param[out] value_out Pointer to store the validated email (if successful)
 * @return NULL on success, error response on failure
 */
http_response_t* api_validate_json_email(json_value_t* json, 
                    const char* field, 
                    int required, 
                    const char** value_out) {
  json_value_t* value_obj = json_object_get(json, field);
  const char* value = value_obj ? json_get_string(value_obj) : NULL;
  
  if (!value) {
    if (required) {
      return validation_error_response(VALIDATION_ERROR_NULL_INPUT, field);
    } else {
      /* Field is optional and not present, so no validation needed */
      if (value_out) {
        *value_out = NULL;
      }
      return NULL;
    }
  }
  
  validation_error_t result = validate_email(value);
  if (result != VALIDATION_SUCCESS) {
    return validation_error_response(result, field);
  }
  
  if (value_out) {
    *value_out = value;
  }
  
  return NULL; /* NULL means validation successful */
}

/**
 * Validate a boolean parameter in a JSON object
 * 
 * @param json The JSON object to validate
 * @param field The field name to validate
 * @param required Whether the field is required
 * @param[out] value_out Pointer to store the validated boolean (if successful)
 * @return NULL on success, error response on failure
 */
http_response_t* api_validate_json_boolean(json_value_t* json, 
                     const char* field, 
                     int required, 
                     int* value_out) {
  if (!json_object_has(json, field)) {
    if (required) {
      return validation_error_response(VALIDATION_ERROR_NULL_INPUT, field);
    } else {
      /* Field is optional and not present, so no validation needed */
      return NULL;
    }
  }
  
  json_value_t* value_obj = json_object_get(json, field);
  if (!value_obj || json_get_type(value_obj) != JSON_BOOLEAN) {
    return validation_error_response(VALIDATION_ERROR_INVALID_FORMAT, field);
  }
  
  if (value_out) {
    *value_out = json_get_boolean(value_obj);
  }
  
  return NULL; /* NULL means validation successful */
}