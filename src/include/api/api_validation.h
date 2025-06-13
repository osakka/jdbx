#ifndef JDBX_API_VALIDATION_H
#define JDBX_API_VALIDATION_H

/**
 * @file api_validation.h
 * @brief Input validation for API requests
 *
 * This header provides utility functions for validating API request inputs
 * using the input validation framework.
 */

#include "api/api.h"
#include "utils/json.h"
#include <stddef.h>
#include <stdint.h>

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
                                             size_t collection_name_size);

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
                                         size_t document_id_size);

/**
 * Validate a JSON request body
 * 
 * @param request The HTTP request
 * @param[out] json Pointer to store the parsed JSON (if successful)
 * @return NULL on success, error response on failure
 */
http_response_t* api_validate_json_body(http_request_t* request, json_value_t** json);

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
                                         const char** value_out);

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
                                         double* value_out);

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
                                          int64_t* value_out);

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
                                      const char** value_out);

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
                                        const char** value_out);

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
                                          int* value_out);

#endif /* JDBX_API_VALIDATION_H */