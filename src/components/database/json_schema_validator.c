#include "database/database.h"
#include "utils/json.h"
#include "utils/logger.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Use the schema_validation_result_t from database.h */

/* Forward declarations */
static schema_validation_result_t validate_value(json_value_t* schema, json_value_t* value, const char* path);
static schema_validation_result_t validate_object(json_value_t* schema, json_value_t* value, const char* path);
static schema_validation_result_t validate_array(json_value_t* schema, json_value_t* value, const char* path);
static schema_validation_result_t validate_string(json_value_t* schema, json_value_t* value, const char* path);
static schema_validation_result_t validate_number(json_value_t* schema, json_value_t* value, const char* path);
static schema_validation_result_t validate_boolean(json_value_t* schema, json_value_t* value, const char* path);
static schema_validation_result_t validate_null(json_value_t* schema, json_value_t* value, const char* path);

/* Create success result */
static schema_validation_result_t success_result() {
    return (schema_validation_result_t){ .is_valid = 1, .error_message = NULL, .error_field = NULL };
}

/* Create error result */
static schema_validation_result_t error_result(const char* error, const char* path) {
    return (schema_validation_result_t){ 
        .is_valid = 0, 
        .error_message = strdup(error), 
        .error_field = strdup(path) 
    };
}

/* Free validation result */
static void free_validation_result(schema_validation_result_t* result) {
    if (result->error_message) free(result->error_message);
    if (result->error_field) free(result->error_field);
}

/* Validate JSON value against JSON Schema */
int json_schema_validate(json_value_t* schema, json_value_t* value, char** error_msg) {
    if (!schema || !value) {
        if (error_msg) *error_msg = strdup("Schema and value cannot be NULL");
        return 0;
    }
    
    schema_validation_result_t result = validate_value(schema, value, "$");
    
    if (!result.is_valid && error_msg) {
        char buffer[512];
        snprintf(buffer, sizeof(buffer), "Validation failed at %s: %s", 
                 result.error_field ? result.error_field : "unknown", 
                 result.error_message ? result.error_message : "unknown error");
        *error_msg = strdup(buffer);
    }
    
    int valid = result.is_valid;
    free_validation_result(&result);
    return valid;
}

/* Main validation dispatcher */
static schema_validation_result_t validate_value(json_value_t* schema, json_value_t* value, const char* path) {
    if (!schema || schema->type != JSON_OBJECT) {
        return error_result("Invalid schema", path);
    }
    
    /* Check type constraint */
    json_value_t* type_constraint = json_object_get(schema, "type");
    if (type_constraint && type_constraint->type == JSON_STRING) {
        const char* expected_type = json_get_string(type_constraint);
        
        /* Check type match */
        if (strcmp(expected_type, "object") == 0) {
            if (value->type != JSON_OBJECT) {
                return error_result("Expected object", path);
            }
            return validate_object(schema, value, path);
        } else if (strcmp(expected_type, "array") == 0) {
            if (value->type != JSON_ARRAY) {
                return error_result("Expected array", path);
            }
            return validate_array(schema, value, path);
        } else if (strcmp(expected_type, "string") == 0) {
            if (value->type != JSON_STRING) {
                return error_result("Expected string", path);
            }
            return validate_string(schema, value, path);
        } else if (strcmp(expected_type, "number") == 0) {
            if (value->type != JSON_NUMBER && value->type != JSON_INTEGER) {
                return error_result("Expected number", path);
            }
            return validate_number(schema, value, path);
        } else if (strcmp(expected_type, "integer") == 0) {
            if (value->type != JSON_INTEGER) {
                return error_result("Expected integer", path);
            }
            return validate_number(schema, value, path);
        } else if (strcmp(expected_type, "boolean") == 0) {
            if (value->type != JSON_BOOLEAN) {
                return error_result("Expected boolean", path);
            }
            return validate_boolean(schema, value, path);
        } else if (strcmp(expected_type, "null") == 0) {
            if (value->type != JSON_NULL) {
                return error_result("Expected null", path);
            }
            return validate_null(schema, value, path);
        }
    }
    
    /* No type constraint or unrecognized type - assume valid */
    return success_result();
}

