#include "js/js_native_storage.h"
#include "utils/logger.h"
#include "utils/metrics.h"
#include "database/system_schemas.h"
#include "rbac/rbac_db.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <float.h>

/* System collection names for JavaScript storage */
#define JS_VALIDATORS_COLLECTION "_validators"
#define JS_TRANSFORMERS_COLLECTION "_transformers"
#define JS_FUNCTIONS_COLLECTION "_functions"
/* Use existing system metrics collection for JavaScript execution metrics */
#define JS_EXECUTION_METRICS_COLLECTION "_metrics"

const char* js_script_type_to_string(js_script_type_t type) {
    switch (type) {
        case JS_SCRIPT_VALIDATOR: return "validator";
        case JS_SCRIPT_TRANSFORMER: return "transformer";
        case JS_SCRIPT_FUNCTION: return "function";
        default: return "unknown";
    }
}

const char* js_script_type_to_collection(js_script_type_t type) {
    switch (type) {
        case JS_SCRIPT_VALIDATOR: return JS_VALIDATORS_COLLECTION;
        case JS_SCRIPT_TRANSFORMER: return JS_TRANSFORMERS_COLLECTION;
        case JS_SCRIPT_FUNCTION: return JS_FUNCTIONS_COLLECTION;
        default: return NULL;
    }
}

static js_script_type_t js_script_type_from_string(const char *type_str) {
    if (strcmp(type_str, "validator") == 0) return JS_SCRIPT_VALIDATOR;
    if (strcmp(type_str, "transformer") == 0) return JS_SCRIPT_TRANSFORMER;
    if (strcmp(type_str, "function") == 0) return JS_SCRIPT_FUNCTION;
    return JS_SCRIPT_VALIDATOR; /* Default */
}

/* Initialize native JavaScript storage system */
int js_native_storage_init(database_t *db) {
    if (!db) {
        LOG_ERROR("Database pointer is NULL.");
        return 0;
    }

    LOG_INFO("Initializing native JavaScript storage system.");

    /* Create validators collection */
    if (!db_collection_exists(db, JS_VALIDATORS_COLLECTION)) {
        if (db_create_collection(db, JS_VALIDATORS_COLLECTION) != 0) {
            LOG_ERROR("Cannot create JavaScript validators collection.");
            return 0;
        }
        LOG_DEBUG("Created collection: %s", JS_VALIDATORS_COLLECTION);
    }

    /* Create transformers collection */
    if (!db_collection_exists(db, JS_TRANSFORMERS_COLLECTION)) {
        if (db_create_collection(db, JS_TRANSFORMERS_COLLECTION) != 0) {
            LOG_ERROR("Cannot create JavaScript transformers collection.");
            return 0;
        }
        LOG_DEBUG("Created collection: %s", JS_TRANSFORMERS_COLLECTION);
    }

    /* Create functions collection */
    if (!db_collection_exists(db, JS_FUNCTIONS_COLLECTION)) {
        if (db_create_collection(db, JS_FUNCTIONS_COLLECTION) != 0) {
            LOG_ERROR("Cannot create JavaScript functions collection.");
            return 0;
        }
        LOG_DEBUG("Created collection: %s", JS_FUNCTIONS_COLLECTION);
    }

    /* Create execution metrics collection */
    if (!db_collection_exists(db, JS_EXECUTION_METRICS_COLLECTION)) {
        if (db_create_collection(db, JS_EXECUTION_METRICS_COLLECTION) != 0) {
            LOG_ERROR("Cannot create JavaScript execution metrics collection.");
            return 0;
        }
        LOG_DEBUG("Created collection: %s", JS_EXECUTION_METRICS_COLLECTION);
    }

    LOG_INFO("Native JavaScript storage system initialized successfully.");
    return 1;
}

/* Generate unique script ID using standard document ID format */
void js_native_generate_script_id(char *buffer, size_t buffer_size) {
//     time_t now = time(NULL);
    int random_part = rand() % 10000;
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    snprintf(buffer, buffer_size, "doc-%ld-%09ld-%04x", 
             ts.tv_sec, ts.tv_nsec, random_part & 0xFFFF);
}

/* Create script metadata structure */
js_script_metadata_t* js_native_create_script_metadata(void) {
    js_script_metadata_t *metadata = calloc(1, sizeof(js_script_metadata_t));
    if (!metadata) {
        LOG_ERROR("Cannot allocate memory for script metadata.");
        return NULL;
    }

    /* Initialize with default values */
    metadata->type = JS_SCRIPT_FUNCTION;
    metadata->trigger_type = JS_TRIGGER_MANUAL;
    metadata->trigger_tags = json_create_array();
    metadata->rbac_permissions = json_create_array();
    metadata->execution_stats = json_create_object();
    metadata->created_at = time(NULL);
    metadata->updated_at = time(NULL);
    metadata->version = 1;
    metadata->enabled = 1;

    return metadata;
}

/* Free script metadata structure */
void js_native_free_script_metadata(js_script_metadata_t *metadata) {
    if (!metadata) return;

    if (metadata->script_code) {
        free(metadata->script_code);
    }
    if (metadata->trigger_tags) {
        json_free(metadata->trigger_tags);
    }
    if (metadata->rbac_permissions) {
        json_free(metadata->rbac_permissions);
    }
    if (metadata->execution_stats) {
        json_free(metadata->execution_stats);
    }
    
    free(metadata);
}

