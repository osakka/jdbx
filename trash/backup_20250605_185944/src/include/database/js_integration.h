#ifndef DATABASE_JS_INTEGRATION_H
#define DATABASE_JS_INTEGRATION_H

#include "database/database.h"
#include "utils/json.h"

/**
 * JavaScript-Enhanced Database Operations
 * 
 * These functions integrate native JavaScript storage with database operations,
 * automatically executing validators, transformers, and functions based on 
 * document and collection tags.
 */

/**
 * Enhanced document insertion with JavaScript validation and transformation
 * Executes validators first, then transformers, then performs insertion
 */
json_value_t* db_insert_document_with_js(database_t* db, const char* collection_name, 
                                        json_value_t* document, const char* user_id);

/**
 * Enhanced document update with JavaScript validation and transformation
 * Merges update data, executes validators, then transformers, then performs update
 */
json_value_t* db_update_document_with_js(database_t* db, const char* collection_name, 
                                        const char* document_id, json_value_t* update_data, 
                                        const char* user_id);

/**
 * Enhanced document deletion with JavaScript triggers
 * Executes any JavaScript functions triggered by delete operations
 */
int db_delete_document_with_js(database_t* db, const char* collection_name, 
                              const char* document_id, const char* user_id);

/**
 * Execute JavaScript functions based on collection tags
 * Finds and executes all functions triggered by collection operations
 */
json_value_t* db_execute_collection_functions(database_t* db, const char* collection_name, 
                                             const char* operation, json_value_t* input_data, 
                                             const char* user_id);

/**
 * Batch execute JavaScript transformers for multiple documents
 * Efficiently transforms multiple documents in a single operation
 */
json_value_t* db_batch_transform_documents(database_t* db, const char* collection_name, 
                                          json_value_t* documents, const char* operation, 
                                          const char* user_id);

/**
 * Check if collection has JavaScript scripts configured
 * Quick check to determine if JavaScript processing is needed
 */
int db_collection_has_js_scripts(database_t* db, const char* collection_name);

#endif /* DATABASE_JS_INTEGRATION_H */