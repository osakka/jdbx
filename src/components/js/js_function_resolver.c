/**
 * JavaScript Function Resolver
 * Handles inline and reference function resolution for the unified documents system
 */

#include "js/js_engine.h"
#include "js/js_function_resolver.h"
#include "database/document_storage.h"
#include "database/database.h"
#include "utils/json.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include <string.h>

/* Function reference prefix */
#define FUNCTION_REF_PREFIX "@function:"
#define INLINE_FUNCTION_PREFIX "function"

/**
 * Check if a value is a function reference
 * Function references are strings starting with "@function:"
 */
int js_is_function_reference(json_value_t* value) {
    if (!value || value->type != JSON_STRING) {
        return 0;
    }
    
    return strncmp(value->value.string, FUNCTION_REF_PREFIX, 
                   strlen(FUNCTION_REF_PREFIX)) == 0;
}

/**
 * Check if a value is an inline function
 * Inline functions are strings starting with "function" or arrow functions
 */
int js_is_inline_function(json_value_t* value) {
    if (!value || value->type != JSON_STRING) {
        return 0;
    }
    
    const char* str = value->value.string;
    
    /* Check for traditional function */
    if (strncmp(str, INLINE_FUNCTION_PREFIX, strlen(INLINE_FUNCTION_PREFIX)) == 0) {
        return 1;
    }
    
    /* Check for arrow function */
    const char* arrow = strstr(str, "=>");
    if (arrow) {
        /* Simple check - could be improved */
        return 1;
    }
    
    return 0;
}

/**
 * Extract function ID from a reference
 * @function:library/function-name -> library/function-name
 */
static char* extract_function_id(const char* reference) {
    if (!reference) return NULL;
    
    const char* id_start = reference + strlen(FUNCTION_REF_PREFIX);
    return buffer_pool_strdup(id_start);
}

/**
 * Load a function from the functions collection
 */
static json_value_t* load_function_from_collection(database_t* db, const char* function_id) {
    if (!db || !function_id) return NULL;
    
    /* Parse library and function name */
    char library[256] = "system";
    char function_name[256];
    const char* slash = strchr(function_id, '/');
    
    if (slash) {
        size_t lib_len = slash - function_id;
        if (lib_len < sizeof(library)) {
            strncpy(library, function_id, lib_len);
            library[lib_len] = '\0';
            strncpy(function_name, slash + 1, sizeof(function_name) - 1);
        } else {
            strncpy(function_name, function_id, sizeof(function_name) - 1);
        }
    } else {
        strncpy(function_name, function_id, sizeof(function_name) - 1);
    }
    function_name[sizeof(function_name) - 1] = '\0';
    
    /* Query the functions collection */
    char collection_path[512];
    snprintf(collection_path, sizeof(collection_path), "%s/functions", library);
    
    json_value_t* query = json_create_object();
    json_object_set(query, "name", json_create_string(function_name));
    
    json_value_t* results = db_query_documents(db, STORAGE_LIBRARY, collection_path, query);
    /* CHECKPOINT: json_free(query); */
    
    if (!results) {
        LOG_ERROR("Failed to query functions collection: %s", collection_path);
        return NULL;
    }
    
    json_value_t* documents = json_object_get(results, "documents");
    if (!documents || documents->type != JSON_ARRAY || json_array_size(documents) == 0) {
        LOG_DEBUG("Function not found: %s in %s", function_name, collection_path);
        /* CHECKPOINT: json_free(results); */
        return NULL;
    }
    
    /* Get the first matching function */
    json_value_t* func_doc = json_array_get(documents, 0);
    if (!func_doc) {
        /* CHECKPOINT: json_free(results); */
        return NULL;
    }
    
    /* Clone the function document before freeing results */
    json_value_t* function_clone = json_clone(func_doc);
    /* CHECKPOINT: json_free(results); */
    
    return function_clone;
}

/**
 * Resolve a function - either inline or by reference
 * Returns a JSON object with:
 * - code: The JavaScript code to execute
 * - name: The function name (for debugging)
 * - type: "inline" or "reference"
 */
