#include "database/database.h"
#include "database/document_storage.h"
#include "js/js_native_storage.h"
#include "js/js_engine.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include "rbac/rbac_db.h"
#include <string.h>

/* External references */
extern js_engine_t *g_js_engine;

/**
 * Enhanced document insertion with JavaScript validation and transformation
 */
json_value_t* db_insert_document_with_js(database_t* db, const char* collection_name, 
                                        json_value_t* document, const char* user_id) {
    if (!db || !collection_name || !document || !user_id) {
        LOG_ERROR("Invalid parameters for JavaScript-enhanced insert.");
        return NULL;
    }

    LOG_DEBUG("Starting JavaScript-enhanced document insertion for collection: %s", collection_name);

    /* 1. First, execute validators if JavaScript engine is available */
    json_value_t *validation_errors = NULL;
    if (g_js_engine) {
        LOG_DEBUG("Executing JavaScript validators for document.");
        int validation_result = js_native_execute_validators(g_js_engine, db, collection_name, 
                                                           document, user_id, &validation_errors);
        
        if (!validation_result) {
            LOG_WARNING("Document validation failed for collection: %s", collection_name);
            
            /* Create error response with validation details */
            json_value_t *error_response = json_create_object();
            json_object_set(error_response, "error", json_create_string("Document validation failed"));
            json_object_set(error_response, "validation_errors", 
                           validation_errors ? validation_errors : json_create_array());
            
            return error_response;
        }
        
        if (validation_errors) {
            json_free(validation_errors);
        }
    }

    /* 2. Execute transformers to modify the document before insertion */
    json_value_t *transformed_document = document;
    if (g_js_engine) {
        LOG_DEBUG("Executing JavaScript transformers for document.");
        transformed_document = js_native_execute_transformers(g_js_engine, db, collection_name, 
                                                            document, "insert", user_id);
        
        if (!transformed_document) {
            LOG_ERROR("Document transformation failed for collection: %s", collection_name);
            return NULL;
        }
    }

    /* 3. Perform the actual database insertion using virtual layer */
    /* Parse collection_name to extract library and collection */
    const char *slash_pos = strrchr(collection_name, '/');
    const char *library_part = "default";
    const char *collection_part = collection_name;
    if (slash_pos) {
        size_t lib_len = slash_pos - collection_name;
        char *library_copy = BUFFER_ALLOC(lib_len + 1);
        strncpy(library_copy, collection_name, lib_len);
        library_copy[lib_len] = '\0';
        library_part = library_copy;
        collection_part = slash_pos + 1;
    }
    
    /* Determine document type from collection */
    const char *doc_type = collection_part;
    if (strcmp(collection_part, "users") == 0) doc_type = "user";
    else if (strcmp(collection_part, "roles") == 0) doc_type = "role";
    else if (strcmp(collection_part, "sessions") == 0) doc_type = "session";
    
    json_value_t *result = virtual_insert(db, doc_type, library_part, collection_part, transformed_document, user_id);
    
    /* Clean up library copy if allocated */
    if (slash_pos) {
        BUFFER_FREE((void*)library_part);
    }
    
    /* 4. Clean up transformed document if it's different from original */
    if (transformed_document != document) {
        json_free(transformed_document);
    }

    LOG_DEBUG("JavaScript-enhanced document insertion completed for collection: %s", collection_name);
    return result;
}

/**
 * Enhanced document update with JavaScript validation and transformation
 */