/* Validate object */
static schema_validation_result_t validate_object(json_value_t* schema, json_value_t* value, const char* path) {
    /* Check properties */
    json_value_t* properties = json_object_get(schema, "properties");
    if (properties && properties->type == JSON_OBJECT) {
        /* Validate each defined property */
        for (size_t i = 0; i < properties->value.object.size; i++) {
            const char* prop_name = properties->value.object.entries[i].key;
            json_value_t* prop_schema = properties->value.object.entries[i].value;
            json_value_t* prop_value = json_object_get(value, prop_name);
            
            if (prop_value) {
                char prop_path[256];
                snprintf(prop_path, sizeof(prop_path), "%s.%s", path, prop_name);
                
                schema_validation_result_t result = validate_value(prop_schema, prop_value, prop_path);
                if (!result.is_valid) {
                    return result;
                }
            }
        }
    }
    
    /* Check required properties */
    json_value_t* required = json_object_get(schema, "required");
    if (required && required->type == JSON_ARRAY) {
        for (size_t i = 0; i < required->value.array.size; i++) {
            json_value_t* req_prop = json_array_get(required, i);
            if (req_prop && req_prop->type == JSON_STRING) {
                const char* prop_name = json_get_string(req_prop);
                if (!json_object_has(value, prop_name)) {
                    char error[256];
                    snprintf(error, sizeof(error), "Missing required property: %s", prop_name);
                    return error_result(error, path);
                }
            }
        }
    }
    
    /* Check additionalProperties */
    json_value_t* additional_props = json_object_get(schema, "additionalProperties");
    if (additional_props && additional_props->type == JSON_BOOLEAN && 
        !additional_props->value.boolean && properties) {
        
        /* Check for properties not defined in schema */
        for (size_t i = 0; i < value->value.object.size; i++) {
            const char* prop_name = value->value.object.entries[i].key;
            if (!json_object_has(properties, prop_name)) {
                char error[256];
                snprintf(error, sizeof(error), "Additional property not allowed: %s", prop_name);
                return error_result(error, path);
            }
        }
    }
    
    return success_result();
}

/* Validate array */
static schema_validation_result_t validate_array(json_value_t* schema, json_value_t* value, const char* path) {
    /* Check items constraint */
    json_value_t* items = json_object_get(schema, "items");
    if (items) {
        for (size_t i = 0; i < value->value.array.size; i++) {
            json_value_t* item = json_array_get(value, i);
            char item_path[256];
            snprintf(item_path, sizeof(item_path), "%s[%zu]", path, i);
            
            schema_validation_result_t result = validate_value(items, item, item_path);
            if (!result.is_valid) {
                return result;
            }
        }
    }
    
    /* Check minItems */
    json_value_t* min_items = json_object_get(schema, "minItems");
    if (min_items && (min_items->type == JSON_INTEGER || min_items->type == JSON_NUMBER)) {
        int min = min_items->type == JSON_INTEGER ? min_items->value.integer : (int)min_items->value.number;
        if (value->value.array.size < min) {
            char error[256];
            snprintf(error, sizeof(error), "Array has %zu items, minimum is %d", 
                     value->value.array.size, min);
            return error_result(error, path);
        }
    }
    
    /* Check maxItems */
    json_value_t* max_items = json_object_get(schema, "maxItems");
    if (max_items && (max_items->type == JSON_INTEGER || max_items->type == JSON_NUMBER)) {
        int max = max_items->type == JSON_INTEGER ? max_items->value.integer : (int)max_items->value.number;
        if (value->value.array.size > max) {
            char error[256];
            snprintf(error, sizeof(error), "Array has %zu items, maximum is %d", 
                     value->value.array.size, max);
            return error_result(error, path);
        }
    }
    
    return success_result();
}