/* Convert script metadata to JSON */
json_value_t* js_native_script_metadata_to_json(js_script_metadata_t *metadata) {
    if (!metadata) return NULL;

    json_value_t *json = json_create_object();
    
    json_object_set(json, "uuid", json_create_string(metadata->id));
    json_object_set(json, "name", json_create_string(metadata->name));
    json_object_set(json, "description", json_create_string(metadata->description));
    json_object_set(json, "type", json_create_string(js_script_type_to_string(metadata->type)));
    json_object_set(json, "collection_pattern", json_create_string(metadata->collection_pattern));
    json_object_set(json, "trigger_type", json_create_integer(metadata->trigger_type));
    json_object_set(json, "trigger_tags", json_clone(metadata->trigger_tags));
    json_object_set(json, "script_code", json_create_string(metadata->script_code ? metadata->script_code : ""));
    json_object_set(json, "created_by", json_create_string(metadata->created_by));
    json_object_set(json, "created_at", json_create_integer(metadata->created_at));
    json_object_set(json, "updated_at", json_create_integer(metadata->updated_at));
    json_object_set(json, "version", json_create_integer(metadata->version));
    json_object_set(json, "enabled", json_create_boolean(metadata->enabled));
    json_object_set(json, "rbac_permissions", json_clone(metadata->rbac_permissions));
    json_object_set(json, "execution_stats", json_clone(metadata->execution_stats));

    return json;
}

/* Create script metadata from JSON */
js_script_metadata_t* js_native_script_metadata_from_json(json_value_t *json) {
    if (!json || json->type != JSON_OBJECT) return NULL;

    js_script_metadata_t *metadata = js_native_create_script_metadata();
    if (!metadata) return NULL;

    json_value_t *val;

    /* Extract ID */
    val = json_object_get(json, "uuid");
    if (val && val->type == JSON_STRING) {
        strncpy(metadata->id, val->value.string, sizeof(metadata->id) - 1);
    }

    /* Extract name */
    val = json_object_get(json, "name");
    if (val && val->type == JSON_STRING) {
        strncpy(metadata->name, val->value.string, sizeof(metadata->name) - 1);
    }

    /* Extract description */
    val = json_object_get(json, "description");
    if (val && val->type == JSON_STRING) {
        strncpy(metadata->description, val->value.string, sizeof(metadata->description) - 1);
    }

    /* Extract type */
    val = json_object_get(json, "type");
    if (val && val->type == JSON_STRING) {
        metadata->type = js_script_type_from_string(val->value.string);
    }

    /* Extract collection pattern */
    val = json_object_get(json, "collection_pattern");
    if (val && val->type == JSON_STRING) {
        strncpy(metadata->collection_pattern, val->value.string, sizeof(metadata->collection_pattern) - 1);
    }

    /* Extract trigger type */
    val = json_object_get(json, "trigger_type");
    if (val && val->type == JSON_INTEGER) {
        metadata->trigger_type = (js_trigger_type_t)val->value.integer;
    }

    /* Extract trigger tags */
    val = json_object_get(json, "trigger_tags");
    if (val) {
        json_free(metadata->trigger_tags);
        metadata->trigger_tags = json_clone(val);
    }

    /* Extract script code */
    val = json_object_get(json, "script_code");
    if (val && val->type == JSON_STRING) {
        metadata->script_code = strdup(val->value.string);
    }

    /* Extract created_by */
    val = json_object_get(json, "created_by");
    if (val && val->type == JSON_STRING) {
        strncpy(metadata->created_by, val->value.string, sizeof(metadata->created_by) - 1);
    }

    /* Extract timestamps */
    val = json_object_get(json, "created_at");
    if (val && val->type == JSON_INTEGER) {
        metadata->created_at = val->value.integer;
    }

    val = json_object_get(json, "updated_at");
    if (val && val->type == JSON_INTEGER) {
        metadata->updated_at = val->value.integer;
    }

    /* Extract version */
    val = json_object_get(json, "version");
    if (val && val->type == JSON_INTEGER) {
        metadata->version = val->value.integer;
    }

    /* Extract enabled flag */
    val = json_object_get(json, "enabled");
    if (val && val->type == JSON_BOOLEAN) {
        metadata->enabled = val->value.boolean;
    }

    /* Extract RBAC permissions */
    val = json_object_get(json, "rbac_permissions");
    if (val) {
        json_free(metadata->rbac_permissions);
        metadata->rbac_permissions = json_clone(val);
    }

    /* Extract execution stats */
    val = json_object_get(json, "execution_stats");
    if (val) {
        json_free(metadata->execution_stats);
        metadata->execution_stats = json_clone(val);
    }

    return metadata;
}

