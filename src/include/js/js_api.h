#ifndef JS_API_H
#define JS_API_H

#include "database/database.h"
#include "js/js_engine.h"

/**
 * Global JavaScript engine instance
 * This is needed for access from various API endpoints
 */
extern js_engine_t* g_js_engine;

/**
 * Initialize JavaScript API
 * 
 * @param db Database instance
 */
void js_api_init(database_t* db);

/**
 * Clean up JavaScript API
 */
void js_api_cleanup();

/**
 * Execute JavaScript query
 * 
 * @param db Database instance
 * @param collection_name Collection name
 * @param query_script JavaScript query script
 * @return JSON array of matching documents or NULL on error
 */
json_value_t* js_execute_query(js_engine_t* engine, const char* collection_name, const char* query_script);

/**
 * Execute JavaScript function
 * 
 * @param db Database instance
 * @param function_name Function name
 * @param arguments Function arguments (as JSON object)
 * @return Function result or NULL on error
 */
json_value_t* js_execute_function(js_engine_t* engine, const char* function_name, json_value_t* arguments);

/**
 * Register JavaScript function
 * 
 * @param db Database instance
 * @param function_name Function name
 * @param function_code Function code
 * @return 1 on success, 0 on failure
 */
int js_register_function(js_engine_t* engine, const char* function_name, const char* function_code);

/**
 * Handle JavaScript query request
 */
http_response_t* api_handle_js_query(api_context_t* ctx, http_request_t* request);

/**
 * Handle JavaScript eval request
 */
http_response_t* api_handle_js_eval(api_context_t* ctx, http_request_t* request);

/**
 * Handle JavaScript function request
 */
http_response_t* api_handle_js_function(api_context_t* ctx, http_request_t* request);

/**
 * Handle JavaScript function registration request
 */
http_response_t* api_handle_js_function_register(api_context_t* ctx, http_request_t* request);

#endif /* JS_API_H */