/* Validate string */
static schema_validation_result_t validate_string(json_value_t* schema, json_value_t* value, const char* path) {
    const char* str = json_get_string(value);
    size_t len = strlen(str);
    
    /* Check minLength */
    json_value_t* min_length = json_object_get(schema, "minLength");
    if (min_length && (min_length->type == JSON_INTEGER || min_length->type == JSON_NUMBER)) {
        int min = min_length->type == JSON_INTEGER ? min_length->value.integer : (int)min_length->value.number;
        if (len < min) {
            char error[256];
            snprintf(error, sizeof(error), "String length %zu is less than minimum %d", len, min);
            return error_result(error, path);
        }
    }
    
    /* Check maxLength */
    json_value_t* max_length = json_object_get(schema, "maxLength");
    if (max_length && (max_length->type == JSON_INTEGER || max_length->type == JSON_NUMBER)) {
        int max = max_length->type == JSON_INTEGER ? max_length->value.integer : (int)max_length->value.number;
        if (len > max) {
            char error[256];
            snprintf(error, sizeof(error), "String length %zu exceeds maximum %d", len, max);
            return error_result(error, path);
        }
    }
    
    /* Check pattern (regex) */
    json_value_t* pattern = json_object_get(schema, "pattern");
    if (pattern && pattern->type == JSON_STRING) {
        /* TODO: Implement regex validation */
        /* For now, we skip pattern validation */
    }
    
    /* Check enum */
    json_value_t* enum_values = json_object_get(schema, "enum");
    if (enum_values && enum_values->type == JSON_ARRAY) {
        int found = 0;
        for (size_t i = 0; i < enum_values->value.array.size; i++) {
            json_value_t* enum_val = json_array_get(enum_values, i);
            if (enum_val && enum_val->type == JSON_STRING) {
                if (strcmp(str, json_get_string(enum_val)) == 0) {
                    found = 1;
                    break;
                }
            }
        }
        if (!found) {
            return error_result("Value not in enumeration", path);
        }
    }
    
    return success_result();
}

/* Validate number */
static schema_validation_result_t validate_number(json_value_t* schema, json_value_t* value, const char* path) {
    double num = value->type == JSON_INTEGER ? value->value.integer : value->value.number;
    
    /* Check minimum */
    json_value_t* minimum = json_object_get(schema, "minimum");
    if (minimum && (minimum->type == JSON_INTEGER || minimum->type == JSON_NUMBER)) {
        double min = minimum->type == JSON_INTEGER ? minimum->value.integer : minimum->value.number;
        if (num < min) {
            char error[256];
            snprintf(error, sizeof(error), "Value %g is less than minimum %g", num, min);
            return error_result(error, path);
        }
    }
    
    /* Check maximum */
    json_value_t* maximum = json_object_get(schema, "maximum");
    if (maximum && (maximum->type == JSON_INTEGER || maximum->type == JSON_NUMBER)) {
        double max = maximum->type == JSON_INTEGER ? maximum->value.integer : maximum->value.number;
        if (num > max) {
            char error[256];
            snprintf(error, sizeof(error), "Value %g exceeds maximum %g", num, max);
            return error_result(error, path);
        }
    }
    
    /* Check multipleOf */
    json_value_t* multiple_of = json_object_get(schema, "multipleOf");
    if (multiple_of && (multiple_of->type == JSON_INTEGER || multiple_of->type == JSON_NUMBER)) {
        double divisor = multiple_of->type == JSON_INTEGER ? multiple_of->value.integer : multiple_of->value.number;
        if (divisor != 0) {
            double remainder = fmod(num, divisor);
            if (remainder != 0) {
                char error[256];
                snprintf(error, sizeof(error), "Value %g is not a multiple of %g", num, divisor);
                return error_result(error, path);
            }
        }
    }
    
    return success_result();
}

/* Validate boolean */
static schema_validation_result_t validate_boolean(json_value_t* schema, json_value_t* value, const char* path) {
    /* Boolean values don't have specific constraints in JSON Schema */
    (void)schema;
    (void)value;
    (void)path;
    return success_result();
}

/* Validate null */
static schema_validation_result_t validate_null(json_value_t* schema, json_value_t* value, const char* path) {
    /* Null values don't have specific constraints in JSON Schema */
    (void)schema;
    (void)value;
    (void)path;
    return success_result();
}