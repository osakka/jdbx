#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define USE_QUICKJS 1

#include "utils/json.h"
#include "utils/logger.h"
#include "js/js_engine.h"

// Simple in-memory database stub for testing
typedef struct {
    char name[32];
} stub_database_t;

// Replace js_engine's database_t with our stub for testing
#define database_t stub_database_t

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

// Initialize the logger
void init_logger() {
    // Initialize the logger with a file path and log level
    logger_init("var/log/jsondb/js_test.log", LOG_LEVEL_DEBUG);
}

int main(int argc, char** argv) {
    // Initialize the logger
    init_logger();
    
    // Log the start of the test
    LOG_INFO("Starting simple JavaScript engine test");
    
    // Set up a minimal stub database
    stub_database_t db;
    strcpy(db.name, "test_db");
    
    // Initialize the JavaScript engine
    js_engine_t* engine = js_engine_init(&db);
    if (!engine) {
        fprintf(stderr, "Failed to initialize JS engine\n");
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
    
    // Test with the js_test.js file if it exists
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
    logger_close();
    
    return 0;
}