#include "js/js_engine.h"
#include "utils/logger.h"
#include "utils/js_file_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h> /* For PATH_MAX */
#include <errno.h> /* For errno and error codes */
#include <unistd.h> /* For getcwd */

/* This file contains the JavaScript engine implementation using QuickJS.
 * When USE_QUICKJS is defined, we provide the full implementation.
 * When USE_QUICKJS is not defined, we provide stub implementations that return appropriate error codes.
 */

#ifdef USE_QUICKJS

/* Forward declarations for JavaScript callback functions */
static JSValue js_db_get_collection(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_db_query_documents(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_db_get_document(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_db_insert_document(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_db_update_document(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_db_delete_document(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* Convert JSON value to JavaScript value */
static JSValue json_to_js(JSContext *ctx, json_value_t *json_val) {
    if (!json_val) {
        return JS_NULL;
    }
    
    switch (json_val->type) {
        case JSON_NULL:
            return JS_NULL;
            
        case JSON_BOOLEAN:
            return JS_NewBool(ctx, json_val->value.boolean);
            
        case JSON_INTEGER:
            return JS_NewInt64(ctx, json_val->value.integer);
            
        case JSON_NUMBER:
            return JS_NewFloat64(ctx, json_val->value.number);
            
        case JSON_STRING:
            return JS_NewString(ctx, json_val->value.string);
            
        case JSON_ARRAY: {
            JSValue js_array = JS_NewArray(ctx);
            for (size_t i = 0; i < json_array_size(json_val); i++) {
                json_value_t *item = json_array_get(json_val, i);
                JSValue js_item = json_to_js(ctx, item);
                JS_SetPropertyUint32(ctx, js_array, i, js_item);
            }
            return js_array;
        }
        
        case JSON_OBJECT: {
            JSValue js_obj = JS_NewObject(ctx);
            for (size_t i = 0; i < json_val->value.object.size; i++) {
                const char *key = json_val->value.object.entries[i].key;
                json_value_t *value = json_val->value.object.entries[i].value;
                JSValue js_value = json_to_js(ctx, value);
                JS_SetPropertyStr(ctx, js_obj, key, js_value);
            }
            return js_obj;
        }
        
        default:
            return JS_NULL;
    }
}

/* Convert JavaScript value to JSON value */
static json_value_t* js_to_json(JSContext *ctx, JSValueConst js_val) {
    if (JS_IsNull(js_val) || JS_IsUndefined(js_val)) {
        return json_create_null();
    }
    
    if (JS_IsBool(js_val)) {
        int val = JS_ToBool(ctx, js_val);
        return json_create_boolean(val);
    }
    
    if (JS_IsNumber(js_val)) {
        double d;
        JS_ToFloat64(ctx, &d, js_val);
        if (d == (int64_t)d) {
            return json_create_integer((int64_t)d);
        } else {
            return json_create_number(d);
        }
    }
    
    if (JS_IsString(js_val)) {
        const char *str = JS_ToCString(ctx, js_val);
        json_value_t *val = json_create_string(str);
        JS_FreeCString(ctx, str);
        return val;
    }
    
    if (JS_IsArray(ctx, js_val)) {
        json_value_t *arr = json_create_array();
        JSValue length_val = JS_GetPropertyStr(ctx, js_val, "length");
        int64_t length;
        JS_ToInt64(ctx, &length, length_val);
        JS_FreeValue(ctx, length_val);
        
        for (int i = 0; i < length; i++) {
            JSValue item = JS_GetPropertyUint32(ctx, js_val, i);
            json_value_t *json_item = js_to_json(ctx, item);
            json_array_append(arr, json_item);
            JS_FreeValue(ctx, item);
        }
        
        return arr;
    }
    
    if (JS_IsObject(js_val)) {
        json_value_t *obj = json_create_object();
        JSPropertyEnum *props;
        uint32_t prop_count;
        
        if (JS_GetOwnPropertyNames(ctx, &props, &prop_count, js_val, JS_GPN_STRING_MASK) == 0) {
            for (uint32_t i = 0; i < prop_count; i++) {
                JSValue prop_name = JS_AtomToString(ctx, props[i].atom);
                const char *key = JS_ToCString(ctx, prop_name);
                JSValue value = JS_GetProperty(ctx, js_val, props[i].atom);
                
                json_value_t *json_value = js_to_json(ctx, value);
                json_object_set(obj, key, json_value);
                
                JS_FreeCString(ctx, key);
                JS_FreeValue(ctx, prop_name);
                JS_FreeValue(ctx, value);
                JS_FreeAtom(ctx, props[i].atom);
            }
            js_free(ctx, props);
        }
        
        return obj;
    }
    
    /* Default to null for unsupported types */
    return json_create_null();
}

/* Initialize JavaScript engine */
js_engine_t* js_engine_init(database_t *db) {
    js_engine_t *engine = (js_engine_t*)malloc(sizeof(js_engine_t));
    if (!engine) {
        return NULL;
    }
    
    /* Initialize fields */
    engine->db = db;
    engine->last_error = NULL;
    
    /* Create JavaScript runtime and context */
    engine->rt = JS_NewRuntime();
    if (!engine->rt) {
        free(engine);
        return NULL;
    }
    
    engine->ctx = JS_NewContext(engine->rt);
    if (!engine->ctx) {
        JS_FreeRuntime(engine->rt);
        free(engine);
        return NULL;
    }
    
    /* Register database functions */
    js_register_db_functions(engine);
    
    return engine;
}

/* Free JavaScript engine */
void js_engine_free(js_engine_t *engine) {
    if (engine) {
        /* Free last error string if exists */
        if (engine->last_error) {
            free(engine->last_error);
            engine->last_error = NULL;
        }
        
        /* First run the garbage collector to clean up any pending objects */
        if (engine->rt) {
            JS_RunGC(engine->rt);
        }
        
        /* Free the context first */
        if (engine->ctx) {
            JS_FreeContext(engine->ctx);
            engine->ctx = NULL;
        }
        
        /* Free the runtime after the context */
        if (engine->rt) {
            JS_FreeRuntime(engine->rt);
            engine->rt = NULL;
        }
        
        /* Free the engine structure itself */
        free(engine);
    }
}

/* Evaluate JavaScript code */
int js_engine_eval(js_engine_t *engine, const char *script, char **result) {
    if (!engine || !script) {
        return 0;
    }
    
    JSValue val = JS_Eval(engine->ctx, script, strlen(script), "<input>", JS_EVAL_TYPE_GLOBAL);
    
    if (JS_IsException(val)) {
        JSValue exception = JS_GetException(engine->ctx);
        const char *str = JS_ToCString(engine->ctx, exception);
        js_set_error(engine, str);
        JS_FreeCString(engine->ctx, str);
        JS_FreeValue(engine->ctx, exception);
        JS_FreeValue(engine->ctx, val);
        return 0;
    }
    
    if (result) {
        if (JS_IsNull(val) || JS_IsUndefined(val)) {
            *result = strdup("");
        } else {
            JSValue json_val = JS_JSONStringify(engine->ctx, val, JS_NULL, JS_NULL);
            
            if (JS_IsException(json_val)) {
                *result = strdup("{\"error\":\"Cannot stringify result\"}");
                JS_FreeValue(engine->ctx, json_val);
            } else {
                const char *str = JS_ToCString(engine->ctx, json_val);
                if (str) {
                    *result = strdup(str);
                    JS_FreeCString(engine->ctx, str);
                } else {
                    *result = strdup("");
                }
                JS_FreeValue(engine->ctx, json_val);
            }
        }
    }
    
    JS_FreeValue(engine->ctx, val);
    return 1;
}

/* Evaluate JavaScript file */
int js_engine_eval_file(js_engine_t *engine, const char *file_path, char **result) {
    if (!engine || !file_path) {
        return 0;
    }

    /* Use JS file utility to find the file with enhanced path resolution */
    char resolved_path[PATH_MAX];
    if (!js_file_find(file_path, resolved_path, sizeof(resolved_path))) {
        /* File not found - log detailed error */
        char error_msg[PATH_MAX + 100];
        snprintf(error_msg, sizeof(error_msg), "Failed to find JavaScript file: %s", file_path);
        js_set_error(engine, error_msg);

        /* Log detailed search paths */
        js_file_log_not_found(file_path, LOG_LEVEL_ERROR);

        if (g_logger) {
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd))) {
                LOG_DEBUG("Working directory: %s", cwd);
            }
        }

        return 0;
    }

    /* Log file resolution for debugging */
    if (g_logger && strcmp(file_path, resolved_path) != 0) {
        LOG_DEBUG("Resolved JavaScript file path: %s -> %s", file_path, resolved_path);
    }

    /* Open file */
    FILE *file = fopen(resolved_path, "rb");
    if (!file) {
        char error_msg[PATH_MAX + 100];
        snprintf(error_msg, sizeof(error_msg), "Failed to open JavaScript file: %s (Error: %s)",
                resolved_path, strerror(errno));
        js_set_error(engine, error_msg);

        if (g_logger) {
            LOG_ERROR("%s", error_msg);
        }

        return 0;
    }

    /* Get file size */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    /* Check for empty file */
    if (file_size <= 0) {
        fclose(file);
        js_set_error(engine, "JavaScript file is empty");

        if (g_logger) {
            LOG_WARNING("JavaScript file is empty: %s", resolved_path);
        }

        return 0;
    }

    /* Read file contents */
    char *script = (char*)malloc(file_size + 1);
    if (!script) {
        fclose(file);
        js_set_error(engine, "Memory allocation failed");

        if (g_logger) {
            LOG_ERROR("Memory allocation failed when loading JavaScript file: %s", resolved_path);
        }

        return 0;
    }

    size_t read_size = fread(script, 1, file_size, file);
    fclose(file);

    /* Check if file was read correctly */
    if (read_size != file_size) {
        free(script);
        char error_msg[PATH_MAX + 100];
        snprintf(error_msg, sizeof(error_msg), "Failed to read JavaScript file: %s (Error: %s)",
                resolved_path, strerror(errno));
        js_set_error(engine, error_msg);

        if (g_logger) {
            LOG_ERROR("%s", error_msg);
        }

        return 0;
    }

    script[read_size] = '\0';

    /* Log successful file loading at debug level */
    if (g_logger) {
        LOG_DEBUG("Successfully loaded JavaScript file: %s (%ld bytes)", resolved_path, read_size);
    }

    /* Evaluate script */
    int ret = js_engine_eval(engine, script, result);

    /* Log evaluation result */
    if (g_logger) {
        if (ret) {
            LOG_DEBUG("JavaScript evaluation successful: %s", resolved_path);
        } else {
            LOG_ERROR("JavaScript evaluation failed: %s - %s",
                     resolved_path, engine->last_error ? engine->last_error : "Unknown error");
        }
    }

    free(script);
    return ret;
}

/* Register database functions */
void js_register_db_functions(js_engine_t *engine) {
    JSContext *ctx = engine->ctx;
    JSValue global_obj = JS_GetGlobalObject(ctx);
    
    /* Create database object */
    JSValue db_obj = JS_NewObject(ctx);
    
    /* Store database pointer as a global property */
    JS_SetOpaque(db_obj, engine);
    
    /* Register database functions */
    JS_SetPropertyStr(ctx, db_obj, "getCollection", 
                     JS_NewCFunction(ctx, js_db_get_collection, "getCollection", 1));
    
    JS_SetPropertyStr(ctx, db_obj, "queryDocuments", 
                     JS_NewCFunction(ctx, js_db_query_documents, "queryDocuments", 2));
    
    JS_SetPropertyStr(ctx, db_obj, "getDocument", 
                     JS_NewCFunction(ctx, js_db_get_document, "getDocument", 2));
    
    JS_SetPropertyStr(ctx, db_obj, "insertDocument", 
                     JS_NewCFunction(ctx, js_db_insert_document, "insertDocument", 2));
    
    JS_SetPropertyStr(ctx, db_obj, "updateDocument", 
                     JS_NewCFunction(ctx, js_db_update_document, "updateDocument", 3));
    
    JS_SetPropertyStr(ctx, db_obj, "deleteDocument", 
                     JS_NewCFunction(ctx, js_db_delete_document, "deleteDocument", 2));
    
    /* Add database object to global scope */
    JS_SetPropertyStr(ctx, global_obj, "db", db_obj);
    
    JS_FreeValue(ctx, global_obj);
}

/* JavaScript callback: db.getCollection(collectionName) */
static JSValue js_db_get_collection(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "Expected string collection name");
    }
    
    /* Get database from context */
    JSValue db_obj = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "db");
    JSClassID class_id = 0;
    js_engine_t *engine = JS_GetOpaque(db_obj, class_id);
    JS_FreeValue(ctx, db_obj);
    
    if (!engine || !engine->db) {
        return JS_ThrowInternalError(ctx, "Database not initialized");
    }
    
    /* Get collection name */
    const char *collection_name = JS_ToCString(ctx, argv[0]);
    
    /* Get collection */
    db_collection_t *collection = db_get_collection(engine->db, collection_name);
    JS_FreeCString(ctx, collection_name);
    
    if (!collection) {
        return JS_NULL;
    }
    
    /* Get documents from collection */
    json_value_t *documents = db_query_documents(engine->db, collection_name, NULL);
    
    /* Convert to JavaScript array */
    JSValue result = json_to_js(ctx, documents);
    
    /* Free JSON value */
    json_free(documents);
    
    return result;
}

