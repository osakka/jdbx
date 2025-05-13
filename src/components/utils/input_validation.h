#ifndef JSONDB_INPUT_VALIDATION_H
#define JSONDB_INPUT_VALIDATION_H

/**
 * @file input_validation.h
 * @brief Input validation utilities for JSONdb
 *
 * This header provides functions for validating user inputs to prevent
 * security vulnerabilities like SQL injection, path traversal, and XSS.
 */

#include <stddef.h>
#include <stdint.h>
#include "components/utils/json.h"

/**
 * @brief Validation error codes
 */
typedef enum {
    VALIDATION_SUCCESS = 0,           /**< Validation succeeded */
    VALIDATION_ERROR_NULL_INPUT,      /**< Null input provided */
    VALIDATION_ERROR_EMPTY_INPUT,     /**< Empty input provided */
    VALIDATION_ERROR_TOO_LONG,        /**< Input exceeds maximum length */
    VALIDATION_ERROR_INVALID_CHARS,   /**< Input contains invalid characters */
    VALIDATION_ERROR_PATTERN_MATCH,   /**< Input matches a forbidden pattern */
    VALIDATION_ERROR_INVALID_FORMAT,  /**< Input has invalid format */
    VALIDATION_ERROR_OUT_OF_RANGE,    /**< Numeric input is out of range */
    VALIDATION_ERROR_PATH_TRAVERSAL,  /**< Input contains path traversal sequences */
    VALIDATION_ERROR_JSON_FORMAT,     /**< Invalid JSON format */
    VALIDATION_ERROR_UNKNOWN          /**< Unknown validation error */
} validation_error_t;

/**
 * @brief Get a string description of a validation error
 *
 * @param error Validation error code
 * @return String description of the error
 */
const char* validation_error_string(validation_error_t error);

/**
 * @brief Validate a string based on allowed character set and length
 *
 * @param input String to validate
 * @param min_len Minimum allowed length (0 for no minimum)
 * @param max_len Maximum allowed length
 * @param allowed_chars String containing all allowed characters, or NULL for any printable
 * @return VALIDATION_SUCCESS on success, error code otherwise
 */
validation_error_t validate_string(const char* input, size_t min_len, size_t max_len, const char* allowed_chars);

/**
 * @brief Validate an alphanumeric string
 *
 * @param input String to validate
 * @param min_len Minimum allowed length (0 for no minimum)
 * @param max_len Maximum allowed length
 * @return VALIDATION_SUCCESS on success, error code otherwise
 */
validation_error_t validate_alphanumeric(const char* input, size_t min_len, size_t max_len);

/**
 * @brief Validate an identifier (alphanumeric plus underscore, starting with letter)
 *
 * @param input String to validate
 * @param min_len Minimum allowed length (0 for no minimum)
 * @param max_len Maximum allowed length
 * @return VALIDATION_SUCCESS on success, error code otherwise
 */
validation_error_t validate_identifier(const char* input, size_t min_len, size_t max_len);

/**
 * @brief Validate a file path for path traversal attacks
 *
 * @param input Path to validate
 * @return VALIDATION_SUCCESS on success, error code otherwise
 */
validation_error_t validate_path(const char* input);

/**
 * @brief Validate a number is within range
 *
 * @param value Number to validate
 * @param min_value Minimum allowed value
 * @param max_value Maximum allowed value
 * @return VALIDATION_SUCCESS on success, error code otherwise
 */
validation_error_t validate_int_range(int64_t value, int64_t min_value, int64_t max_value);

/**
 * @brief Validate a floating point number is within range
 *
 * @param value Number to validate
 * @param min_value Minimum allowed value
 * @param max_value Maximum allowed value
 * @return VALIDATION_SUCCESS on success, error code otherwise
 */
validation_error_t validate_double_range(double value, double min_value, double max_value);

/**
 * @brief Validate JSON string format
 *
 * @param json_str String containing JSON to validate
 * @param json_len Length of the JSON string
 * @param[out] result Optional pointer to receive parsed JSON (NULL to just validate)
 * @return VALIDATION_SUCCESS on success, error code otherwise
 */
validation_error_t validate_json(const char* json_str, size_t json_len, json_value_t** result);

/**
 * @brief Validate a URL format
 *
 * @param url URL to validate
 * @return VALIDATION_SUCCESS on success, error code otherwise
 */
validation_error_t validate_url(const char* url);

/**
 * @brief Validate an email address format
 *
 * @param email Email address to validate
 * @return VALIDATION_SUCCESS on success, error code otherwise
 */
validation_error_t validate_email(const char* email);

/**
 * @brief Validate an IP address format
 *
 * @param ip IP address to validate
 * @return VALIDATION_SUCCESS on success, error code otherwise
 */
validation_error_t validate_ip_address(const char* ip);

/**
 * @brief Sanitize a string for safe output (e.g., prevents XSS)
 *
 * @param input String to sanitize
 * @param output Buffer to store sanitized string
 * @param output_size Size of the output buffer
 * @return 0 on success, non-zero on error
 */
int sanitize_string(const char* input, char* output, size_t output_size);

/**
 * @brief Sanitize a database collection name
 *
 * @param input Collection name to sanitize
 * @param output Buffer to store sanitized name
 * @param output_size Size of the output buffer
 * @return 0 on success, non-zero on error
 */
int sanitize_collection_name(const char* input, char* output, size_t output_size);

/**
 * @brief Sanitize a database document ID
 *
 * @param input Document ID to sanitize
 * @param output Buffer to store sanitized ID
 * @param output_size Size of the output buffer
 * @return 0 on success, non-zero on error
 */
int sanitize_document_id(const char* input, char* output, size_t output_size);

/**
 * @brief Validate a database collection name
 *
 * @param name Collection name to validate
 * @return VALIDATION_SUCCESS on success, error code otherwise
 */
validation_error_t validate_collection_name(const char* name);

/**
 * @brief Validate a database document ID
 *
 * @param id Document ID to validate
 * @return VALIDATION_SUCCESS on success, error code otherwise
 */
validation_error_t validate_document_id(const char* id);

#endif /* JSONDB_INPUT_VALIDATION_H */