json_value_t* db_update_document_with_js(database_t* db, const char* collection_name, 
                                        const char* document_id, json_value_t* update_data, 
                                        const char* user_id) {
    if (!db || !collection_name || !document_id || !update_data || !user_id) {
        LOG_ERROR("Invalid parameters for JavaScript-enhanced update.");
        return NULL;
    }

    LOG_DEBUG("Starting JavaScript-enhanced document update for collection: %s, document: %s", 
              collection_name, document_id);

    /* 1. Get the existing document first */
    json_value_t *existing_doc = db_get_document(db, STORAGE_LIBRARY, STORAGE_COLLECTION, document_id);
    if (!existing_doc) {
        LOG_ERROR("Document not found for update: %s", document_id);
        return NULL;
    }

    /* 2. Merge update data with existing document */
    json_value_t *merged_doc = json_clone(existing_doc);
    if (!merged_doc) {
        LOG_ERROR("Cannot clone existing document for update.");
        json_free(existing_doc);
        return NULL;
    }

    /* Simple merge - overwrite fields from update_data */
    if (update_data->type == JSON_OBJECT) {
        /* For now, just add a simple marker field to indicate update */
        json_object_set(merged_doc, "_updated_at", json_create_integer(time(NULL)));
        /* TODO: Implement proper JSON object merging with available functions */
    }

    json_free(existing_doc);

    /* 3. Execute validators if JavaScript engine is available */
    json_value_t *validation_errors = NULL;
    if (g_js_engine) {
        LOG_DEBUG("Executing JavaScript validators for document update.");
        int validation_result = js_native_execute_validators(g_js_engine, db, collection_name, 
                                                           merged_doc, user_id, &validation_errors);
        
        if (!validation_result) {
            LOG_WARNING("Document update validation failed for collection: %s", collection_name);
            json_free(merged_doc);
            
            /* Create error response with validation details */
            json_value_t *error_response = json_create_object();
            json_object_set(error_response, "error", json_create_string("Document validation failed"));
            json_object_set(error_response, "validation_errors", 
                           validation_errors ? validation_errors : json_create_array());
            
            return error_response;
        }
        
        if (validation_errors) {
            json_free(validation_errors);
        }
    }

    /* 4. Execute transformers to modify the document before update */
    json_value_t *transformed_document = merged_doc;
    if (g_js_engine) {
        LOG_DEBUG("Executing JavaScript transformers for document update.");
        transformed_document = js_native_execute_transformers(g_js_engine, db, collection_name, 
                                                            merged_doc, "update", user_id);
        
        if (!transformed_document) {
            LOG_ERROR("Document transformation failed for update in collection: %s", collection_name);
            json_free(merged_doc);
            return NULL;
        }
    }

    /* 5. Perform the actual database update using virtual layer */
    json_value_t *result = virtual_update(db, document_id, transformed_document);
    
    /* 6. Clean up */
    if (transformed_document != merged_doc) {
        json_free(transformed_document);
    }
    json_free(merged_doc);

    LOG_DEBUG("JavaScript-enhanced document update completed for collection: %s", collection_name);
    return result;
}

/**
 * Enhanced document deletion with JavaScript triggers
 */
int db_delete_document_with_js(database_t* db, const char* collection_name, 
                              const char* document_id, const char* user_id) {
    if (!db || !collection_name || !document_id || !user_id) {
        LOG_ERROR("Invalid parameters for JavaScript-enhanced delete.");
        return 0;
    }

    LOG_DEBUG("Starting JavaScript-enhanced document deletion for collection: %s, document: %s", 
              collection_name, document_id);

    /* 1. Get the document before deletion for JavaScript execution */
    json_value_t *document = NULL;
    if (g_js_engine) {
        document = db_get_document(db, STORAGE_LIBRARY, STORAGE_COLLECTION, document_id);
        if (document) {
            /* Execute any JavaScript functions triggered by delete operations */
            json_value_t *delete_results = js_native_execute_tagged_functions(g_js_engine, db, 
                                                                            collection_name, 
                                                                            document, "delete", user_id);
            if (delete_results) {
                LOG_DEBUG("Executed JavaScript functions for document deletion.");
                json_free(delete_results);
            }
        }
    }

    /* 2. Perform the actual database deletion using virtual layer */
    int result = virtual_delete(db, document_id);
    
    /* 3. Clean up */
    if (document) {
        json_free(document);
    }

    LOG_DEBUG("JavaScript-enhanced document deletion completed for collection: %s", collection_name);
    return result;
}

/**
 * Execute JavaScript functions based on collection tags
 */
