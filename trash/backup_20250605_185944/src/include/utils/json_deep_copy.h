#ifndef JSON_DEEP_COPY_H
#define JSON_DEEP_COPY_H

#include "utils/json.h"

/**
 * Create a deep copy of a JSON value
 * 
 * This function recursively copies all nested values to create
 * a completely independent copy of the JSON structure.
 * 
 * @param value The JSON value to copy
 * @return A new JSON value that is a deep copy of the input, or NULL on error
 */
json_value_t* json_deep_copy(json_value_t* value);

/**
 * Optimized version of json_deep_copy
 * 
 * This version avoids stringify/parse for non-object types
 * 
 * @param value The JSON value to copy
 * @return A new JSON value that is a deep copy of the input, or NULL on error
 */
json_value_t* json_deep_copy_optimized(json_value_t* value);

#endif /* JSON_DEEP_COPY_H */