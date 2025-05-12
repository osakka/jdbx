#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "utils/json.h"
#include "database/database.h"
#include "js/js_engine.h"
#include "utils/logger.h"

// Test basic JavaScript evaluation
int test_basic_js_eval(js_engine_t* engine) {
    printf("Testing basic JavaScript evaluation...\n");
    
    const char* script = "({ message: 'Hello, World!', sum: 2 + 2 })";
    char* result = NULL;
    int success = js_engine_eval(engine, script, &result);

    if (!success) {
        printf("FAIL: Basic JavaScript evaluation failed: %s\n", js_get_last_error(engine));
        return 0;
    }
    
    printf("PASS: Basic JavaScript evaluation succeeded: %s\n", result);
    free(result);
    return 1;
}

// Test JavaScript database operations
int test_db_operations(js_engine_t* engine) {
    printf("Testing JavaScript database operations...\n");
    
    // Create a test collection and perform basic CRUD operations
    const char* script = 
        "// Create test collection\n"
        "const testCollection = 'js_test_collection';\n"
        "\n"
        "// Test document\n"
        "const testDoc = { id: 'test1', name: 'Test Document', value: 42 };\n"
        "\n"
        "// Insert document\n"
        "const insertResult = db.insertDocument(testCollection, testDoc);\n"
        "if (!insertResult) throw new Error('Insert operation failed');\n"
        "\n"
        "// Get document\n"
        "const getResult = db.getDocument(testCollection, 'test1');\n"
        "if (!getResult) throw new Error('Get operation failed');\n"
        "if (getResult.name !== 'Test Document') throw new Error('Document content mismatch');\n"
        "\n"
        "// Update document\n"
        "const updateDoc = { id: 'test1', name: 'Updated Document', value: 100 };\n"
        "const updateResult = db.updateDocument(testCollection, 'test1', updateDoc);\n"
        "if (!updateResult) throw new Error('Update operation failed');\n"
        "\n"
        "// Query documents\n"
        "const queryResult = db.queryDocuments(testCollection, { name: 'Updated Document' });\n"
        "if (!queryResult || !queryResult.length) throw new Error('Query operation failed');\n"
        "\n"
        "// Delete document\n"
        "const deleteResult = db.deleteDocument(testCollection, 'test1');\n"
        "if (!deleteResult) throw new Error('Delete operation failed');\n"
        "\n"
        "// All tests passed\n"
        "({ success: true, operations: ['insert', 'get', 'update', 'query', 'delete'] });\n";
    
    char* result = NULL;
    int success = js_engine_eval(engine, script, &result);
    
    if (!success) {
        printf("FAIL: Database operations test failed: %s\n", js_get_last_error(engine));
        return 0;
    }
    
    printf("PASS: Database operations test succeeded: %s\n", result);
    free(result);
    return 1;
}

// Test JavaScript file evaluation
int test_js_file_eval(js_engine_t* engine) {
    printf("Testing JavaScript file evaluation...\n");
    
    // Create a test JavaScript file in the functions directory
    char js_test_path[256];
    snprintf(js_test_path, sizeof(js_test_path), "functions/js_integration_test.js");
    
    // Create directory if it doesn't exist
    struct stat st = {0};
    if (stat("functions", &st) == -1) {
        mkdir("functions", 0755);
    }
    
    // Write test JavaScript file
    FILE* file = fopen(js_test_path, "w");
    if (!file) {
        printf("FAIL: Could not create test JavaScript file\n");
        return 0;
    }
    
    fprintf(file, 
        "// JavaScript integration test\n"
        "function testFunction(x, y) {\n"
        "    return x + y;\n"
        "}\n"
        "\n"
        "// Test math operations\n"
        "const mathResult = testFunction(10, 20);\n"
        "\n"
        "// Test database access\n"
        "const dbTest = typeof db !== 'undefined';\n"
        "\n"
        "// Return results\n"
        "({ mathResult, dbTest });\n"
    );
    fclose(file);
    
    // Test file evaluation
    char* result = NULL;
    int success = js_engine_eval_file(engine, js_test_path, &result);
    
    if (!success) {
        printf("FAIL: JavaScript file evaluation failed: %s\n", js_get_last_error(engine));
        return 0;
    }
    
    printf("PASS: JavaScript file evaluation succeeded: %s\n", result);
    free(result);
    return 1;
}

int main(int argc, char** argv) {
    printf("=== JavaScript Integration Test ===\n\n");
    
    // Initialize logger
    logger_init("var/log/jsondb/js_integration_test.log", LOG_LEVEL_DEBUG);
    LOG_INFO("Starting JavaScript integration test");
    
    // Create directories if needed
    struct stat st = {0};
    if (stat("var/data/jsondb", &st) == -1) {
        system("mkdir -p var/data/jsondb");
    }
    
    if (stat("var/log/jsondb", &st) == -1) {
        system("mkdir -p var/log/jsondb");
    }
    
    // Initialize test database
    char db_path[256];
    snprintf(db_path, sizeof(db_path), "var/data/jsondb/js_integration_test.json");
    LOG_INFO("Initializing test database at %s", db_path);
    
    database_t* db = db_init(db_path);
    if (!db) {
        printf("FAIL: Failed to initialize database\n");
        LOG_ERROR("Failed to initialize database");
        logger_close();
        return 1;
    }
    
    // Initialize JavaScript engine
    js_engine_t* engine = js_engine_init(db);
    if (!engine) {
        printf("FAIL: Failed to initialize JavaScript engine\n");
        LOG_ERROR("Failed to initialize JavaScript engine");
        db_close(db);
        logger_close();
        return 1;
    }
    
    printf("JavaScript engine initialized successfully\n\n");
    LOG_INFO("JavaScript engine initialized successfully");
    
    // Run tests
    int tests_passed = 0;
    int total_tests = 3;
    
    // Test 1: Basic JavaScript evaluation
    tests_passed += test_basic_js_eval(engine);
    
    // Test 2: Database operations
    tests_passed += test_db_operations(engine);
    
    // Test 3: JavaScript file evaluation
    tests_passed += test_js_file_eval(engine);
    
    // Print results
    printf("\n=== Test Results ===\n");
    printf("Total tests: %d\n", total_tests);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", total_tests - tests_passed);
    
    if (tests_passed == total_tests) {
        printf("\nALL TESTS PASSED\n");
        LOG_INFO("All JavaScript integration tests passed");
    } else {
        printf("\nSOME TESTS FAILED\n");
        LOG_ERROR("%d out of %d JavaScript integration tests failed", total_tests - tests_passed, total_tests);
    }
    
    // Clean up
    js_engine_free(engine);
    db_close(db);
    logger_close();
    
    return tests_passed == total_tests ? 0 : 1;
}
EOF < /dev/null