/* Store JavaScript script in native database storage */
int js_native_store_script(database_t *db, const char *user_id, js_script_metadata_t *metadata) {
    if (!db || !user_id || !metadata) {
        LOG_ERROR("Invalid parameters for storing JavaScript script.");
        return 0;
    }

    /* Generate ID if not provided */
    if (strlen(metadata->id) == 0) {
        js_native_generate_script_id(metadata->id, sizeof(metadata->id));
    }

    /* Set created_by */
    strncpy(metadata->created_by, user_id, sizeof(metadata->created_by) - 1);
    metadata->updated_at = time(NULL);

    /* Get target collection */
    const char *collection_name = js_script_type_to_collection(metadata->type);
    if (!collection_name) {
        LOG_ERROR("Invalid script type: %d", metadata->type);
        return 0;
    }

    /* Convert to JSON */
    json_value_t *script_doc = js_native_script_metadata_to_json(metadata);
    if (!script_doc) {
        LOG_ERROR("Cannot convert script metadata to JSON.");
        return 0;
    }

    /* Insert into database */
    json_value_t *result = db_insert_document(db, collection_name, script_doc);
    json_free(script_doc);

    if (!result) {
        LOG_ERROR("Cannot store JavaScript script in database.");
        return 0;
    }

    json_free(result);
    LOG_INFO("JavaScript script stored successfully: %s (type: %s)", 
             metadata->id, js_script_type_to_string(metadata->type));
    return 1;
}

/* Retrieve JavaScript script by ID */
js_script_metadata_t* js_native_get_script(database_t *db, const char *script_id) {
    if (!db || !script_id) {
        LOG_ERROR("Invalid parameters for retrieving JavaScript script.");
        return NULL;
    }

    /* Try to find script in all script collections */
    const char *collections[] = {
        JS_VALIDATORS_COLLECTION,
        JS_TRANSFORMERS_COLLECTION,
        JS_FUNCTIONS_COLLECTION
    };

    for (size_t i = 0; i < sizeof(collections) / sizeof(collections[0]); i++) {
        json_value_t *script_doc = db_get_document(db, collections[i], script_id);
        if (script_doc) {
            js_script_metadata_t *metadata = js_native_script_metadata_from_json(script_doc);
            json_free(script_doc);
            return metadata;
        }
    }

    LOG_DEBUG("JavaScript script not found: %s", script_id);
    return NULL;
}

/* Check if user has permission to execute script */
int js_native_check_execution_permission(database_t *db, const char *user_id, 
                                        const char *script_id, const char *operation) {
    if (!db || !user_id || !script_id) {
        LOG_ERROR("Invalid parameters for permission check.");
        return 0;
    }
    
    (void)operation; /* Suppress unused parameter warning */

    /* Get script metadata */
    js_script_metadata_t *metadata = js_native_get_script(db, script_id);
    if (!metadata) {
        LOG_ERROR("Script not found for permission check: %s", script_id);
        return 0;
    }

    /* Check if script is enabled */
    if (!metadata->enabled) {
        LOG_WARNING("Attempted to execute disabled script: %s", script_id);
        js_native_free_script_metadata(metadata);
        return 0;
    }

    /* If script has no RBAC permissions specified, allow execution */
    if (!metadata->rbac_permissions || json_array_size(metadata->rbac_permissions) == 0) {
        js_native_free_script_metadata(metadata);
        return 1;
    }

    /* Check RBAC permissions for each required permission */
    for (size_t i = 0; i < json_array_size(metadata->rbac_permissions); i++) {
        json_value_t *perm = json_array_get(metadata->rbac_permissions, i);
        if (perm && perm->type == JSON_STRING) {
            /* Check if user has this permission */
            /* This would integrate with the existing RBAC system */
            /* For now, use simplified permission checking */
            if (!rbac_db_check_permission(db, user_id, RBAC_COLLECTION, 
                                        metadata->collection_pattern, RBAC_WRITE)) {
                LOG_WARNING("User %s lacks permission %s for script %s", 
                           user_id, perm->value.string, script_id);
                js_native_free_script_metadata(metadata);
                return 0;
            }
        }
    }

    js_native_free_script_metadata(metadata);
    return 1;
}

/* Record script execution metrics */
int js_native_record_execution_metrics(database_t *db, js_execution_context_t *context) {
    if (!db || !context) {
        LOG_ERROR("Invalid parameters for recording execution metrics.");
        return 0;
    }

    /* Calculate execution time in milliseconds */
    double execution_time_ms = 
        (context->end_time.tv_sec - context->start_time.tv_sec) * 1000.0 +
        (context->end_time.tv_nsec - context->start_time.tv_nsec) / 1000000.0;

    /* Create metrics document */
    json_value_t *metrics_doc = json_create_object();
    json_object_set(metrics_doc, "script_id", json_create_string(context->script_id));
    json_object_set(metrics_doc, "script_type", json_create_string(js_script_type_to_string(context->script_type)));
    json_object_set(metrics_doc, "collection_name", json_create_string(context->collection_name));
    json_object_set(metrics_doc, "operation", json_create_string(context->operation));
    json_object_set(metrics_doc, "execution_time_ms", json_create_number(execution_time_ms));
    json_object_set(metrics_doc, "memory_before", json_create_integer(context->memory_before));
    json_object_set(metrics_doc, "memory_after", json_create_integer(context->memory_after));
    json_object_set(metrics_doc, "memory_delta", json_create_integer(context->memory_after - context->memory_before));
    json_object_set(metrics_doc, "success", json_create_boolean(context->success));
    json_object_set(metrics_doc, "timestamp", json_create_integer(time(NULL)));

    if (!context->success && strlen(context->error_message) > 0) {
        json_object_set(metrics_doc, "error_message", json_create_string(context->error_message));
    }

    /* Store input/output data sizes for analysis */
    if (context->input_data) {
        char *input_str = json_stringify(context->input_data);
        if (input_str) {
            json_object_set(metrics_doc, "input_size_bytes", json_create_integer(strlen(input_str)));
            free(input_str);
        }
    }

    if (context->output_data) {
        char *output_str = json_stringify(context->output_data);
        if (output_str) {
            json_object_set(metrics_doc, "output_size_bytes", json_create_integer(strlen(output_str)));
            free(output_str);
        }
    }

    /* Insert metrics document */
    json_value_t *result = db_insert_document(db, JS_EXECUTION_METRICS_COLLECTION, metrics_doc);
    json_free(metrics_doc);

    if (!result) {
        LOG_ERROR("Cannot record JavaScript execution metrics.");
        return 0;
    }

    json_free(result);
    
    /* Update global metrics for real-time monitoring */
    /* TODO: Implement proper metrics integration when metrics API is available */
    LOG_DEBUG("JavaScript execution metrics: success=%d, time=%.2fms", 
              context->success, execution_time_ms);

    return 1;
}