json_value_t* db_execute_collection_functions(database_t* db, const char* collection_name, 
                                             const char* operation, json_value_t* input_data, 
                                             const char* user_id) {
    if (!db || !collection_name || !operation || !user_id || !g_js_engine) {
        LOG_ERROR("Invalid parameters for collection function execution.");
        return NULL;
    }

    LOG_DEBUG("Executing collection functions for collection: %s, operation: %s", 
              collection_name, operation);

    /* Find and execute scripts triggered by collection operations */
    json_value_t *triggered_scripts = js_native_find_triggered_scripts(db, collection_name, 
                                                                      operation, input_data);
    if (!triggered_scripts || json_array_size(triggered_scripts) == 0) {
        if (triggered_scripts) json_free(triggered_scripts);
        return json_create_array(); /* No functions found */
    }

    json_value_t *results = json_create_array();

    /* Execute each triggered script */
    for (size_t i = 0; i < json_array_size(triggered_scripts); i++) {
        json_value_t *script_json = json_array_get(triggered_scripts, i);
        if (!script_json) continue;

        js_script_metadata_t *script = js_native_script_metadata_from_json(script_json);
        if (!script || script->type != JS_SCRIPT_FUNCTION) {
            if (script) js_native_free_script_metadata(script);
            continue;
        }

        /* Check execution permissions */
        if (!js_native_check_execution_permission(db, user_id, script->id, operation)) {
            js_native_free_script_metadata(script);
            continue;
        }

        /* Execute function */
        js_execution_context_t context = {0};
        json_value_t *output_data = NULL;
        
        int execution_result = js_native_execute_script(g_js_engine, db, script->id, user_id, 
                                                      input_data, &output_data, &context);

        /* Store result */
        json_value_t *result = json_create_object();
        json_object_set(result, "script_id", json_create_string(script->id));
        json_object_set(result, "script_name", json_create_string(script->name));
        json_object_set(result, "success", json_create_boolean(execution_result));
        
        if (execution_result && output_data) {
            json_object_set(result, "result", output_data);
        } else if (!execution_result) {
            json_object_set(result, "error", json_create_string(context.error_message));
        }

        json_array_append(results, result);
        js_native_free_script_metadata(script);
    }

    json_free(triggered_scripts);
    return results;
}

/**
 * Batch execute JavaScript transformers for multiple documents
 */
json_value_t* db_batch_transform_documents(database_t* db, const char* collection_name, 
                                          json_value_t* documents, const char* operation, 
                                          const char* user_id) {
    if (!db || !collection_name || !documents || !operation || !user_id || !g_js_engine) {
        LOG_ERROR("Invalid parameters for batch document transformation.");
        return NULL;
    }

    if (documents->type != JSON_ARRAY) {
        LOG_ERROR("Documents parameter must be an array.");
        return NULL;
    }

    LOG_DEBUG("Batch transforming %zu documents for collection: %s", 
              json_array_size(documents), collection_name);

    json_value_t *transformed_docs = json_create_array();

    for (size_t i = 0; i < json_array_size(documents); i++) {
        json_value_t *doc = json_array_get(documents, i);
        if (!doc || doc->type != JSON_OBJECT) continue;

        /* Transform each document */
        json_value_t *transformed_doc = js_native_execute_transformers(g_js_engine, db, 
                                                                      collection_name, doc, 
                                                                      operation, user_id);
        if (transformed_doc) {
            json_array_append(transformed_docs, transformed_doc);
        } else {
            /* If transformation fails, include original document */
            json_array_append(transformed_docs, json_clone(doc));
        }
    }

    LOG_DEBUG("Batch transformation completed, processed %zu documents", 
              json_array_size(transformed_docs));
    return transformed_docs;
}

/**
 * Check if collection has JavaScript scripts configured
 */
int db_collection_has_js_scripts(database_t* db, const char* collection_name) {
    if (!db || !collection_name || !g_js_engine) {
        return 0;
    }

    /* Quick check for any enabled scripts for this collection */
    json_value_t *validators = js_native_list_scripts(db, JS_SCRIPT_VALIDATOR, collection_name, NULL);
    json_value_t *transformers = js_native_list_scripts(db, JS_SCRIPT_TRANSFORMER, collection_name, NULL);
    json_value_t *functions = js_native_list_scripts(db, JS_SCRIPT_FUNCTION, collection_name, NULL);

    int has_scripts = 0;
    if (validators && json_array_size(validators) > 0) has_scripts = 1;
    if (transformers && json_array_size(transformers) > 0) has_scripts = 1;
    if (functions && json_array_size(functions) > 0) has_scripts = 1;

    if (validators) json_free(validators);
    if (transformers) json_free(transformers);
    if (functions) json_free(functions);

    return has_scripts;
}