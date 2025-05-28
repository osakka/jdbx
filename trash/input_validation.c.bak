#include "utils/input_validation.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>

/* Maximum string lengths for various types */
#define MAX_COLLECTION_NAME_LENGTH 128
#define MAX_DOCUMENT_ID_LENGTH 256
#define MAX_PATH_LENGTH 1024
#define MAX_URL_LENGTH 2048
#define MAX_EMAIL_LENGTH 256
#define MAX_IP_LENGTH 45  /* IPv6 addresses can be up to 45 chars */

/* Helper function to check if a character is in a string */
static int char_in_string(char c, const char* str) {
    if (!str) return 0;
    
    while (*str) {
        if (c == *str++) {
            return 1;
        }
    }
    
    return 0;
}

/* Helper function to compile and execute a regex pattern */
static int regex_match(const char* pattern, const char* str) {
    regex_t regex;
    int result;
    
    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        LOG_ERROR("Failed to compile regex pattern: %s", pattern);
        return -1;
    }
    
    result = regexec(&regex, str, 0, NULL, 0);
    regfree(&regex);
    
    return (result == 0) ? 1 : 0;
}

/* Get a string description of a validation error */
const char* validation_error_string(validation_error_t error) {
    switch (error) {
        case VALIDATION_SUCCESS:
            return "Validation succeeded";
        case VALIDATION_ERROR_NULL_INPUT:
            return "Null input provided";
        case VALIDATION_ERROR_EMPTY_INPUT:
            return "Empty input provided";
        case VALIDATION_ERROR_TOO_LONG:
            return "Input exceeds maximum length";
        case VALIDATION_ERROR_INVALID_CHARS:
            return "Input contains invalid characters";
        case VALIDATION_ERROR_PATTERN_MATCH:
            return "Input matches a forbidden pattern";
        case VALIDATION_ERROR_INVALID_FORMAT:
            return "Input has invalid format";
        case VALIDATION_ERROR_OUT_OF_RANGE:
            return "Numeric input is out of range";
        case VALIDATION_ERROR_PATH_TRAVERSAL:
            return "Input contains path traversal sequences";
        case VALIDATION_ERROR_JSON_FORMAT:
            return "Invalid JSON format";
        case VALIDATION_ERROR_UNKNOWN:
        default:
            return "Unknown validation error";
    }
}

/* Validate a string based on allowed character set and length */
validation_error_t validate_string(const char* input, size_t min_len, size_t max_len, const char* allowed_chars) {
    if (!input) {
        return VALIDATION_ERROR_NULL_INPUT;
    }
    
    size_t len = strlen(input);
    
    if (len < min_len) {
        return VALIDATION_ERROR_EMPTY_INPUT;
    }
    
    if (len > max_len) {
        return VALIDATION_ERROR_TOO_LONG;
    }
    
    /* If no allowed_chars specified, accept any printable character */
    if (!allowed_chars) {
        for (size_t i = 0; i < len; i++) {
            if (!isprint((unsigned char)input[i])) {
                return VALIDATION_ERROR_INVALID_CHARS;
            }
        }
    } else {
        for (size_t i = 0; i < len; i++) {
            if (!char_in_string(input[i], allowed_chars)) {
                return VALIDATION_ERROR_INVALID_CHARS;
            }
        }
    }
    
    return VALIDATION_SUCCESS;
}

/* Validate an alphanumeric string */
validation_error_t validate_alphanumeric(const char* input, size_t min_len, size_t max_len) {
    if (!input) {
        return VALIDATION_ERROR_NULL_INPUT;
    }
    
    size_t len = strlen(input);
    
    if (len < min_len) {
        return VALIDATION_ERROR_EMPTY_INPUT;
    }
    
    if (len > max_len) {
        return VALIDATION_ERROR_TOO_LONG;
    }
    
    for (size_t i = 0; i < len; i++) {
        if (!isalnum((unsigned char)input[i])) {
            return VALIDATION_ERROR_INVALID_CHARS;
        }
    }
    
    return VALIDATION_SUCCESS;
}