/* Find scripts triggered by document operation */
json_value_t* js_native_find_triggered_scripts(database_t *db, const char *collection_name,
                                               const char *operation, json_value_t *document) {
    if (!db || !collection_name || !operation) {
        LOG_ERROR("Invalid parameters for finding triggered scripts.");
        return NULL;
    }

    json_value_t *triggered_scripts = json_create_array();

    /* Check all script types */
    const char *collections[] = {
        JS_VALIDATORS_COLLECTION,
        JS_TRANSFORMERS_COLLECTION,
        JS_FUNCTIONS_COLLECTION
    };

    for (size_t i = 0; i < sizeof(collections) / sizeof(collections[0]); i++) {
        /* Query for scripts that match the collection pattern and are enabled */
        json_value_t *query = json_create_object();
        json_object_set(query, "enabled", json_create_boolean(1));
        json_object_set(query, "collection_pattern", json_create_string(collection_name));

        json_value_t *scripts = db_query_documents(db, collections[i], query);
        json_free(query);

        if (scripts && scripts->type == JSON_ARRAY) {
            for (size_t j = 0; j < json_array_size(scripts); j++) {
                json_value_t *script = json_array_get(scripts, j);
                if (script && script->type == JSON_OBJECT) {
                    /* Check trigger conditions */
                    json_value_t *trigger_type_val = json_object_get(script, "trigger_type");
                    if (trigger_type_val && trigger_type_val->type == JSON_INTEGER) {
                        js_trigger_type_t trigger_type = (js_trigger_type_t)trigger_type_val->value.integer;
                        
                        int should_trigger = 0;
                        
                        switch (trigger_type) {
                            case JS_TRIGGER_OPERATION:
                                should_trigger = 1; /* Always trigger on operations */
                                break;
                                
                            case JS_TRIGGER_DOCUMENT_TAG:
                                if (document) {
                                    json_value_t *doc_tags = json_object_get(document, "tags");
                                    json_value_t *script_tags = json_object_get(script, "trigger_tags");
                                    
                                    if (doc_tags && script_tags && 
                                        doc_tags->type == JSON_ARRAY && script_tags->type == JSON_ARRAY) {
                                        /* Check if any document tag matches script trigger tags */
                                        for (size_t dt = 0; dt < json_array_size(doc_tags); dt++) {
                                            json_value_t *doc_tag = json_array_get(doc_tags, dt);
                                            if (doc_tag && doc_tag->type == JSON_STRING) {
                                                for (size_t st = 0; st < json_array_size(script_tags); st++) {
                                                    json_value_t *script_tag = json_array_get(script_tags, st);
                                                    if (script_tag && script_tag->type == JSON_STRING &&
                                                        strcmp(doc_tag->value.string, script_tag->value.string) == 0) {
                                                        should_trigger = 1;
                                                        break;
                                                    }
                                                }
                                                if (should_trigger) break;
                                            }
                                        }
                                    }
                                }
                                break;
                                
                            case JS_TRIGGER_COLLECTION_TAG:
                                /* This would check collection-level tags - implementation depends on collection metadata */
                                should_trigger = 1; /* For now, trigger for all collections */
                                break;
                                
                            case JS_TRIGGER_MANUAL:
                            case JS_TRIGGER_NONE:
                            default:
                                should_trigger = 0;
                                break;
                        }
                        
                        if (should_trigger) {
                            json_array_append(triggered_scripts, json_clone(script));
                        }
                    }
                }
            }
        }
        
        if (scripts) {
            json_free(scripts);
        }
    }

    return triggered_scripts;
}

