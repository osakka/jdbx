#ifndef JS_ENGINE_H
#define JS_ENGINE_H

#include "database/database.h"
#include "utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* QuickJS headers - only included when QuickJS support is enabled */
#ifdef USE_QUICKJS
#include "quickjs/quickjs.h"
#endif

/* JS context for database operations */
typedef struct {
#ifdef USE_QUICKJS
    JSRuntime *rt;        /* JavaScript runtime */
    JSContext *ctx;       /* JavaScript context */
#endif
    database_t *db;       /* Reference to the database */
    char *last_error;     /* Last error message */
} js_engine_t;

/* JavaScript engine initialization and cleanup */
js_engine_t* js_engine_init(database_t *db);
void js_engine_free(js_engine_t *engine);

/* JavaScript evaluation */
int js_engine_eval(js_engine_t *engine, const char *script, char **result);
int js_engine_eval_file(js_engine_t *engine, const char *file_path, char **result);

/* Execute JavaScript file */
int js_execute_file(js_engine_t *engine, const char *file_path);

/* Register database functions */
void js_register_db_functions(js_engine_t *engine);

/* JavaScript query execution */
json_value_t* js_execute_query(js_engine_t *engine, const char *collection_name, const char *query_script);

/* JavaScript document validation */
int js_validate_document(js_engine_t *engine, const char *collection_name, json_value_t *document);

/* JavaScript document transformation */
json_value_t* js_transform_document(js_engine_t *engine, const char *collection_name, json_value_t *document, const char *operation);

/* User-defined functions */
int js_register_user_function(js_engine_t *engine, const char *name, const char *script);
int js_call_user_function(js_engine_t *engine, const char *name, json_value_t *args, json_value_t **result);

/* Error handling */
const char* js_get_last_error(js_engine_t *engine);
void js_set_error(js_engine_t *engine, const char *error);

#endif /* JS_ENGINE_H */