/* Validate an identifier (alphanumeric plus underscore, starting with letter) */
validation_error_t validate_identifier(const char* input, size_t min_len, size_t max_len) {
    if (!input) {
        return VALIDATION_ERROR_NULL_INPUT;
    }
    
    size_t len = strlen(input);
    
    if (len < min_len) {
        return VALIDATION_ERROR_EMPTY_INPUT;
    }
    
    if (len > max_len) {
        return VALIDATION_ERROR_TOO_LONG;
    }
    
    /* First character must be a letter */
    if (!isalpha((unsigned char)input[0])) {
        return VALIDATION_ERROR_INVALID_FORMAT;
    }
    
    /* Remaining characters must be alphanumeric or underscore */
    for (size_t i = 1; i < len; i++) {
        if (!isalnum((unsigned char)input[i]) && input[i] != '_') {
            return VALIDATION_ERROR_INVALID_CHARS;
        }
    }
    
    return VALIDATION_SUCCESS;
}

/* Validate a file path for path traversal attacks */
validation_error_t validate_path(const char* input) {
    if (!input) {
        return VALIDATION_ERROR_NULL_INPUT;
    }
    
    size_t len = strlen(input);
    
    if (len == 0) {
        return VALIDATION_ERROR_EMPTY_INPUT;
    }
    
    if (len > MAX_PATH_LENGTH) {
        return VALIDATION_ERROR_TOO_LONG;
    }
    
    /* Check for path traversal sequences */
    if (strstr(input, "../") || strstr(input, "..\\") || 
        strcmp(input, "..") == 0 || strcmp(input, ".") == 0) {
        return VALIDATION_ERROR_PATH_TRAVERSAL;
    }
    
    /* Check for null byte injection */
    for (size_t i = 0; i < len; i++) {
        if (input[i] == '\0') {
            return VALIDATION_ERROR_INVALID_CHARS;
        }
    }
    
    return VALIDATION_SUCCESS;
}

/* Validate a number is within range */
validation_error_t validate_int_range(int64_t value, int64_t min_value, int64_t max_value) {
    if (value < min_value || value > max_value) {
        return VALIDATION_ERROR_OUT_OF_RANGE;
    }
    
    return VALIDATION_SUCCESS;
}

/* Validate a floating point number is within range */
validation_error_t validate_double_range(double value, double min_value, double max_value) {
    if (value < min_value || value > max_value) {
        return VALIDATION_ERROR_OUT_OF_RANGE;
    }
    
    return VALIDATION_SUCCESS;
}

/* Validate JSON string format */
validation_error_t validate_json(const char* json_str, size_t json_len, json_value_t** result) {
    if (!json_str) {
        return VALIDATION_ERROR_NULL_INPUT;
    }
    
    if (json_len == 0) {
        return VALIDATION_ERROR_EMPTY_INPUT;
    }
    
    /* Attempt to parse the JSON */
    json_value_t* json = json_parse(json_str);
    if (!json) {
        return VALIDATION_ERROR_JSON_FORMAT;
    }
    
    /* If result pointer provided, return the parsed JSON */
    if (result) {
        *result = json;
    } else {
        /* Otherwise, free the parsed JSON */
        json_free(json);
    }
    
    return VALIDATION_SUCCESS;
}

/* Validate a URL format */
validation_error_t validate_url(const char* url) {
    if (!url) {
        return VALIDATION_ERROR_NULL_INPUT;
    }
    
    size_t len = strlen(url);
    
    if (len == 0) {
        return VALIDATION_ERROR_EMPTY_INPUT;
    }
    
    if (len > MAX_URL_LENGTH) {
        return VALIDATION_ERROR_TOO_LONG;
    }
    
    /* Basic URL pattern: protocol://host[:port][/path][?query][#fragment] */
    const char* pattern = "^(https?|ftp)://([a-zA-Z0-9][-a-zA-Z0-9]*[a-zA-Z0-9]\\.)+[a-zA-Z]{2,}(:[0-9]+)?(/[-a-zA-Z0-9%_.~#+]*)*";
    int match = regex_match(pattern, url);
    
    if (match < 0) {
        return VALIDATION_ERROR_UNKNOWN;
    }
    
    if (match == 0) {
        return VALIDATION_ERROR_INVALID_FORMAT;
    }
    
    return VALIDATION_SUCCESS;
}