/* Validate script syntax */
int js_native_validate_script_syntax(js_engine_t *engine, const char *script_code, 
                                    char **error_message) {
    if (!engine || !script_code) {
        if (error_message) {
            *error_message = strdup("Invalid parameters for script validation");
        }
        return 0;
    }

    /* For now, perform basic validation - check if script is non-empty and well-formed */
    if (strlen(script_code) == 0) {
        if (error_message) {
            *error_message = strdup("Script code cannot be empty");
        }
        return 0;
    }

    /* Check for basic JavaScript structure - very basic syntax validation */
    /* This is a simplified validation - we accept the script if it contains valid characters */
    int brace_count = 0;
    int paren_count = 0;
    int bracket_count = 0;
    
    for (const char *p = script_code; *p; p++) {
        switch (*p) {
            case '{': brace_count++; break;
            case '}': brace_count--; break;
            case '(': paren_count++; break;
            case ')': paren_count--; break;
            case '[': bracket_count++; break;
            case ']': bracket_count--; break;
        }
        
        /* Check for negative counts (mismatched brackets) */
        if (brace_count < 0 || paren_count < 0 || bracket_count < 0) {
            if (error_message) {
                *error_message = strdup("Mismatched brackets or parentheses");
            }
            return 0;
        }
    }
    
    /* Check for unmatched brackets */
    if (brace_count != 0 || paren_count != 0 || bracket_count != 0) {
        if (error_message) {
            *error_message = strdup("Unmatched brackets or parentheses");
        }
        return 0;
    }
    
    /* Script passed basic validation */
    return 1;
}

/* List JavaScript scripts with optional filtering */
json_value_t* js_native_list_scripts(database_t *db, js_script_type_t type, 
                                     const char *collection_name, const char *user_id) {
    if (!db) {
        LOG_ERROR("Database pointer is NULL.");
        return NULL;
    }

    const char *collection = js_script_type_to_collection(type);
    if (!collection) {
        LOG_ERROR("Invalid script type: %d", type);
        return NULL;
    }

    /* Build query */
    json_value_t *query = json_create_object();
    json_object_set(query, "enabled", json_create_boolean(1));

    if (collection_name) {
        json_object_set(query, "collection_pattern", json_create_string(collection_name));
    }

    if (user_id) {
        json_object_set(query, "created_by", json_create_string(user_id));
    }

    /* Execute query */
    json_value_t *results = db_query_documents(db, collection, query);
    json_free(query);

    return results;
}

/* Execute script with comprehensive metrics tracking */
int js_native_execute_script(js_engine_t *engine, database_t *db, const char *script_id,
                            const char *user_id, json_value_t *input_data, 
                            json_value_t **output_data, js_execution_context_t *context) {
    if (!engine || !db || !script_id || !user_id || !context) {
        LOG_ERROR("Invalid parameters for script execution.");
        return 0;
    }

    /* Get script metadata */
    js_script_metadata_t *metadata = js_native_get_script(db, script_id);
    if (!metadata) {
        LOG_ERROR("Script not found: %s", script_id);
        return 0;
    }

    /* Initialize execution context */
    strncpy(context->script_id, script_id, sizeof(context->script_id) - 1);
    context->script_type = metadata->type;
    strncpy(context->collection_name, metadata->collection_pattern, sizeof(context->collection_name) - 1);
    context->input_data = input_data;
    clock_gettime(CLOCK_MONOTONIC, &context->start_time);

    /* Execute based on script type */
    int success = 0;
    
    switch (metadata->type) {
        case JS_SCRIPT_FUNCTION:
            success = js_call_user_function(engine, metadata->name, input_data, output_data);
            break;
            
        case JS_SCRIPT_VALIDATOR:
            if (input_data) {
                success = js_validate_document(engine, metadata->collection_pattern, input_data);
                strncpy(context->operation, "validate", sizeof(context->operation) - 1);
            }
            break;
            
        case JS_SCRIPT_TRANSFORMER:
            if (input_data) {
                *output_data = js_transform_document(engine, metadata->collection_pattern, 
                                                   input_data, "transform");
                success = (*output_data != NULL);
                strncpy(context->operation, "transform", sizeof(context->operation) - 1);
            }
            break;
            
        default:
            LOG_ERROR("Unknown script type: %d", metadata->type);
            break;
    }

    clock_gettime(CLOCK_MONOTONIC, &context->end_time);
    context->success = success;
    context->output_data = output_data ? *output_data : NULL;

    if (!success) {
        const char *error = js_get_last_error(engine);
        if (error) {
            strncpy(context->error_message, error, sizeof(context->error_message) - 1);
        }
    }

    /* Record execution metrics */
    js_native_record_execution_metrics(db, context);
    js_native_free_script_metadata(metadata);

    return success;
}