/* JavaScript callback: db.queryDocuments(collectionName, query) */
static JSValue js_db_query_documents(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 2 || !JS_IsString(argv[0]) || !JS_IsObject(argv[1])) {
        return JS_ThrowTypeError(ctx, "Expected string collection name and object query");
    }
    
    /* Get database from context */
    JSValue db_obj = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "db");
    JSClassID class_id = 0;
    js_engine_t *engine = JS_GetOpaque(db_obj, class_id);
    JS_FreeValue(ctx, db_obj);
    
    if (!engine || !engine->db) {
        return JS_ThrowInternalError(ctx, "Database not initialized");
    }
    
    /* Get collection name */
    const char *collection_name = JS_ToCString(ctx, argv[0]);
    
    /* Convert query to JSON */
    json_value_t *query = js_to_json(ctx, argv[1]);
    
    /* Query documents */
    json_value_t *documents = db_query_documents(engine->db, collection_name, query);
    
    /* Free query */
    json_free(query);
    JS_FreeCString(ctx, collection_name);
    
    if (!documents) {
        return JS_NULL;
    }
    
    /* Convert to JavaScript array */
    JSValue result = json_to_js(ctx, documents);
    
    /* Free JSON value */
    json_free(documents);
    
    return result;
}

/* JavaScript callback: db.getDocument(collectionName, id) */
static JSValue js_db_get_document(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 2 || !JS_IsString(argv[0]) || !JS_IsString(argv[1])) {
        return JS_ThrowTypeError(ctx, "Expected string collection name and string document ID");
    }
    
    /* Get database from context */
    JSValue db_obj = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "db");
    JSClassID class_id = 0;
    js_engine_t *engine = JS_GetOpaque(db_obj, class_id);
    JS_FreeValue(ctx, db_obj);
    
    if (!engine || !engine->db) {
        return JS_ThrowInternalError(ctx, "Database not initialized");
    }
    
    /* Get collection name and document ID */
    const char *collection_name = JS_ToCString(ctx, argv[0]);
    const char *document_id = JS_ToCString(ctx, argv[1]);
    
    /* Get document */
    json_value_t *document = db_get_document(engine->db, collection_name, document_id);
    
    JS_FreeCString(ctx, collection_name);
    JS_FreeCString(ctx, document_id);
    
    if (!document) {
        return JS_NULL;
    }
    
    /* Convert to JavaScript object */
    JSValue result = json_to_js(ctx, document);
    
    /* Free JSON value */
    json_free(document);
    
    return result;
}

