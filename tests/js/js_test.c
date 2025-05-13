#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define USE_QUICKJS 1

#include "utils/json.h"
#include "database/database.h"
#include "js/js_engine.h"
#include "utils/logger.h"
#include "utils/js_file_utils.h"

// Helper function to read a file's contents
char* read_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Could not open file %s\n", filename);
        return NULL;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate buffer
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    // Read file into buffer
    fread(buffer, 1, size, file);
    buffer[size] = '\0';

    fclose(file);
    return buffer;
}

// Function to test database operations
int test_database_operations(js_engine_t* engine) {
    printf("\n=== Testing Database Operations ===\n");

    // Test script to create a test collection and perform CRUD operations
    const char* db_script =
        "// Create test collection and document\n"
        "const testCollection = 'js_test_collection';\n"
        "const testDoc = { id: 'test1', name: 'Test Document', value: 42 };\n"
        "\n"
        "// Insert document\n"
        "const insertResult = db.insertDocument(testCollection, testDoc);\n"
        "console.log('Insert result:', insertResult);\n"
        "\n"
        "// Get the document\n"
        "const getResult = db.getDocument(testCollection, 'test1');\n"
        "console.log('Get result:', getResult);\n"
        "\n"
        "// Update the document\n"
        "const updateDoc = { id: 'test1', name: 'Updated Document', value: 100 };\n"
        "const updateResult = db.updateDocument(testCollection, 'test1', updateDoc);\n"
        "console.log('Update result:', updateResult);\n"
        "\n"
        "// Query documents\n"
        "const queryResult = db.queryDocuments(testCollection, { name: 'Updated Document' });\n"
        "console.log('Query result:', queryResult);\n"
        "\n"
        "// Delete document\n"
        "const deleteResult = db.deleteDocument(testCollection, 'test1');\n"
        "console.log('Delete result:', deleteResult);\n"
        "\n"
        "// Return results\n"
        "({ success: true, operations: ['insert', 'get', 'update', 'query', 'delete'] });\n";

    char* result = NULL;
    int success = js_engine_eval(engine, db_script, &result);

    if (success) {
        printf("Database operations test succeeded!\n");
        printf("Result: %s\n", result ? result : "null");
        free(result);
        return 1;
    } else {
        printf("Database operations test failed: %s\n", js_get_last_error(engine));
        return 0;
    }
}

// Initialize the logger
void init_logger() {
    // Initialize the logger with a file path and log level
    logger_init("var/log/jsondb/js_test.log", LOG_LEVEL_DEBUG);
}

int main(int argc, char** argv) {
    // Initialize the logger
    init_logger();

    // Log the start of the test
    LOG_INFO("Starting JavaScript engine test");

    // Set up test database file
    char db_path[PATH_MAX];
    snprintf(db_path, sizeof(db_path), "var/data/jsondb/test_db.json");

    LOG_INFO("Initializing test database at %s", db_path);

    // Initialize a simple test database
    database_t* db = db_init(db_path);
    if (!db) {
        fprintf(stderr, "Failed to initialize database\n");
        return 1;
    }

    // Initialize the JavaScript engine
    js_engine_t* engine = js_engine_init(db);
    if (!engine) {
        fprintf(stderr, "Failed to initialize JS engine\n");
        db_close(db);
        return 1;
    }

    printf("JavaScript engine initialized successfully\n");

    // Test simple JavaScript evaluation
    printf("\n=== Testing Basic JavaScript Evaluation ===\n");
    const char* simple_script = "({ message: 'Hello, World!', sum: 2 + 2 })";
    char* result = NULL;
    int success = js_engine_eval(engine, simple_script, &result);

    if (success) {
        printf("Basic JavaScript evaluation succeeded!\n");
        printf("Result: %s\n", result ? result : "null");
        free(result);
    } else {
        printf("Basic JavaScript evaluation failed: %s\n", js_get_last_error(engine));
    }

    // Test database operations
    test_database_operations(engine);

    // Test with JavaScript file if provided
    if (argc > 1) {
        printf("\n=== Testing JavaScript File Evaluation ===\n");
        char* js_code = read_file(argv[1]);
        if (!js_code) {
            printf("Failed to read JS file %s\n", argv[1]);
        } else {
            char* file_result = NULL;
            int file_success = js_engine_eval(engine, js_code, &file_result);

            if (file_success) {
                printf("JavaScript file evaluation succeeded!\n");
                printf("Result: %s\n", file_result ? file_result : "null");
                free(file_result);
            } else {
                printf("JavaScript file evaluation failed: %s\n", js_get_last_error(engine));
            }

            free(js_code);
        }
    }

    // Test with the existing js_test.js file
    printf("\n=== Testing js_test.js File ===\n");
    char* file_result = NULL;
    int file_success = js_engine_eval_file(engine, "js_test.js", &file_result);

    if (file_success) {
        printf("js_test.js evaluation succeeded!\n");
        printf("Result: %s\n", file_result ? file_result : "null");
        free(file_result);
    } else {
        printf("js_test.js evaluation failed: %s\n", js_get_last_error(engine));
    }

    // Clean up
    LOG_INFO("JavaScript engine test completed");

    js_engine_free(engine);
    db_close(db);
    logger_close();

    return 0;
}