/* Get global JavaScript execution metrics */
json_value_t* js_native_get_global_metrics(database_t *db, time_t from_time, time_t to_time) {
    if (!db) {
        LOG_ERROR("Database pointer is NULL.");
        return NULL;
    }

    /* Build time range query */
    json_value_t *query = json_create_object();
    json_value_t *timestamp_filter = json_create_object();
    
    if (from_time > 0) {
        json_object_set(timestamp_filter, "$gte", json_create_integer(from_time));
    }
    if (to_time > 0) {
        json_object_set(timestamp_filter, "$lte", json_create_integer(to_time));
    }
    
    if (json_object_size(timestamp_filter) > 0) {
        json_object_set(query, "timestamp", timestamp_filter);
    } else {
        json_free(timestamp_filter);
    }

    /* Get execution metrics */
    json_value_t *metrics = db_query_documents(db, JS_EXECUTION_METRICS_COLLECTION, query);
    json_free(query);

    if (!metrics) {
        return json_create_object(); /* Return empty object if no metrics */
    }

    /* Aggregate metrics */
    json_value_t *aggregated = json_create_object();
    json_object_set(aggregated, "total_executions", json_create_integer(json_array_size(metrics)));
    
    int successful = 0, failed = 0;
    double total_time = 0.0;
    double min_time = DBL_MAX, max_time = 0.0;
    
    for (size_t i = 0; i < json_array_size(metrics); i++) {
        json_value_t *metric = json_array_get(metrics, i);
        if (!metric || metric->type != JSON_OBJECT) continue;
        
        json_value_t *success_val = json_object_get(metric, "success");
        if (success_val && success_val->type == JSON_BOOLEAN) {
            if (success_val->value.boolean) {
                successful++;
            } else {
                failed++;
            }
        }
        
        json_value_t *time_val = json_object_get(metric, "execution_time_ms");
        if (time_val && time_val->type == JSON_NUMBER) {
            double exec_time = time_val->value.number;
            total_time += exec_time;
            if (exec_time < min_time) min_time = exec_time;
            if (exec_time > max_time) max_time = exec_time;
        }
    }
    
    json_object_set(aggregated, "successful_executions", json_create_integer(successful));
    json_object_set(aggregated, "failed_executions", json_create_integer(failed));
    json_object_set(aggregated, "success_rate", 
                   json_create_number(successful + failed > 0 ? (double)successful / (successful + failed) : 0.0));
    
    if (json_array_size(metrics) > 0) {
        json_object_set(aggregated, "avg_execution_time_ms", 
                       json_create_number(total_time / json_array_size(metrics)));
        json_object_set(aggregated, "min_execution_time_ms", json_create_number(min_time));
        json_object_set(aggregated, "max_execution_time_ms", json_create_number(max_time));
    }
    
    json_object_set(aggregated, "raw_metrics", metrics);
    
    return aggregated;
}

/* Execute validators for document based on tags */
int js_native_execute_validators(js_engine_t *engine, database_t *db, const char *collection_name,
                               json_value_t *document, const char *user_id, 
                               json_value_t **validation_errors) {
    if (!engine || !db || !collection_name || !document || !user_id) {
        LOG_ERROR("Invalid parameters for executing validators.");
        return 0;
    }

    json_value_t *triggered_scripts = js_native_find_triggered_scripts(db, collection_name, "validate", document);
    if (!triggered_scripts) {
        return 1; /* No validators found, consider valid */
    }

    json_value_t *errors = json_create_array();
    int all_valid = 1;

    /* Execute each triggered validator script */
    for (size_t i = 0; i < json_array_size(triggered_scripts); i++) {
        json_value_t *script_json = json_array_get(triggered_scripts, i);
        if (!script_json) continue;

        js_script_metadata_t *script = js_native_script_metadata_from_json(script_json);
        if (!script || script->type != JS_SCRIPT_VALIDATOR) {
            if (script) js_native_free_script_metadata(script);
            continue;
        }

        /* Check execution permissions */
        if (!js_native_check_execution_permission(db, user_id, script->id, "validate")) {
            js_native_free_script_metadata(script);
            continue;
        }

        /* Execute validation script */
        js_execution_context_t context = {0};
        json_value_t *output_data = NULL;
        
        int validation_result = js_native_execute_script(engine, db, script->id, user_id, 
                                                       document, &output_data, &context);

        if (!validation_result) {
            all_valid = 0;
            /* Parse validation errors and add to errors array */
            json_value_t *validation_error = json_create_object();
            json_object_set(validation_error, "script_id", json_create_string(script->id));
            json_object_set(validation_error, "script_name", json_create_string(script->name));
            json_object_set(validation_error, "error", json_create_string(context.error_message));
            json_array_append(errors, validation_error);
        }

        if (output_data) {
            json_free(output_data);
        }
        js_native_free_script_metadata(script);
    }

    json_free(triggered_scripts);

    if (validation_errors) {
        *validation_errors = errors;
    } else {
        json_free(errors);
    }

    return all_valid;
}

/* Execute transformers for document based on tags */
json_value_t* js_native_execute_transformers(js_engine_t *engine, database_t *db, 
                                            const char *collection_name, json_value_t *document,
                                            const char *operation, const char *user_id) {
    if (!engine || !db || !collection_name || !document || !operation || !user_id) {
        LOG_ERROR("Invalid parameters for executing transformers.");
        return json_clone(document); /* Return original document on error */
    }

    json_value_t *triggered_scripts = js_native_find_triggered_scripts(db, collection_name, operation, document);
    if (!triggered_scripts) {
        return json_clone(document); /* No transformers found, return original */
    }

    json_value_t *transformed_doc = json_clone(document);

    /* Execute each triggered transformer script */
    for (size_t i = 0; i < json_array_size(triggered_scripts); i++) {
        json_value_t *script_json = json_array_get(triggered_scripts, i);
        if (!script_json) continue;

        js_script_metadata_t *script = js_native_script_metadata_from_json(script_json);
        if (!script || script->type != JS_SCRIPT_TRANSFORMER) {
            if (script) js_native_free_script_metadata(script);
            continue;
        }

        /* Check execution permissions */
        if (!js_native_check_execution_permission(db, user_id, script->id, operation)) {
            js_native_free_script_metadata(script);
            continue;
        }

        /* Execute transformer script */
        js_execution_context_t context = {0};
        json_value_t *output_data = NULL;
        
        int transform_result = js_native_execute_script(engine, db, script->id, user_id, 
                                                      transformed_doc, &output_data, &context);

        if (transform_result && output_data) {
            json_free(transformed_doc);
            transformed_doc = output_data;
        } else if (output_data) {
            json_free(output_data);
        }

        js_native_free_script_metadata(script);
    }

    json_free(triggered_scripts);
    return transformed_doc;
}

