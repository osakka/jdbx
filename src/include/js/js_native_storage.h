#ifndef JS_NATIVE_STORAGE_H
#define JS_NATIVE_STORAGE_H

#include "database/database.h"
#include "utils/json.h"
#include "rbac/rbac.h"
#include "js/js_engine.h"
#include <time.h>

/* JavaScript script types */
typedef enum {
    JS_SCRIPT_VALIDATOR = 0,
    JS_SCRIPT_TRANSFORMER = 1,
    JS_SCRIPT_FUNCTION = 2
} js_script_type_t;

/* JavaScript script execution trigger types */
typedef enum {
    JS_TRIGGER_NONE = 0,
    JS_TRIGGER_DOCUMENT_TAG = 1,      /* Execute when document has specific tag */
    JS_TRIGGER_COLLECTION_TAG = 2,    /* Execute for all operations on tagged collection */
    JS_TRIGGER_OPERATION = 3,         /* Execute on specific operations (insert/update/delete) */
    JS_TRIGGER_MANUAL = 4             /* Manual execution only */
} js_trigger_type_t;

/* JavaScript execution context for metrics */
typedef struct {
    char script_id[64];
    js_script_type_t script_type;
    char collection_name[128];
    char operation[16];
    struct timespec start_time;
    struct timespec end_time;
    size_t memory_before;
    size_t memory_after;
    int success;
    char error_message[512];
    json_value_t *input_data;
    json_value_t *output_data;
} js_execution_context_t;

/* JavaScript script metadata */
typedef struct {
    char id[64];
    char name[128];
    char description[512];
    js_script_type_t type;
    char collection_pattern[128];     /* Collection name or pattern */
    js_trigger_type_t trigger_type;
    json_value_t *trigger_tags;       /* Array of tags that trigger execution */
    char *script_code;
    char created_by[64];              /* User ID who created the script */
    time_t created_at;
    time_t updated_at;
    int version;
    int enabled;
    json_value_t *rbac_permissions;   /* Required permissions to execute */
    json_value_t *execution_stats;    /* Performance and usage statistics */
} js_script_metadata_t;

/* Native JavaScript storage functions */

/**
 * Initialize native JavaScript storage system
 * Creates system collections for storing scripts
 */
int js_native_storage_init(database_t *db);

/**
 * Store JavaScript script in native database storage
 */
int js_native_store_script(database_t *db, const char *user_id, js_script_metadata_t *metadata);

/**
 * Retrieve JavaScript script by ID
 */
js_script_metadata_t* js_native_get_script(database_t *db, const char *script_id);

/**
 * List JavaScript scripts with optional filtering
 */
json_value_t* js_native_list_scripts(database_t *db, js_script_type_t type, 
                                     const char *collection_name, const char *user_id);

/**
 * Update JavaScript script
 */
int js_native_update_script(database_t *db, const char *user_id, const char *script_id, 
                           js_script_metadata_t *metadata);

/**
 * Delete JavaScript script
 */
int js_native_delete_script(database_t *db, const char *user_id, const char *script_id);

/**
 * Check if user has permission to execute script
 */
int js_native_check_execution_permission(database_t *db, const char *user_id, 
                                        const char *script_id, const char *operation);

/**
 * Find scripts triggered by document operation
 */
json_value_t* js_native_find_triggered_scripts(database_t *db, const char *collection_name,
                                               const char *operation, json_value_t *document);

/**
 * Execute script with comprehensive metrics tracking
 */
int js_native_execute_script(js_engine_t *engine, database_t *db, const char *script_id,
                            const char *user_id, json_value_t *input_data, 
                            json_value_t **output_data, js_execution_context_t *context);

/**
 * Record script execution metrics
 */
int js_native_record_execution_metrics(database_t *db, js_execution_context_t *context);

/**
 * Get script execution statistics
 */
json_value_t* js_native_get_script_statistics(database_t *db, const char *script_id, 
                                              time_t from_time, time_t to_time);

/**
 * Get global JavaScript execution metrics
 */
json_value_t* js_native_get_global_metrics(database_t *db, time_t from_time, time_t to_time);

/* Tag-based execution functions */

/**
 * Execute validators for document based on tags
 */
int js_native_execute_validators(js_engine_t *engine, database_t *db, const char *collection_name,
                               json_value_t *document, const char *user_id, 
                               json_value_t **validation_errors);

/**
 * Execute transformers for document based on tags
 */
json_value_t* js_native_execute_transformers(js_engine_t *engine, database_t *db, 
                                            const char *collection_name, json_value_t *document,
                                            const char *operation, const char *user_id);

/**
 * Execute custom functions with tag matching
 */
json_value_t* js_native_execute_tagged_functions(js_engine_t *engine, database_t *db,
                                                const char *collection_name, 
                                                json_value_t *input_data, 
                                                const char *tag, const char *user_id);

/* Utility functions */

/**
 * Create script metadata structure
 */
js_script_metadata_t* js_native_create_script_metadata(void);

/**
 * Free script metadata structure
 */
void js_native_free_script_metadata(js_script_metadata_t *metadata);

/**
 * Convert script metadata to JSON
 */
json_value_t* js_native_script_metadata_to_json(js_script_metadata_t *metadata);

/**
 * Create script metadata from JSON
 */
js_script_metadata_t* js_native_script_metadata_from_json(json_value_t *json);

/**
 * Generate unique script ID
 */
void js_native_generate_script_id(char *buffer, size_t buffer_size);

/**
 * Validate script syntax
 */
int js_native_validate_script_syntax(js_engine_t *engine, const char *script_code, 
                                    char **error_message);

/**
 * Convert script type enum to string
 */
const char* js_script_type_to_string(js_script_type_t type);

#endif /* JS_NATIVE_STORAGE_H */