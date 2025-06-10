#ifndef JS_FUNCTION_RESOLVER_H
#define JS_FUNCTION_RESOLVER_H

#include "utils/json.h"
#include "database/database.h"

/**
 * JavaScript Function Resolver
 * 
 * This module provides functionality to resolve JavaScript functions that can be:
 * 1. Inline functions - defined directly in the document as strings
 * 2. Referenced functions - stored in the functions collection and referenced by ID
 * 
 * Function references use the format: @function:library/function-name
 * Inline functions are detected by starting with "function" or containing "=>"
 */

/* Check if a JSON value is a function reference */
int js_is_function_reference(json_value_t* value);

/* Check if a JSON value is an inline function */
int js_is_inline_function(json_value_t* value);

/**
 * Resolve a function specification into executable code
 * Returns a JSON object with:
 * - code: The JavaScript code to execute
 * - name: The function name (for debugging)
 * - type: "inline" or "reference"
 * - parameters: (optional) Parameter definitions
 * - description: (optional) Function description
 */
json_value_t* js_resolve_function(database_t* db, json_value_t* function_spec);

/**
 * Resolve all function references and inline functions in a document
 * This recursively walks through the document and resolves functions in fields like:
 * - validator
 * - transformer
 * - handler
 * - onCreate, onUpdate, onDelete
 */
json_value_t* js_resolve_document_functions(database_t* db, json_value_t* document);

/**
 * Create a function reference string
 * Returns: @function:library/function-name
 */
char* js_create_function_reference(const char* library, const char* function_name);

#endif /* JS_FUNCTION_RESOLVER_H */