/* Update JavaScript script */
int js_native_update_script(database_t *db, const char *user_id, const char *script_id, 
                           js_script_metadata_t *metadata) {
    if (!db || !user_id || !script_id || !metadata) {
        LOG_ERROR("Invalid parameters for updating JavaScript script.");
        return 0;
    }

    /* Get existing script to verify ownership and type */
    js_script_metadata_t *existing = js_native_get_script(db, script_id);
    if (!existing) {
        LOG_ERROR("Script not found for update: %s", script_id);
        return 0;
    }

    /* Check if user owns the script or has admin permissions */
    if (strcmp(existing->created_by, user_id) != 0) {
        /* Check for admin permission */
        if (!rbac_db_check_permission(db, user_id, RBAC_COLLECTION, "_js_scripts", RBAC_ADMIN)) {
            LOG_WARNING("User %s attempted to update script %s without permission", user_id, script_id);
            js_native_free_script_metadata(existing);
            return 0;
        }
    }

    /* Update metadata */
    strncpy(metadata->id, script_id, sizeof(metadata->id) - 1);
    strncpy(metadata->created_by, existing->created_by, sizeof(metadata->created_by) - 1);
    metadata->created_at = existing->created_at;
    metadata->updated_at = time(NULL);
    metadata->version = existing->version + 1;

    /* Get target collection based on existing type (type cannot be changed) */
    const char *collection_name = js_script_type_to_collection(existing->type);
    if (!collection_name) {
        LOG_ERROR("Invalid existing script type: %d", existing->type);
        js_native_free_script_metadata(existing);
        return 0;
    }

    js_native_free_script_metadata(existing);

    /* Convert to JSON */
    json_value_t *script_doc = js_native_script_metadata_to_json(metadata);
    if (!script_doc) {
        LOG_ERROR("Cannot convert script metadata to JSON for update.");
        return 0;
    }

    /* Update in database */
    json_value_t *result = db_update_document(db, collection_name, script_id, script_doc);
    json_free(script_doc);

    if (!result) {
        LOG_ERROR("Cannot update JavaScript script in database.");
        return 0;
    }

    json_free(result);
    LOG_INFO("JavaScript script updated successfully: %s (version: %d)", script_id, metadata->version);
    return 1;
}

/* Delete JavaScript script */
int js_native_delete_script(database_t *db, const char *user_id, const char *script_id) {
    if (!db || !user_id || !script_id) {
        LOG_ERROR("Invalid parameters for deleting JavaScript script.");
        return 0;
    }

    /* Get existing script to verify ownership and determine collection */
    js_script_metadata_t *existing = js_native_get_script(db, script_id);
    if (!existing) {
        LOG_ERROR("Script not found for deletion: %s", script_id);
        return 0;
    }

    /* Check if user owns the script or has admin permissions */
    if (strcmp(existing->created_by, user_id) != 0) {
        /* Check for admin permission */
        if (!rbac_db_check_permission(db, user_id, RBAC_COLLECTION, "_js_scripts", RBAC_ADMIN)) {
            LOG_WARNING("User %s attempted to delete script %s without permission", user_id, script_id);
            js_native_free_script_metadata(existing);
            return 0;
        }
    }

    /* Get target collection */
    const char *collection_name = js_script_type_to_collection(existing->type);
    if (!collection_name) {
        LOG_ERROR("Invalid script type for deletion: %d", existing->type);
        js_native_free_script_metadata(existing);
        return 0;
    }

    js_native_free_script_metadata(existing);

    /* Delete from database */
    int result = db_delete_document(db, collection_name, script_id);
    if (!result) {
        LOG_ERROR("Cannot delete JavaScript script from database.");
        return 0;
    }

    LOG_INFO("JavaScript script deleted successfully: %s", script_id);
    return 1;
}