/* JavaScript callback: db.insertDocument(collectionName, document) */
static JSValue js_db_insert_document(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 2 || !JS_IsString(argv[0]) || !JS_IsObject(argv[1])) {
        return JS_ThrowTypeError(ctx, "Expected string collection name and object document");
    }
    
    /* Get database from context */
    JSValue db_obj = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "db");
    JSClassID class_id = 0;
    js_engine_t *engine = JS_GetOpaque(db_obj, class_id);
    JS_FreeValue(ctx, db_obj);
    
    if (!engine || !engine->db) {
        return JS_ThrowInternalError(ctx, "Database not initialized");
    }
    
    /* Get collection name */
    const char *collection_name = JS_ToCString(ctx, argv[0]);
    
    /* Convert document to JSON */
    json_value_t *document = js_to_json(ctx, argv[1]);
    
    /* Insert document */
    json_value_t *result = db_insert_document(engine->db, collection_name, document);
    
    /* Free resources */
    json_free(document);
    JS_FreeCString(ctx, collection_name);
    
    if (!result) {
        return JS_NULL;
    }
    
    /* Convert to JavaScript object */
    JSValue js_result = json_to_js(ctx, result);
    
    /* Free JSON value */
    json_free(result);
    
    return js_result;
}

/* JavaScript callback: db.updateDocument(collectionName, id, document) */
static JSValue js_db_update_document(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 3 || !JS_IsString(argv[0]) || !JS_IsString(argv[1]) || !JS_IsObject(argv[2])) {
        return JS_ThrowTypeError(ctx, "Expected string collection name, string document ID, and object document");
    }
    
    /* Get database from context */
    JSValue db_obj = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "db");
    JSClassID class_id = 0;
    js_engine_t *engine = JS_GetOpaque(db_obj, class_id);
    JS_FreeValue(ctx, db_obj);
    
    if (!engine || !engine->db) {
        return JS_ThrowInternalError(ctx, "Database not initialized");
    }
    
    /* Get collection name and document ID */
    const char *collection_name = JS_ToCString(ctx, argv[0]);
    const char *document_id = JS_ToCString(ctx, argv[1]);
    
    /* Convert document to JSON */
    json_value_t *document = js_to_json(ctx, argv[2]);
    
    /* Update document */
    json_value_t *result = db_update_document(engine->db, collection_name, document_id, document);
    
    /* Free resources */
    json_free(document);
    JS_FreeCString(ctx, collection_name);
    JS_FreeCString(ctx, document_id);
    
    if (!result) {
        return JS_NULL;
    }
    
    /* Convert to JavaScript object */
    JSValue js_result = json_to_js(ctx, result);
    
    /* Free JSON value */
    json_free(result);
    
    return js_result;
}

