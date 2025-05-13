#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "jsondb.h"
#include "jsondb/database/database.h"
#include "jsondb/js/js_api.h"
#include "jsondb/js/js_engine.h"
#include "jsondb/utils/json.h"
#include "jsondb/utils/json_helpers.h"

#define TEST_LOG_FILE "../test_logs/integration_js.log"
#define TEST_DB_FILE "../data/test_js_integration.json"
#define TEST_SCRIPT_DIR "../data/scripts"

#ifndef DISABLE_JS
// Note: We only test JS integration if it's enabled in the build

FILE *log_file = NULL;

void log_test(const char *message) {
    if (log_file) {
        fprintf(log_file, "[%s] %s\n", __TIME__, message);
    }
    printf("%s\n", message);
}

void test_init() {
    log_file = fopen(TEST_LOG_FILE, "w");
    if (!log_file) {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
    }
    
    log_test("Starting JavaScript integration tests");
    
    // Create test scripts directory if it doesn't exist
    mkdir(TEST_SCRIPT_DIR, 0755);
}

void test_cleanup() {
    if (log_file) {
        fprintf(log_file, "Time: %ld ms\n", clock() / (CLOCKS_PER_SEC / 1000));
        fclose(log_file);
    }
}

/* Helper function to create a test JavaScript file */
void create_test_script(const char *filename, const char *script_content) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", TEST_SCRIPT_DIR, filename);
    
    FILE *script_file = fopen(filepath, "w");
    if (!script_file) {
        log_test("FAIL: Could not create test script file");
        return;
    }
    
    fputs(script_content, script_file);
    fclose(script_file);
}

/* Test basic JavaScript engine initialization */
int test_js_engine_init() {
    log_test("Testing JavaScript engine initialization");
    
    js_engine_t *engine = js_engine_init();
    if (!engine) {
        log_test("FAIL: Failed to initialize JavaScript engine");
        return 0;
    }
    
    log_test("PASS: JavaScript engine initialized successfully");
    js_engine_free(engine);
    return 1;
}

/* Test JavaScript script execution */
int test_js_script_execution() {
    log_test("Testing JavaScript script execution");
    
    js_engine_t *engine = js_engine_init();
    if (!engine) {
        log_test("FAIL: Failed to initialize JavaScript engine for script execution test");
        return 0;
    }
    
    // Create a simple test script
    const char *script_code = "function test() { return 42; }\ntest();";
    const char *script_filename = "test_exec.js";
    create_test_script(script_filename, script_code);
    
    char script_path[256];
    snprintf(script_path, sizeof(script_path), "%s/%s", TEST_SCRIPT_DIR, script_filename);
    
    // Execute the script
    json_value_t *result = js_engine_execute_file(engine, script_path);
    if (!result) {
        log_test("FAIL: Failed to execute JavaScript script");
        js_engine_free(engine);
        return 0;
    }
    
    // Check the result
    if (json_get_type(result) != JSON_NUMBER || json_get_number(result) != 42) {
        log_test("FAIL: JavaScript script execution returned unexpected result");
        js_engine_free(engine);
        return 0;
    }
    
    log_test("PASS: JavaScript script execution succeeded");
    js_engine_free(engine);
    return 1;
}