/* Get script execution statistics */
json_value_t* js_native_get_script_statistics(database_t *db, const char *script_id, 
                                              time_t from_time, time_t to_time) {
    if (!db || !script_id) {
        LOG_ERROR("Invalid parameters for script statistics.");
        return NULL;
    }

    /* Build query for script-specific metrics */
    json_value_t *query = json_create_object();
    json_object_set(query, "script_id", json_create_string(script_id));
    
    /* Add time range filter */
    if (from_time > 0 || to_time > 0) {
        json_value_t *timestamp_filter = json_create_object();
        if (from_time > 0) {
            json_object_set(timestamp_filter, "$gte", json_create_integer(from_time));
        }
        if (to_time > 0) {
            json_object_set(timestamp_filter, "$lte", json_create_integer(to_time));
        }
        json_object_set(query, "timestamp", timestamp_filter);
    }

    /* Get execution metrics for this script */
    json_value_t *metrics = db_query_documents(db, JS_EXECUTION_METRICS_COLLECTION, query);
    json_free(query);

    if (!metrics) {
        return json_create_object(); /* Return empty object if no metrics */
    }

    /* Calculate script-specific statistics */
    json_value_t *stats = json_create_object();
    json_object_set(stats, "script_id", json_create_string(script_id));
    json_object_set(stats, "total_executions", json_create_integer(json_array_size(metrics)));
    
    int successful = 0, failed = 0;
    double total_time = 0.0;
    double min_time = DBL_MAX, max_time = 0.0;
    size_t total_memory_delta = 0;
    
    for (size_t i = 0; i < json_array_size(metrics); i++) {
        json_value_t *metric = json_array_get(metrics, i);
        if (!metric || metric->type != JSON_OBJECT) continue;
        
        json_value_t *success_val = json_object_get(metric, "success");
        if (success_val && success_val->type == JSON_BOOLEAN) {
            if (success_val->value.boolean) {
                successful++;
            } else {
                failed++;
            }
        }
        
        json_value_t *time_val = json_object_get(metric, "execution_time_ms");
        if (time_val && time_val->type == JSON_NUMBER) {
            double exec_time = time_val->value.number;
            total_time += exec_time;
            if (exec_time < min_time) min_time = exec_time;
            if (exec_time > max_time) max_time = exec_time;
        }

        json_value_t *memory_val = json_object_get(metric, "memory_delta");
        if (memory_val && memory_val->type == JSON_INTEGER) {
            total_memory_delta += memory_val->value.integer;
        }
    }
    
    json_object_set(stats, "successful_executions", json_create_integer(successful));
    json_object_set(stats, "failed_executions", json_create_integer(failed));
    json_object_set(stats, "success_rate", 
                   json_create_number(successful + failed > 0 ? (double)successful / (successful + failed) : 0.0));
    
    if (json_array_size(metrics) > 0) {
        json_object_set(stats, "avg_execution_time_ms", 
                       json_create_number(total_time / json_array_size(metrics)));
        json_object_set(stats, "min_execution_time_ms", json_create_number(min_time));
        json_object_set(stats, "max_execution_time_ms", json_create_number(max_time));
        json_object_set(stats, "avg_memory_delta_bytes", 
                       json_create_integer(total_memory_delta / json_array_size(metrics)));
    }
    
    json_object_set(stats, "raw_metrics", metrics);
    
    return stats;
}

/* Execute custom functions with tag matching */
json_value_t* js_native_execute_tagged_functions(js_engine_t *engine, database_t *db,
                                                const char *collection_name, 
                                                json_value_t *input_data, 
                                                const char *tag, const char *user_id) {
    if (!engine || !db || !collection_name || !tag || !user_id) {
        LOG_ERROR("Invalid parameters for executing tagged functions.");
        return NULL;
    }

    /* Build query to find functions with matching tags */
    json_value_t *query = json_create_object();
    json_object_set(query, "enabled", json_create_boolean(1));
    json_object_set(query, "collection_pattern", json_create_string(collection_name));
    
    /* Create trigger tags filter */
    json_value_t *tag_filter = json_create_object();
    json_object_set(tag_filter, "$in", json_create_string(tag));
    json_object_set(query, "trigger_tags", tag_filter);

    json_value_t *functions = db_query_documents(db, JS_FUNCTIONS_COLLECTION, query);
    json_free(query);

    if (!functions || json_array_size(functions) == 0) {
        if (functions) json_free(functions);
        return json_create_object(); /* No functions found */
    }

    json_value_t *results = json_create_array();

    /* Execute each matching function */
    for (size_t i = 0; i < json_array_size(functions); i++) {
        json_value_t *function_json = json_array_get(functions, i);
        if (!function_json) continue;

        js_script_metadata_t *function = js_native_script_metadata_from_json(function_json);
        if (!function || function->type != JS_SCRIPT_FUNCTION) {
            if (function) js_native_free_script_metadata(function);
            continue;
        }

        /* Check execution permissions */
        if (!js_native_check_execution_permission(db, user_id, function->id, "execute")) {
            js_native_free_script_metadata(function);
            continue;
        }

        /* Execute function */
        js_execution_context_t context = {0};
        json_value_t *output_data = NULL;
        
        int execution_result = js_native_execute_script(engine, db, function->id, user_id, 
                                                      input_data, &output_data, &context);

        /* Store result */
        json_value_t *result = json_create_object();
        json_object_set(result, "function_id", json_create_string(function->id));
        json_object_set(result, "function_name", json_create_string(function->name));
        json_object_set(result, "success", json_create_boolean(execution_result));
        
        if (execution_result && output_data) {
            json_object_set(result, "result", output_data);
        } else if (!execution_result) {
            json_object_set(result, "error", json_create_string(context.error_message));
        }

        json_array_append(results, result);
        js_native_free_script_metadata(function);
    }

    json_free(functions);
    return results;
}