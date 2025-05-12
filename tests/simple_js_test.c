#include "../src/include/database/database.h"
#include "../src/include/js/js_engine.h"
#include "../src/include/utils/json.h"
#include "../src/include/utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Simple JavaScript integration test
 * This test verifies that JavaScript integration works with QuickJS
 */

int main(int argc, char** argv) {
    /* Initialize logger */
    logger_init("simple_js_test.log", LOG_LEVEL_DEBUG);
    LOG_INFO("Starting simple JavaScript integration test");
    
    /* Create in-memory database */
    database_t* db = database_create(":memory:");
    if (!db) {
        LOG_ERROR("Failed to create database");
        return 1;
    }
    LOG_INFO("Database created successfully");
    
    /* Initialize JavaScript engine */
    LOG_INFO("Initializing JavaScript engine");
    js_engine_t* engine = js_engine_init(db);
    if (!engine) {
        LOG_ERROR("Failed to initialize JavaScript engine");
        database_free(db);
        return 1;
    }
    LOG_INFO("JavaScript engine initialized successfully");
    
    /* Execute minimal JavaScript test */
    LOG_INFO("Executing minimal JavaScript test");
    const char* script = "42;";
    char* result = NULL;
    
    if (js_engine_eval(engine, script, &result)) {
        LOG_INFO("JavaScript execution succeeded");
        LOG_INFO("Result: %s", result ? result : "null");
        if (result) {
            free(result);
            result = NULL;
        }
    } else {
        LOG_ERROR("JavaScript execution failed: %s", js_get_last_error(engine));
        js_engine_free(engine);
        database_free(db);
        return 1;
    }
    
    /* Test simple database operations from JavaScript */
    LOG_INFO("Testing JavaScript database operations");
    const char* db_script = 
        "const testCollection = 'test_collection';"
        "const testDoc = { id: 'test1', name: 'Test Document', value: 42 };"
        ""
        "// Insert document"
        "const insertResult = db.insertDocument(testCollection, testDoc);"
        ""
        "// Query document"
        "const queryResult = db.getDocument(testCollection, 'test1');"
        ""
        "// Check if query succeeded"
        "if (queryResult && queryResult.name === 'Test Document' && queryResult.value === 42) {"
        "  return { success: true, message: 'Database operations succeed' };"
        "} else {"
        "  return { success: false, message: 'Database operations failed' };"
        "}";
    
    if (js_engine_eval(engine, db_script, &result)) {
        LOG_INFO("Database operations test succeeded");
        LOG_INFO("Result: %s", result ? result : "null");
        if (result) {
            free(result);
            result = NULL;
        }
    } else {
        LOG_ERROR("Database operations test failed: %s", js_get_last_error(engine));
        js_engine_free(engine);
        database_free(db);
        return 1;
    }
    
    /* Test file evaluation */
    LOG_INFO("Testing JavaScript file evaluation");
    if (js_engine_eval_file(engine, "tests/js/minimal_test.js", &result)) {
        LOG_INFO("JavaScript file evaluation succeeded");
        LOG_INFO("Result: %s", result ? result : "null");
        if (result) {
            free(result);
            result = NULL;
        }
    } else {
        LOG_ERROR("JavaScript file evaluation failed: %s", js_get_last_error(engine));
        js_engine_free(engine);
        database_free(db);
        return 1;
    }
    
    /* Test integrated file execution */
    LOG_INFO("Testing JavaScript file execution");
    if (js_execute_file(engine, "tests/js/integration_test.js")) {
        LOG_INFO("JavaScript file execution succeeded");
    } else {
        LOG_ERROR("JavaScript file execution failed: %s", js_get_last_error(engine));
        js_engine_free(engine);
        database_free(db);
        return 1;
    }
    
    /* Clean up */
    LOG_INFO("Cleaning up resources");
    js_engine_free(engine);
    database_free(db);
    
    LOG_INFO("Test completed successfully");
    return 0;
}