/* Test JavaScript - Database integration */
int test_js_database_integration() {
    log_test("Testing JavaScript - Database integration");
    
    // Initialize database
    database_t *db = database_init(TEST_DB_FILE);
    if (!db) {
        log_test("FAIL: Failed to initialize database for JS integration test");
        return 0;
    }
    
    // Initialize JS engine
    js_engine_t *engine = js_engine_init();
    if (!engine) {
        log_test("FAIL: Failed to initialize JavaScript engine for DB integration test");
        database_free(db);
        return 0;
    }
    
    // Register database with JS engine
    if (js_api_register_database(engine, db) != 0) {
        log_test("FAIL: Failed to register database with JavaScript engine");
        js_engine_free(engine);
        database_free(db);
        return 0;
    }
    
    // Create a test script that uses database functions
    const char *script_code = 
        "// Test database integration\n"
        "function testDatabaseIntegration() {\n"
        "    // Insert a document\n"
        "    var doc = {\n"
        "        name: 'JS Integration Test',\n"
        "        value: 42,\n"
        "        timestamp: Date.now()\n"
        "    };\n"
        "    \n"
        "    var result = db.insertDocument('test_collection', 'js_doc_1', doc);\n"
        "    if (!result) {\n"
        "        return { success: false, stage: 'insert' };\n"
        "    }\n"
        "    \n"
        "    // Read the document back\n"
        "    var retrieved = db.getDocument('test_collection', 'js_doc_1');\n"
        "    if (!retrieved || retrieved.name !== 'JS Integration Test') {\n"
        "        return { success: false, stage: 'retrieve', retrieved: retrieved };\n"
        "    }\n"
        "    \n"
        "    // Update the document\n"
        "    retrieved.value = 99;\n"
        "    retrieved.updated = true;\n"
        "    \n"
        "    var updateResult = db.updateDocument('test_collection', 'js_doc_1', retrieved);\n"
        "    if (!updateResult) {\n"
        "        return { success: false, stage: 'update' };\n"
        "    }\n"
        "    \n"
        "    // Read again to verify update\n"
        "    var updated = db.getDocument('test_collection', 'js_doc_1');\n"
        "    if (!updated || updated.value !== 99 || !updated.updated) {\n"
        "        return { success: false, stage: 'verify_update', updated: updated };\n"
        "    }\n"
        "    \n"
        "    // Delete the document\n"
        "    var deleteResult = db.deleteDocument('test_collection', 'js_doc_1');\n"
        "    if (!deleteResult) {\n"
        "        return { success: false, stage: 'delete' };\n"
        "    }\n"
        "    \n"
        "    // Verify deletion\n"
        "    var afterDelete = db.getDocument('test_collection', 'js_doc_1');\n"
        "    if (afterDelete) {\n"
        "        return { success: false, stage: 'verify_delete', document: afterDelete };\n"
        "    }\n"
        "    \n"
        "    return { success: true };\n"
        "}\n"
        "\n"
        "testDatabaseIntegration();\n";
    
    const char *script_filename = "test_db_integration.js";
    create_test_script(script_filename, script_code);
    
    char script_path[256];
    snprintf(script_path, sizeof(script_path), "%s/%s", TEST_SCRIPT_DIR, script_filename);
    
    // Execute the script
    json_value_t *result = js_engine_execute_file(engine, script_path);
    if (!result) {
        log_test("FAIL: Failed to execute JavaScript database integration script");
        js_engine_free(engine);
        database_free(db);
        return 0;
    }
    
    // Check the result
    json_value_t *success_val = json_object_get(result, "success");
    if (!success_val || json_get_boolean(success_val) != 1) {
        json_value_t *stage_val = json_object_get(result, "stage");
        const char *stage = stage_val ? json_get_string(stage_val) : "unknown";
        
        char failure_msg[256];
        snprintf(failure_msg, sizeof(failure_msg), 
                 "FAIL: JavaScript database integration failed at stage: %s", stage);
        log_test(failure_msg);
        
        js_engine_free(engine);
        database_free(db);
        return 0;
    }
    
    log_test("PASS: JavaScript database integration succeeded");
    js_engine_free(engine);
    database_free(db);
    return 1;
}

