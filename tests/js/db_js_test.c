#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef USE_QUICKJS
#include "quickjs/quickjs.h"
#endif

// Simple mock database operations
typedef struct {
    char name[32];
    void* collections; // Simulated collections
} mock_database_t;

// Mock database operations
mock_database_t* db_init(const char* path) {
    mock_database_t* db = (mock_database_t*)malloc(sizeof(mock_database_t));
    if (db) {
        strncpy(db->name, path, sizeof(db->name) - 1);
        db->name[sizeof(db->name) - 1] = '\0';
        db->collections = NULL;
    }
    return db;
}

void db_close(mock_database_t* db) {
    if (db) {
        free(db);
    }
}

// Mock JSON value for testing
typedef struct {
    int type;
    union {
        int boolean;
        int integer;
        double number;
        char* string;
        void* array;
        void* object;
    } value;
} mock_json_value_t;

// JavaScript database context
typedef struct {
#ifdef USE_QUICKJS
    JSRuntime *rt;
    JSContext *ctx;
#endif
    mock_database_t *db;
    char *last_error;
} js_engine_t;

// JavaScript initialization
js_engine_t* js_engine_init(mock_database_t *db) {
#ifdef USE_QUICKJS
    js_engine_t *engine = (js_engine_t*)malloc(sizeof(js_engine_t));
    if (!engine) {
        return NULL;
    }
    
    engine->db = db;
    engine->last_error = NULL;
    
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
    
    // Register database object and functions
    JSValue global_obj = JS_GetGlobalObject(engine->ctx);
    JSValue db_obj = JS_NewObject(engine->ctx);
    
    // Store database pointer for callbacks
    JS_SetOpaque(db_obj, engine);
    
    // Register a simple test function
    JSValue test_func = JS_NewCFunction(engine->ctx, NULL, "testFunc", 0);
    JS_SetPropertyStr(engine->ctx, db_obj, "testFunc", test_func);
    
    // Add database object to global scope
    JS_SetPropertyStr(engine->ctx, global_obj, "db", db_obj);
    
    JS_FreeValue(engine->ctx, global_obj);
    
    return engine;
#else
    return NULL;
#endif
}

// JavaScript cleanup
void js_engine_free(js_engine_t *engine) {
#ifdef USE_QUICKJS
    if (engine) {
        if (engine->last_error) {
            free(engine->last_error);
        }
        
        if (engine->ctx) {
            JS_FreeContext(engine->ctx);
        }
        
        if (engine->rt) {
            JS_FreeRuntime(engine->rt);
        }
        
        free(engine);
    }
#endif
}

// JavaScript evaluation
int js_engine_eval(js_engine_t *engine, const char *script, char **result) {
#ifdef USE_QUICKJS
    if (!engine || !script) {
        return 0;
    }
    
    JSValue val = JS_Eval(engine->ctx, script, strlen(script), "<input>", 0);
    
    if (JS_IsException(val)) {
        JSValue exception = JS_GetException(engine->ctx);
        const char *str = JS_ToCString(engine->ctx, exception);
        
        if (engine->last_error) {
            free(engine->last_error);
        }
        engine->last_error = strdup(str);
        
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
#else
    if (result) {
        *result = NULL;
    }
    return 0;
#endif
}

// Get last JavaScript error
const char* js_get_last_error(js_engine_t *engine) {
    if (!engine || !engine->last_error) {
        return "Unknown error";
    }
    return engine->last_error;
}

int main(int argc, char** argv) {
    printf("Testing QuickJS Database Integration\n");
    
    // Initialize mock database
    mock_database_t* db = db_init("test_db.json");
    if (!db) {
        fprintf(stderr, "Failed to initialize mock database\n");
        return 1;
    }
    
    // Initialize JavaScript engine
    js_engine_t* engine = js_engine_init(db);
    if (!engine) {
        fprintf(stderr, "Failed to initialize JavaScript engine\n");
        db_close(db);
        return 1;
    }
    
    printf("JavaScript engine initialized successfully\n");
    
    // Test basic JavaScript evaluation
    printf("\n=== Testing Basic JavaScript Evaluation ===\n");
    const char* basic_script = "({message: 'Hello from JavaScript!', sum: 2 + 2})";
    char* result = NULL;
    
    int success = js_engine_eval(engine, basic_script, &result);
    if (success) {
        printf("Basic script evaluation succeeded!\n");
        printf("Result: %s\n", result ? result : "null");
        free(result);
    } else {
        printf("Basic script evaluation failed: %s\n", js_get_last_error(engine));
    }
    
    // Test database object access
    printf("\n=== Testing Database Object Access ===\n");
    const char* db_script = "({dbExists: typeof db !== 'undefined', hasTestFunc: typeof db.testFunc === 'function'})";
    result = NULL;
    
    success = js_engine_eval(engine, db_script, &result);
    if (success) {
        printf("Database object access test succeeded!\n");
        printf("Result: %s\n", result ? result : "null");
        free(result);
    } else {
        printf("Database object access test failed: %s\n", js_get_last_error(engine));
    }
    
    // Clean up
    js_engine_free(engine);
    db_close(db);
    
    printf("\nTest completed successfully\n");
    return 0;
}