/* JavaScript callback: db.deleteDocument(collectionName, id) */
static JSValue js_db_delete_document(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 2 || !JS_IsString(argv[0]) || !JS_IsString(argv[1])) {
        return JS_ThrowTypeError(ctx, "Expected string collection name and string document ID");
    }
    
    /* Get database from context */
    JSValue db_obj = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "db");
    JSClassID class_id = 0;
    js_engine_t *engine = JS_GetOpaque(db_obj, class_id);
    JS_FreeValue(ctx, db_obj);
    
    if (!engine || !engine->db) {
        return JS_ThrowInternalError(ctx, "Database not initialized");
    }
    
    /* Get collection name and document ID */
    const char *collection_name = JS_ToCString(ctx, argv[0]);
    const char *document_id = JS_ToCString(ctx, argv[1]);
    
    /* Delete document */
    int result = db_delete_document(engine->db, collection_name, document_id);
    
    /* Free resources */
    JS_FreeCString(ctx, collection_name);
    JS_FreeCString(ctx, document_id);
    
    return JS_NewBool(ctx, result);
}

/* Execute JavaScript query */
json_value_t* js_execute_query(js_engine_t *engine, const char *collection_name, const char *query_script) {
    if (!engine || !collection_name || !query_script) {
        return NULL;
    }
    
    /* Create the query function */
    char script[4096];
    snprintf(script, sizeof(script),
             "function queryFilter(doc) {\n"
             "  try {\n"
             "    return (%s);\n"
             "  } catch (e) {\n"
             "    return false;\n"
             "  }\n"
             "}\n"
             "\n"
             "db.getCollection('%s').filter(queryFilter);",
             query_script, collection_name);
    
    /* Evaluate the script */
    JSValue result = JS_Eval(engine->ctx, script, strlen(script), "<query>", JS_EVAL_TYPE_GLOBAL);
    
    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(engine->ctx);
        const char *str = JS_ToCString(engine->ctx, exception);
        js_set_error(engine, str);
        JS_FreeCString(engine->ctx, str);
        JS_FreeValue(engine->ctx, exception);
        JS_FreeValue(engine->ctx, result);
        return NULL;
    }
    
    /* Convert result to JSON */
    json_value_t *json_result = js_to_json(engine->ctx, result);
    
    /* Free JavaScript value */
    JS_FreeValue(engine->ctx, result);
    
    return json_result;
}