/* Test JavaScript transformations */
int test_js_transformations() {
    log_test("Testing JavaScript transformations");
    
    // Initialize database
    database_t *db = database_init(TEST_DB_FILE);
    if (!db) {
        log_test("FAIL: Failed to initialize database for JS transformations test");
        return 0;
    }
    
    // Initialize JS engine
    js_engine_t *engine = js_engine_init();
    if (!engine) {
        log_test("FAIL: Failed to initialize JavaScript engine for transformations test");
        database_free(db);
        return 0;
    }
    
    // Register database with JS engine
    if (js_api_register_database(engine, db) != 0) {
        log_test("FAIL: Failed to register database with JavaScript engine");
        js_engine_free(engine);
        database_free(db);
        return 0;
    }
    
    // Create a transformation script
    const char *transform_code = 
        "// Document transformation function\n"
        "function transformDocument(doc) {\n"
        "    // Add transformed flag\n"
        "    doc.transformed = true;\n"
        "    \n"
        "    // Add a timestamp\n"
        "    doc.transform_time = Date.now();\n"
        "    \n"
        "    // Calculate a new field\n"
        "    if (typeof doc.value === 'number') {\n"
        "        doc.calculated = doc.value * 2;\n"
        "    }\n"
        "    \n"
        "    return doc;\n"
        "}\n";
    
    const char *transform_filename = "transformer.js";
    create_test_script(transform_filename, transform_code);
    
    // Create a test script that uses the transformation
    const char *test_code = 
        "// Test transformations\n"
        "function testTransformations() {\n"
        "    // Create sample document\n"
        "    var doc = {\n"
        "        name: 'Transform Test',\n"
        "        value: 42\n"
        "    };\n"
        "    \n"
        "    // Register transformation\n"
        "    var transformPath = '" TEST_SCRIPT_DIR "/transformer.js';\n"
        "    if (!db.registerTransformation('test_transform', transformPath, 'transformDocument')) {\n"
        "        return { success: false, stage: 'register_transform' };\n"
        "    }\n"
        "    \n"
        "    // Insert document with transformation\n"
        "    var result = db.insertDocumentWithTransform('test_collection', 'transform_doc_1', doc, 'test_transform');\n"
        "    if (!result) {\n"
        "        return { success: false, stage: 'insert_with_transform' };\n"
        "    }\n"
        "    \n"
        "    // Get the document and check transformation was applied\n"
        "    var retrieved = db.getDocument('test_collection', 'transform_doc_1');\n"
        "    if (!retrieved) {\n"
        "        return { success: false, stage: 'retrieve_transformed' };\n"
        "    }\n"
        "    \n"
        "    if (!retrieved.transformed || !retrieved.transform_time || retrieved.calculated !== 84) {\n"
        "        return { \n"
        "            success: false, \n"
        "            stage: 'verify_transformation',\n"
        "            transformed: retrieved.transformed,\n"
        "            calculated: retrieved.calculated\n"
        "        };\n"
        "    }\n"
        "    \n"
        "    return { success: true, document: retrieved };\n"
        "}\n"
        "\n"
        "testTransformations();\n";
    
    const char *test_filename = "test_transformations.js";
    create_test_script(test_filename, test_code);
    
    char test_path[256];
    snprintf(test_path, sizeof(test_path), "%s/%s", TEST_SCRIPT_DIR, test_filename);
    
    // Execute the test script
    json_value_t *result = js_engine_execute_file(engine, test_path);
    if (!result) {
        log_test("FAIL: Failed to execute JavaScript transformations test script");
        js_engine_free(engine);
        database_free(db);
        return 0;
    }
    
    // Check the result
    json_value_t *success_val = json_object_get(result, "success");
    if (!success_val || json_get_boolean(success_val) != 1) {
        json_value_t *stage_val = json_object_get(result, "stage");
        const char *stage = stage_val ? json_get_string(stage_val) : "unknown";
        
        char failure_msg[256];
        snprintf(failure_msg, sizeof(failure_msg), 
                 "FAIL: JavaScript transformations test failed at stage: %s", stage);
        log_test(failure_msg);
        
        js_engine_free(engine);
        database_free(db);
        return 0;
    }
    
    log_test("PASS: JavaScript transformations test succeeded");
    js_engine_free(engine);
    database_free(db);
    return 1;
}

int main() {
    int success_count = 0;
    int total_tests = 0;
    
    test_init();
    
    // Run the tests
    total_tests++;
    success_count += test_js_engine_init();
    
    total_tests++;
    success_count += test_js_script_execution();
    
    total_tests++;
    success_count += test_js_database_integration();
    
    total_tests++;
    success_count += test_js_transformations();
    
    // Print summary
    printf("\nTest Summary: %d/%d tests passed\n", success_count, total_tests);
    log_test("\nDetails: JavaScript integration test suite");
    
    if (success_count == total_tests) {
        log_test("PASS: All JavaScript integration tests passed");
    } else {
        log_test("FAIL: Some JavaScript integration tests failed");
    }
    
    test_cleanup();
    return (success_count == total_tests) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#else // DISABLE_JS is defined

int main() {
    printf("JavaScript support is disabled in this build. Skipping JS integration tests.\n");
    return EXIT_SUCCESS;
}

#endif // DISABLE_JS