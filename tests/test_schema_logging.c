#include "database/database.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    // Initialize logger
    printf("Initializing logger...\n");
    const char* log_path = "../var/log/jsondb/schema_test.log";
    printf("Log path: %s\n", log_path);
    if (!logger_init(log_path, LOG_LEVEL_TRACE)) {
        fprintf(stderr, "Failed to initialize logger at path: %s\n", log_path);
        perror("Error");
        return 1;
    }
    printf("Logger initialized successfully\n");
    
    // Test schema creation with logging
    printf("Creating schema...\n");
    schema_t* schema = db_create_schema("test_schema", "A test schema for logging");
    
    // Add some rules
    printf("Adding schema rules...\n");
    schema_rule_t* rule1 = db_create_schema_rule(SCHEMA_TYPE_CHECK, "name");
    rule1->params.type_value = JSON_STRING;
    db_add_schema_rule(schema, rule1);
    
    schema_rule_t* rule2 = db_create_schema_rule(SCHEMA_REQUIRED, "name");
    rule2->params.required = 1;
    db_add_schema_rule(schema, rule2);
    
    schema_rule_t* rule3 = db_create_schema_rule(SCHEMA_MIN_LENGTH, "name");
    rule3->params.min_length = 3;
    db_add_schema_rule(schema, rule3);
    
    schema_rule_t* rule4 = db_create_schema_rule(SCHEMA_PATTERN, "email");
    rule4->params.pattern = strdup("[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}");
    db_add_schema_rule(schema, rule4);
    
    // Convert to JSON and back
    printf("Converting schema to JSON...\n");
    json_value_t* json_schema = db_schema_to_json(schema);
    
    printf("Converting JSON back to schema...\n");
    schema_t* schema2 = db_schema_from_json(json_schema);
    
    // Test document validation
    printf("Testing document validation...\n");
    
    // Valid document
    json_value_t* valid_doc = json_create_object();
    json_object_set(valid_doc, "name", json_create_string("John Doe"));
    json_object_set(valid_doc, "email", json_create_string("john.doe@example.com"));
    
    schema_validation_result_t result = db_validate_document(schema, valid_doc);
    printf("Valid document validation result: %s\n", result.is_valid ? "valid" : "invalid");
    
    // Invalid document (missing required field)
    json_value_t* invalid_doc = json_create_object();
    json_object_set(invalid_doc, "email", json_create_string("invalid@example"));
    
    result = db_validate_document(schema, invalid_doc);
    printf("Invalid document validation result: %s\n", result.is_valid ? "valid" : "invalid");
    if (!result.is_valid) {
        printf("  Error field: %s\n", result.error_field ? result.error_field : "NULL");
        printf("  Error message: %s\n", result.error_message ? result.error_message : "NULL");
        free(result.error_field);
        free(result.error_message);
    }
    
    // Cleanup
    printf("Cleaning up...\n");
    db_free_schema(schema);
    db_free_schema(schema2);
    json_free(json_schema);
    json_free(valid_doc);
    json_free(invalid_doc);
    
    logger_close();
    printf("Test completed. Check var/log/jsondb/schema_test.log for log output.\n");
    
    return 0;
}