/* Validate document using JavaScript */
int js_validate_document(js_engine_t *engine, const char *collection_name, json_value_t *document) {
    if (!engine || !collection_name || !document) {
        return 0;
    }
    
    /* Get validation script for collection */
    char rel_path[512];
    char script_path[PATH_MAX];
    snprintf(rel_path, sizeof(rel_path), "validators/%s.js", collection_name);

    /* Convert to absolute path - defined in main.c */
    extern void make_path_absolute(const char* rel_path, char* abs_path, size_t abs_path_size);
    make_path_absolute(rel_path, script_path, sizeof(script_path));

    /* Check if validation script exists */
    FILE *file = fopen(script_path, "rb");
    if (!file) {
        /* No validation script, document is valid */
        return 1;
    }
    fclose(file);
    
    /* Convert document to JavaScript object */
    JSValue js_doc = json_to_js(engine->ctx, document);
    
    /* Create global document variable */
    JSValue global = JS_GetGlobalObject(engine->ctx);
    JS_SetPropertyStr(engine->ctx, global, "document", JS_DupValue(engine->ctx, js_doc));
    JS_FreeValue(engine->ctx, global);
    
    /* Prepare validation script */
    char script[4096];
    snprintf(script, sizeof(script),
             "let validationErrors = [];\n"
             "let isValid = true;\n"
             "\n"
             "function validateDocument(doc) {\n"
             "  // This will be overridden by the validation script\n"
             "  return true;\n"
             "}\n"
             "\n"
             "function addError(field, message) {\n"
             "  validationErrors.push({ field, message });\n"
             "  isValid = false;\n"
             "}\n");
    
    /* Evaluate the setup script */
    JSValue setup_result = JS_Eval(engine->ctx, script, strlen(script), "<validation-setup>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(setup_result)) {
        JS_FreeValue(engine->ctx, setup_result);
        JS_FreeValue(engine->ctx, js_doc);
        return 0;
    }
    JS_FreeValue(engine->ctx, setup_result);
    
    /* Load and evaluate the validation script */
    char *validation_result = NULL;
    if (!js_engine_eval_file(engine, script_path, &validation_result)) {
        if (validation_result) free(validation_result);
        JS_FreeValue(engine->ctx, js_doc);
        return 0;
    }
    if (validation_result) free(validation_result);
    
    /* Execute validation */
    const char *validation_code = "validateDocument(document); isValid;";
    JSValue result = JS_Eval(engine->ctx, validation_code, strlen(validation_code), "<validation>", JS_EVAL_TYPE_GLOBAL);
    
    /* Check result */
    int is_valid = JS_ToBool(engine->ctx, result);
    JS_FreeValue(engine->ctx, result);
    
    /* If not valid, get validation errors */
    if (!is_valid) {
        JSValue global = JS_GetGlobalObject(engine->ctx);
        JSValue errors = JS_GetPropertyStr(engine->ctx, global, "validationErrors");
        
        /* Convert errors to string and set as last error */
        JSValue json_errors = JS_JSONStringify(engine->ctx, errors, JS_NULL, JS_NULL);
        const char *str = JS_ToCString(engine->ctx, json_errors);
        js_set_error(engine, str);
        
        JS_FreeCString(engine->ctx, str);
        JS_FreeValue(engine->ctx, json_errors);
        JS_FreeValue(engine->ctx, errors);
        JS_FreeValue(engine->ctx, global);
    }
    
    /* Clean up */
    global = JS_GetGlobalObject(engine->ctx);
    JS_SetPropertyStr(engine->ctx, global, "document", JS_NULL);
    JS_FreeValue(engine->ctx, global);
    JS_FreeValue(engine->ctx, js_doc);
    
    return is_valid;
}