json_value_t* js_resolve_function(database_t* db, json_value_t* function_spec) {
    if (!function_spec) return NULL;
    
    json_value_t* result = json_create_object();
    if (!result) return NULL;
    
    /* Handle inline function */
    if (js_is_inline_function(function_spec)) {
        json_object_set(result, "code", json_create_string(function_spec->value.string));
        json_object_set(result, "name", json_create_string("inline_function"));
        json_object_set(result, "type", json_create_string("inline"));
        LOG_DEBUG("Resolved inline function");
        return result;
    }
    
    /* Handle function reference */
    if (js_is_function_reference(function_spec)) {
        char* function_id = extract_function_id(function_spec->value.string);
        if (!function_id) {
            /* CHECKPOINT: json_free(result); */
            return NULL;
        }
        
        json_value_t* func_doc = load_function_from_collection(db, function_id);
        buffer_pool_free(function_id);
        
        if (!func_doc) {
            /* CHECKPOINT: json_free(result); */
            return NULL;
        }
        
        /* Extract function details */
        json_value_t* code = json_object_get(func_doc, "code");
        json_value_t* name = json_object_get(func_doc, "name");
        
        if (!code || code->type != JSON_STRING) {
            LOG_ERROR("Function document missing 'code' field");
            /* CHECKPOINT: json_free(func_doc); */
            /* CHECKPOINT: json_free(result); */
            return NULL;
        }
        
        json_object_set(result, "code", json_create_string(code->value.string));
        json_object_set(result, "name", json_create_string(
            name && name->type == JSON_STRING ? name->value.string : "referenced_function"));
        json_object_set(result, "type", json_create_string("reference"));
        
        /* Copy additional metadata if present */
        json_value_t* params = json_object_get(func_doc, "parameters");
        if (params) {
            json_object_set(result, "parameters", json_clone(params));
        }
        
        json_value_t* desc = json_object_get(func_doc, "description");
        if (desc) {
            json_object_set(result, "description", json_clone(desc));
        }
        
        /* CHECKPOINT: json_free(func_doc); */
        LOG_DEBUG("Resolved function reference");
        return result;
    }
    
    /* Not a function */
    /* CHECKPOINT: json_free(result); */
    return NULL;
}

/**
 * Resolve all functions in a document recursively
 * This walks through the document and resolves any function references or inline functions
 */
static json_value_t* resolve_functions_in_value(database_t* db, json_value_t* value, 
                                               const char* field_name) {
    if (!value) return NULL;
    
    switch (value->type) {
        case JSON_STRING:
            /* Check if this field typically contains functions */
            if (field_name && (strcmp(field_name, "validator") == 0 ||
                             strcmp(field_name, "transformer") == 0 ||
                             strcmp(field_name, "handler") == 0 ||
                             strcmp(field_name, "onCreate") == 0 ||
                             strcmp(field_name, "onUpdate") == 0 ||
                             strcmp(field_name, "onDelete") == 0)) {
                json_value_t* resolved = js_resolve_function(db, value);
                if (resolved) {
                    return resolved;
                }
            }
            return json_clone(value);
            
        case JSON_OBJECT: {
            json_value_t* new_obj = json_create_object();
            for (size_t i = 0; i < value->value.object.size; i++) {
                json_object_entry_t* entry = &value->value.object.entries[i];
                json_value_t* resolved = resolve_functions_in_value(db, entry->value, entry->key);
                json_object_set(new_obj, entry->key, resolved);
            }
            return new_obj;
        }
        
        case JSON_ARRAY: {
            json_value_t* new_arr = json_create_array();
            for (size_t i = 0; i < json_array_size(value); i++) {
                json_value_t* item = json_array_get(value, i);
                json_value_t* resolved = resolve_functions_in_value(db, item, NULL);
                json_array_append(new_arr, resolved);
            }
            return new_arr;
        }
        
        default:
            return json_clone(value);
    }
}

/**
 * Resolve all function references and inline functions in a document
 */
json_value_t* js_resolve_document_functions(database_t* db, json_value_t* document) {
    if (!document || document->type != JSON_OBJECT) {
        return json_clone(document);
    }
    
    return resolve_functions_in_value(db, document, NULL);
}

/**
 * Create a function reference string
 */
char* js_create_function_reference(const char* library, const char* function_name) {
    if (!function_name) return NULL;
    
    char buffer[512];
    if (library) {
        snprintf(buffer, sizeof(buffer), "%s%s/%s", 
                 FUNCTION_REF_PREFIX, library, function_name);
    } else {
        snprintf(buffer, sizeof(buffer), "%ssystem/%s", 
                 FUNCTION_REF_PREFIX, function_name);
    }
    
    return buffer_pool_strdup(buffer);
}