/* Validate an email address format */
validation_error_t validate_email(const char* email) {
    if (!email) {
        return VALIDATION_ERROR_NULL_INPUT;
    }
    
    size_t len = strlen(email);
    
    if (len == 0) {
        return VALIDATION_ERROR_EMPTY_INPUT;
    }
    
    if (len > MAX_EMAIL_LENGTH) {
        return VALIDATION_ERROR_TOO_LONG;
    }
    
    /* Basic email pattern: username@domain.tld */
    const char* pattern = "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$";
    int match = regex_match(pattern, email);
    
    if (match < 0) {
        return VALIDATION_ERROR_UNKNOWN;
    }
    
    if (match == 0) {
        return VALIDATION_ERROR_INVALID_FORMAT;
    }
    
    return VALIDATION_SUCCESS;
}

/* Validate an IP address format */
validation_error_t validate_ip_address(const char* ip) {
    if (!ip) {
        return VALIDATION_ERROR_NULL_INPUT;
    }
    
    size_t len = strlen(ip);
    
    if (len == 0) {
        return VALIDATION_ERROR_EMPTY_INPUT;
    }
    
    if (len > MAX_IP_LENGTH) {
        return VALIDATION_ERROR_TOO_LONG;
    }
    
    /* IPv4 pattern */
    const char* ipv4_pattern = "^([0-9]{1,3}\\.){3}[0-9]{1,3}$";
    int match_ipv4 = regex_match(ipv4_pattern, ip);
    
    if (match_ipv4 < 0) {
        return VALIDATION_ERROR_UNKNOWN;
    }
    
    if (match_ipv4) {
        /* Additional check for IPv4: each octet must be 0-255 */
        unsigned int a, b, c, d;
        if (sscanf(ip, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
            return VALIDATION_ERROR_INVALID_FORMAT;
        }
        
        if (a > 255 || b > 255 || c > 255 || d > 255) {
            return VALIDATION_ERROR_INVALID_FORMAT;
        }
        
        return VALIDATION_SUCCESS;
    }
    
    /* IPv6 pattern - simplified check */
    const char* ipv6_pattern = "^([0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}$";
    int match_ipv6 = regex_match(ipv6_pattern, ip);
    
    if (match_ipv6 < 0) {
        return VALIDATION_ERROR_UNKNOWN;
    }
    
    if (match_ipv6) {
        return VALIDATION_SUCCESS;
    }
    
    /* IPv6 with :: compression */
    const char* ipv6_compressed_pattern = "^(([0-9a-fA-F]{1,4}:){0,6}:([0-9a-fA-F]{1,4}:){0,6}[0-9a-fA-F]{1,4})$";
    int match_ipv6_compressed = regex_match(ipv6_compressed_pattern, ip);
    
    if (match_ipv6_compressed < 0) {
        return VALIDATION_ERROR_UNKNOWN;
    }
    
    if (match_ipv6_compressed) {
        return VALIDATION_SUCCESS;
    }
    
    return VALIDATION_ERROR_INVALID_FORMAT;
}

/* Sanitize a string for safe output (e.g., prevents XSS) */
int sanitize_string(const char* input, char* output, size_t output_size) {
    if (!input || !output || output_size == 0) {
        return -1;
    }
    
    size_t input_len = strlen(input);
    size_t out_index = 0;
    
    for (size_t i = 0; i < input_len && out_index < output_size - 1; i++) {
        char c = input[i];
        
        /* HTML encode special characters */
        switch (c) {
            case '<':
                if (out_index + 4 >= output_size - 1) {
                    goto end_sanitize;
                }
                output[out_index++] = '&';
                output[out_index++] = 'l';
                output[out_index++] = 't';
                output[out_index++] = ';';
                break;
                
            case '>':
                if (out_index + 4 >= output_size - 1) {
                    goto end_sanitize;
                }
                output[out_index++] = '&';
                output[out_index++] = 'g';
                output[out_index++] = 't';
                output[out_index++] = ';';
                break;
                
            case '&':
                if (out_index + 5 >= output_size - 1) {
                    goto end_sanitize;
                }
                output[out_index++] = '&';
                output[out_index++] = 'a';
                output[out_index++] = 'm';
                output[out_index++] = 'p';
                output[out_index++] = ';';
                break;
                
            case '"':
                if (out_index + 6 >= output_size - 1) {
                    goto end_sanitize;
                }
                output[out_index++] = '&';
                output[out_index++] = 'q';
                output[out_index++] = 'u';
                output[out_index++] = 'o';
                output[out_index++] = 't';
                output[out_index++] = ';';
                break;
                
            case '\'':
                if (out_index + 6 >= output_size - 1) {
                    goto end_sanitize;
                }
                output[out_index++] = '&';
                output[out_index++] = 'a';
                output[out_index++] = 'p';
                output[out_index++] = 'o';
                output[out_index++] = 's';
                output[out_index++] = ';';
                break;
                
            default:
                /* Copy safe characters directly */
                if (isprint(c) && c != '\\' && c != '/') {
                    output[out_index++] = c;
                } else if (c == '\\' || c == '/') {
                    /* Escape backslashes and forward slashes */
                    if (out_index + 1 >= output_size - 1) {
                        goto end_sanitize;
                    }
                    output[out_index++] = '\\';
                    output[out_index++] = c;
                }
                break;
        }
    }
    
end_sanitize:
    output[out_index] = '\0';
    return 0;
}

/* Sanitize a database collection name */
int sanitize_collection_name(const char* input, char* output, size_t output_size) {
    if (!input || !output || output_size == 0) {
        return -1;
    }
    
    /* For collection names, only allow alphanumeric and underscore */
    size_t input_len = strlen(input);
    size_t out_index = 0;
    
    /* First character must be a letter */
    if (input_len > 0 && isalpha((unsigned char)input[0]) && out_index < output_size - 1) {
        output[out_index++] = input[0];
    }
    
    /* Remaining characters must be alphanumeric or underscore */
    for (size_t i = 1; i < input_len && out_index < output_size - 1; i++) {
        if (isalnum((unsigned char)input[i]) || input[i] == '_') {
            output[out_index++] = input[i];
        }
    }
    
    output[out_index] = '\0';
    
    /* If no characters were copied, return error */
    if (out_index == 0) {
        return -1;
    }
    
    return 0;
}

/* Sanitize a database document ID */
int sanitize_document_id(const char* input, char* output, size_t output_size) {
    if (!input || !output || output_size == 0) {
        return -1;
    }
    
    /* For document IDs, allow alphanumeric, underscore, hyphen, and dot */
    static const char* allowed_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.";
    
    size_t input_len = strlen(input);
    size_t out_index = 0;
    
    for (size_t i = 0; i < input_len && out_index < output_size - 1; i++) {
        if (char_in_string(input[i], allowed_chars)) {
            output[out_index++] = input[i];
        }
    }
    
    output[out_index] = '\0';
    
    /* If no characters were copied, return error */
    if (out_index == 0) {
        return -1;
    }
    
    return 0;
}

/* Validate a database collection name */
validation_error_t validate_collection_name(const char* name) {
    return validate_identifier(name, 1, MAX_COLLECTION_NAME_LENGTH);
}

/* Validate a database document ID */
validation_error_t validate_document_id(const char* id) {
    if (!id) {
        return VALIDATION_ERROR_NULL_INPUT;
    }
    
    size_t len = strlen(id);
    
    if (len == 0) {
        return VALIDATION_ERROR_EMPTY_INPUT;
    }
    
    if (len > MAX_DOCUMENT_ID_LENGTH) {
        return VALIDATION_ERROR_TOO_LONG;
    }
    
    /* Allow alphanumeric, underscore, hyphen, and dot */
    static const char* allowed_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.";
    
    for (size_t i = 0; i < len; i++) {
        if (!char_in_string(id[i], allowed_chars)) {
            return VALIDATION_ERROR_INVALID_CHARS;
        }
    }
    
    return VALIDATION_SUCCESS;
}