/* Transform document using JavaScript */
json_value_t* js_transform_document(js_engine_t *engine, const char *collection_name, json_value_t *document, const char *operation) {
    if (!engine || !collection_name || !document || !operation) {
        return NULL;
    }
    
    /* Get transformation script for collection */
    char rel_path[512];
    char script_path[PATH_MAX];
    snprintf(rel_path, sizeof(rel_path), "transforms/%s.js", collection_name);

    /* Convert to absolute path - defined in main.c */
    extern void make_path_absolute(const char* rel_path, char* abs_path, size_t abs_path_size);
    make_path_absolute(rel_path, script_path, sizeof(script_path));

    /* Check if transformation script exists */
    FILE *file = fopen(script_path, "rb");
    if (!file) {
        /* No transformation script, return document as is */
        return json_clone(document);
    }
    fclose(file);
    
    /* Convert document to JavaScript object */
    JSValue js_doc = json_to_js(engine->ctx, document);
    
    /* Create global document variable */
    JSValue global = JS_GetGlobalObject(engine->ctx);
    JS_SetPropertyStr(engine->ctx, global, "document", JS_DupValue(engine->ctx, js_doc));
    JS_SetPropertyStr(engine->ctx, global, "operation", JS_NewString(engine->ctx, operation));
    JS_FreeValue(engine->ctx, global);
    
    /* Load and evaluate the transformation script */
    char *transform_result = NULL;
    if (!js_engine_eval_file(engine, script_path, &transform_result)) {
        if (transform_result) free(transform_result);
        JS_FreeValue(engine->ctx, js_doc);
        return NULL;
    }
    if (transform_result) free(transform_result);
    
    /* Execute transformation */
    const char *transform_code = "transformDocument(document, operation);";
    JSValue result = JS_Eval(engine->ctx, transform_code, strlen(transform_code), "<transform>", JS_EVAL_TYPE_GLOBAL);
    
    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(engine->ctx);
        const char *str = JS_ToCString(engine->ctx, exception);
        js_set_error(engine, str);
        JS_FreeCString(engine->ctx, str);
        JS_FreeValue(engine->ctx, exception);
        JS_FreeValue(engine->ctx, result);
        JS_FreeValue(engine->ctx, js_doc);
        return NULL;
    }
    
    /* Get the transformed document */
    global = JS_GetGlobalObject(engine->ctx);
    JSValue transformed = JS_GetPropertyStr(engine->ctx, global, "document");
    
    /* Convert result to JSON */
    json_value_t *json_result = js_to_json(engine->ctx, transformed);
    
    /* Clean up */
    JS_SetPropertyStr(engine->ctx, global, "document", JS_NULL);
    JS_SetPropertyStr(engine->ctx, global, "operation", JS_NULL);
    JS_FreeValue(engine->ctx, global);
    JS_FreeValue(engine->ctx, transformed);
    JS_FreeValue(engine->ctx, result);
    JS_FreeValue(engine->ctx, js_doc);
    
    return json_result;
}

/* Register user-defined function */
int js_register_user_function(js_engine_t *engine, const char *name, const char *script) {
    if (!engine || !name || !script) {
        return 0;
    }
    
    /* Get functions directory path */
    char functions_dir[PATH_MAX] = {0};

    /* Convert to absolute path - defined in main.c */
    extern void make_path_absolute(const char* rel_path, char* abs_path, size_t abs_path_size);
    make_path_absolute("functions", functions_dir, sizeof(functions_dir));

    /* Create function directory if it doesn't exist */
    struct stat st = {0};
    if (stat(functions_dir, &st) == -1) {
        mkdir(functions_dir, 0755);
    }

    /* Save function to file */
    char script_path[PATH_MAX];
    snprintf(script_path, sizeof(script_path), "%s/%s.js", functions_dir, name);
    
    FILE *file = fopen(script_path, "wb");
    if (!file) {
        js_set_error(engine, "Failed to save function");
        return 0;
    }
    
    fwrite(script, 1, strlen(script), file);
    fclose(file);
    
    return 1;
}

