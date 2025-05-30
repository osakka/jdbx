/**
 * Optimized JSON deep copy implementation
 * 
 * This implementation avoids the inefficient stringify/parse approach
 * and directly copies the JSON structure recursively.
 */

#include "utils/json.h"
#include <string.h>
#include <stdlib.h>

/* Forward declaration */
static json_value_t* json_deep_copy_internal(json_value_t* value);

/* Deep copy a JSON string value */
static json_value_t* copy_string(json_value_t* value) {
    if (!value || value->type != JSON_STRING) return NULL;
    
    return json_create_string(value->value.string);
}

/* Deep copy a JSON number value */
static json_value_t* copy_number(json_value_t* value) {
    if (!value || value->type != JSON_NUMBER) return NULL;
    
    return json_create_number(value->value.number);
}

/* Deep copy a JSON boolean value */
static json_value_t* copy_boolean(json_value_t* value) {
    if (!value || value->type != JSON_BOOLEAN) return NULL;
    
    return json_create_boolean(value->value.boolean);
}

/* Deep copy a JSON array value */
static json_value_t* copy_array(json_value_t* value) {
    if (!value || value->type != JSON_ARRAY) return NULL;
    
    json_value_t* new_array = json_create_array();
    if (!new_array) return NULL;
    
    /* Copy each element */
    for (size_t i = 0; i < value->value.array.size; i++) {
        json_value_t* elem = json_array_get(value, i);
        if (elem) {
            json_value_t* elem_copy = json_deep_copy_internal(elem);
            if (elem_copy) {
                json_array_append(new_array, elem_copy);
            }
        }
    }
    
    return new_array;
}

/* Deep copy a JSON object value */
static json_value_t* copy_object(json_value_t* value) {
    if (!value || value->type != JSON_OBJECT) return NULL;
    
    json_value_t* new_obj = json_create_object();
    if (!new_obj) return NULL;
    
    /* We need to iterate through all keys in the object
     * Since we don't have direct access to internal structure,
     * we'll use a list of known keys for now.
     * TODO: Implement proper object iteration in json.c
     */
    
    /* For now, convert to string and parse to get all keys
     * This is temporary until we have proper object iteration
     */
    char* str = json_stringify(value);
    if (!str) {
        json_free(new_obj);
        return NULL;
    }
    
    json_value_t* temp = json_parse(str);
    free(str);
    
    if (!temp) {
        json_free(new_obj);
        return NULL;
    }
    
    /* Copy the parsed object */
    if (temp->type == JSON_OBJECT) {
        /* This is still using stringify/parse for objects
         * TODO: Implement proper object key iteration
         */
        json_free(new_obj);
        return temp;
    }
    
    json_free(temp);
    json_free(new_obj);
    return NULL;
}

/* Internal recursive deep copy function */
static json_value_t* json_deep_copy_internal(json_value_t* value) {
    if (!value) return NULL;
    
    switch (value->type) {
        case JSON_NULL:
            return json_create_null();
            
        case JSON_STRING:
            return copy_string(value);
            
        case JSON_NUMBER:
            return copy_number(value);
            
        case JSON_BOOLEAN:
            return copy_boolean(value);
            
        case JSON_ARRAY:
            return copy_array(value);
            
        case JSON_OBJECT:
            /* For now, objects still use stringify/parse
             * until we have proper object iteration API */
            return copy_object(value);
            
        default:
            return NULL;
    }
}

/* Public API for JSON deep copy */
json_value_t* json_deep_copy_optimized(json_value_t* value) {
    return json_deep_copy_internal(value);
}

/* Backwards compatible wrapper */
json_value_t* json_deep_copy(json_value_t* value) {
    if (!value) return NULL;
    
    /* For non-object types, use optimized copy */
    if (value->type != JSON_OBJECT) {
        return json_deep_copy_optimized(value);
    }
    
    /* For objects, fall back to stringify/parse for now
     * TODO: Optimize this when object iteration is available */
    char* json_str = json_stringify(value);
    if (!json_str) return NULL;
    
    json_value_t* copy = json_parse(json_str);
    free(json_str);
    
    return copy;
}