/* Call user-defined function */
int js_call_user_function(js_engine_t *engine, const char *name, json_value_t *args, json_value_t **result) {
    if (!engine || !name || !args) {
        return 0;
    }
    
    /* Get function script */
    char rel_path[512];
    char script_path[PATH_MAX];
    snprintf(rel_path, sizeof(rel_path), "functions/%s.js", name);

    /* Convert to absolute path - defined in main.c */
    extern void make_path_absolute(const char* rel_path, char* abs_path, size_t abs_path_size);
    make_path_absolute(rel_path, script_path, sizeof(script_path));

    /* Check if function exists */
    FILE *file = fopen(script_path, "rb");
    if (!file) {
        js_set_error(engine, "Function not found");
        return 0;
    }
    fclose(file);
    
    /* Convert arguments to JavaScript */
    JSValue js_args = json_to_js(engine->ctx, args);
    
    /* Create global args variable */
    JSValue global = JS_GetGlobalObject(engine->ctx);
    JS_SetPropertyStr(engine->ctx, global, "args", JS_DupValue(engine->ctx, js_args));
    JS_FreeValue(engine->ctx, global);
    
    /* Load and evaluate the function script */
    char *function_result = NULL;
    if (!js_engine_eval_file(engine, script_path, &function_result)) {
        if (function_result) free(function_result);
        JS_FreeValue(engine->ctx, js_args);
        return 0;
    }
    if (function_result) free(function_result);
    
    /* Call the function */
    const char *call_code = "typeof userFunction === 'function' ? userFunction(args) : null;";
    JSValue js_result = JS_Eval(engine->ctx, call_code, strlen(call_code), "<function-call>", JS_EVAL_TYPE_GLOBAL);
    
    if (JS_IsException(js_result)) {
        JSValue exception = JS_GetException(engine->ctx);
        const char *str = JS_ToCString(engine->ctx, exception);
        js_set_error(engine, str);
        JS_FreeCString(engine->ctx, str);
        JS_FreeValue(engine->ctx, exception);
        JS_FreeValue(engine->ctx, js_result);
        JS_FreeValue(engine->ctx, js_args);
        return 0;
    }
    
    /* Convert result to JSON */
    if (result) {
        *result = js_to_json(engine->ctx, js_result);
    }
    
    /* Clean up */
    global = JS_GetGlobalObject(engine->ctx);
    JS_SetPropertyStr(engine->ctx, global, "args", JS_NULL);
    JS_FreeValue(engine->ctx, global);
    JS_FreeValue(engine->ctx, js_result);
    JS_FreeValue(engine->ctx, js_args);
    
    return 1;
}

/* Get last error message */
const char* js_get_last_error(js_engine_t *engine) {
    return engine ? engine->last_error : NULL;
}

/* Set error message */
void js_set_error(js_engine_t *engine, const char *error) {
    if (engine) {
        if (engine->last_error) {
            free(engine->last_error);
        }
        engine->last_error = error ? strdup(error) : NULL;
    }
}

/* Execute JavaScript file */
int js_execute_file(js_engine_t* engine, const char* file_path) {
    if (!engine || !file_path) {
        return 0;
    }

    /* Extra safety check - ensure the JS context is valid */
#ifdef USE_QUICKJS
    if (!engine->ctx || !engine->rt) {
        if (g_logger) {
            LOG_ERROR("JavaScript engine context is invalid");
        } else {
            fprintf(stderr, "Error: JavaScript engine context is invalid\n");
        }
        return 0;
    }
#endif

    /* Use JS file utility to find the file with enhanced path resolution */
    char resolved_path[PATH_MAX];
    if (!js_file_find(file_path, resolved_path, sizeof(resolved_path))) {
        /* File not found - log detailed error */
        char error_msg[PATH_MAX + 100];
        snprintf(error_msg, sizeof(error_msg), "Failed to find JavaScript file: %s", file_path);
        js_set_error(engine, error_msg);

        /* Log detailed search paths */
        js_file_log_not_found(file_path, LOG_LEVEL_ERROR);

        if (g_logger) {
            LOG_ERROR("Failed to find JavaScript file for execution: %s", file_path);
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd))) {
                LOG_DEBUG("Working directory: %s", cwd);
            }
        }

        return 0;
    }

    /* Log file resolution for debugging */
    if (g_logger && strcmp(file_path, resolved_path) != 0) {
        LOG_INFO("Executing JavaScript file: %s -> %s", file_path, resolved_path);
    } else if (g_logger) {
        LOG_INFO("Executing JavaScript file: %s", file_path);
    }

    /* Read file content */
    FILE* file = fopen(resolved_path, "r");
    if (!file) {
        char error_msg[PATH_MAX + 100];
        snprintf(error_msg, sizeof(error_msg), "Failed to open JavaScript file: %s (Error: %s)",
                resolved_path, strerror(errno));
        js_set_error(engine, error_msg);

        if (g_logger) {
            LOG_ERROR("%s", error_msg);
        }

        return 0;
    }

    /* Get file size */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    /* Check for empty file */
    if (file_size <= 0) {
        fclose(file);
        js_set_error(engine, "JavaScript file is empty");

        if (g_logger) {
            LOG_WARNING("JavaScript file is empty: %s", resolved_path);
        }

        return 0;
    }

    /* Allocate buffer for file content */
    char* script = (char*)malloc(file_size + 1);
    if (!script) {
        fclose(file);
        js_set_error(engine, "Memory allocation failed");

        if (g_logger) {
            LOG_ERROR("Memory allocation failed when loading JavaScript file: %s", resolved_path);
        }

        return 0;
    }

    /* Read file content */
    size_t read_size = fread(script, 1, file_size, file);
    fclose(file);

    /* Check if file was read correctly */
    if (read_size != file_size) {
        free(script);
        char error_msg[PATH_MAX + 100];
        snprintf(error_msg, sizeof(error_msg), "Failed to read JavaScript file: %s (Error: %s)",
                resolved_path, strerror(errno));
        js_set_error(engine, error_msg);

        if (g_logger) {
            LOG_ERROR("%s", error_msg);
        }

        return 0;
    }

    /* Null-terminate script */
    script[file_size] = '\0';

    /* Log successful file loading at debug level */
    if (g_logger) {
        LOG_DEBUG("Successfully loaded JavaScript file for execution: %s (%ld bytes)", resolved_path, read_size);
    }

    /* Evaluate script */
    char* result = NULL;
    int success = js_engine_eval(engine, script, &result);

    /* Log evaluation result */
    if (g_logger) {
        if (success) {
            LOG_DEBUG("JavaScript execution successful: %s", resolved_path);
            if (result && strlen(result) > 0) {
                LOG_DEBUG("Execution result: %s", result);
            }
        } else {
            LOG_ERROR("JavaScript execution failed: %s - %s",
                     resolved_path, engine->last_error ? engine->last_error : "Unknown error");
        }
    }

    /* Cleanup */
    free(script);
    if (result) {
        free(result);
    }

    return success;
}

#else /* !USE_QUICKJS - Provide stub implementations */

/* Initialize JavaScript engine */
js_engine_t* js_engine_init(database_t *db) {
    /* No QuickJS available, return NULL */
    return NULL;
}

/* Free JavaScript engine */
void js_engine_free(js_engine_t *engine) {
    /* No operation needed */
}

/* JavaScript evaluation */
int js_engine_eval(js_engine_t *engine, const char *script, char **result) {
    if (result) {
        *result = NULL;
    }
    return 0;
}

int js_engine_eval_file(js_engine_t *engine, const char *file_path, char **result) {
    if (result) {
        *result = NULL;
    }
    return 0;
}

/* Execute JavaScript file */
int js_execute_file(js_engine_t* engine, const char* file_path) {
    return 0;
}

/* Register database functions */
void js_register_db_functions(js_engine_t *engine) {
    /* No operation needed */
}

/* JavaScript query execution */
json_value_t* js_execute_query(js_engine_t *engine, const char *collection_name, const char *query_script) {
    return NULL;
}

/* JavaScript document validation */
int js_validate_document(js_engine_t *engine, const char *collection_name, json_value_t *document) {
    /* Default validation passes when JavaScript is not available */
    return 1;
}

/* JavaScript document transformation */
json_value_t* js_transform_document(js_engine_t *engine, const char *collection_name, json_value_t *document, const char *operation) {
    /* Return a copy of the original document when JavaScript is not available */
    return json_clone(document);
}

/* User-defined functions */
int js_register_user_function(js_engine_t *engine, const char *name, const char *script) {
    return 0;
}

int js_call_user_function(js_engine_t *engine, const char *name, json_value_t *args, json_value_t **result) {
    if (result) {
        *result = NULL;
    }
    return 0;
}

/* Error handling */
const char* js_get_last_error(js_engine_t *engine) {
    return "JavaScript support is not available in this build";
}

void js_set_error(js_engine_t *engine, const char *error) {
    /* No operation needed */
}

#endif /* USE_QUICKJS */