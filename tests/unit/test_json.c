#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "src/include/jsondb.h"
#include "jsondb/utils/json.h"
#include "jsondb/utils/json_helpers.h"

#define TEST_LOG_FILE "../test_logs/unit_json.log"
#define MAX_JSON_SAMPLE 2048

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
    
    log_test("Starting JSON utilities unit tests");
}

void test_cleanup() {
    if (log_file) {
        fprintf(log_file, "Time: %ld ms\n", clock() / (CLOCKS_PER_SEC / 1000));
        fclose(log_file);
    }
}

/* Test JSON object creation and basic operations */
int test_json_object_ops() {
    log_test("Testing JSON object operations");
    
    // Create a JSON object
    json_value_t *obj = json_create_object();
    if (!obj) {
        log_test("FAIL: Failed to create JSON object");
        return 0;
    }
    
    // Add string property
    json_value_t *str_val = json_create_string("test_string");
    if (!str_val) {
        log_test("FAIL: Failed to create JSON string");
        return 0;
    }
    json_object_set(obj, "string_key", str_val);
    
    // Add number property
    json_value_t *num_val = json_create_number(42.5);
    if (!num_val) {
        log_test("FAIL: Failed to create JSON number");
        return 0;
    }
    json_object_set(obj, "number_key", num_val);
    
    // Add boolean property
    json_value_t *bool_val = json_create_boolean(1);
    if (!bool_val) {
        log_test("FAIL: Failed to create JSON boolean");
        return 0;
    }
    json_object_set(obj, "bool_key", bool_val);
    
    // Test property retrieval
    json_value_t *retrieved_str = json_object_get(obj, "string_key");
    if (!retrieved_str) {
        log_test("FAIL: Failed to retrieve string property");
        return 0;
    }
    
    const char *str_content = json_get_string(retrieved_str);
    if (!str_content || strcmp(str_content, "test_string") != 0) {
        log_test("FAIL: Retrieved string property has incorrect value");
        return 0;
    }
    
    json_value_t *retrieved_num = json_object_get(obj, "number_key");
    if (!retrieved_num) {
        log_test("FAIL: Failed to retrieve number property");
        return 0;
    }
    
    double num_content = json_get_number(retrieved_num);
    if (num_content != 42.5) {
        log_test("FAIL: Retrieved number property has incorrect value");
        return 0;
    }
    
    json_value_t *retrieved_bool = json_object_get(obj, "bool_key");
    if (!retrieved_bool) {
        log_test("FAIL: Failed to retrieve boolean property");
        return 0;
    }
    
    int bool_content = json_get_boolean(retrieved_bool);
    if (bool_content != 1) {
        log_test("FAIL: Retrieved boolean property has incorrect value");
        return 0;
    }
    
    log_test("PASS: JSON object operations work correctly");
    return 1;
}

/* Test JSON array operations */
int test_json_array_ops() {
    log_test("Testing JSON array operations");
    
    // Create a JSON array
    json_value_t *arr = json_create_array();
    if (!arr) {
        log_test("FAIL: Failed to create JSON array");
        return 0;
    }
    
    // Add string element
    json_value_t *str_val = json_create_string("array_string");
    if (!str_val) {
        log_test("FAIL: Failed to create JSON string for array");
        return 0;
    }
    json_array_append(arr, str_val);
    
    // Add number element
    json_value_t *num_val = json_create_number(99.9);
    if (!num_val) {
        log_test("FAIL: Failed to create JSON number for array");
        return 0;
    }
    json_array_append(arr, num_val);
    
    // Add object element
    json_value_t *obj_val = json_create_object();
    if (!obj_val) {
        log_test("FAIL: Failed to create JSON object for array");
        return 0;
    }
    json_object_set(obj_val, "nested_key", json_create_string("nested_value"));
    json_array_append(arr, obj_val);
    
    // Test array size
    int array_size = json_array_size(arr);
    if (array_size != 3) {
        log_test("FAIL: Array size is incorrect");
        return 0;
    }
    
    // Test element retrieval
    json_value_t *retrieved_str = json_array_get(arr, 0);
    if (!retrieved_str) {
        log_test("FAIL: Failed to retrieve string element from array");
        return 0;
    }
    
    const char *str_content = json_get_string(retrieved_str);
    if (!str_content || strcmp(str_content, "array_string") != 0) {
        log_test("FAIL: Retrieved string element has incorrect value");
        return 0;
    }
    
    json_value_t *retrieved_num = json_array_get(arr, 1);
    if (!retrieved_num) {
        log_test("FAIL: Failed to retrieve number element from array");
        return 0;
    }
    
    double num_content = json_get_number(retrieved_num);
    if (num_content != 99.9) {
        log_test("FAIL: Retrieved number element has incorrect value");
        return 0;
    }
    
    json_value_t *retrieved_obj = json_array_get(arr, 2);
    if (!retrieved_obj) {
        log_test("FAIL: Failed to retrieve object element from array");
        return 0;
    }
    
    json_value_t *nested_val = json_object_get(retrieved_obj, "nested_key");
    if (!nested_val) {
        log_test("FAIL: Failed to retrieve nested property");
        return 0;
    }
    
    const char *nested_content = json_get_string(nested_val);
    if (!nested_content || strcmp(nested_content, "nested_value") != 0) {
        log_test("FAIL: Retrieved nested property has incorrect value");
        return 0;
    }
    
    log_test("PASS: JSON array operations work correctly");
    return 1;
}

/* Test JSON parsing */
int test_json_parse() {
    log_test("Testing JSON parsing");
    
    const char *json_str = "{"
        "\"string_key\": \"string_value\","
        "\"number_key\": 123.456,"
        "\"bool_key\": true,"
        "\"null_key\": null,"
        "\"array_key\": [1, 2, \"three\", {}],"
        "\"object_key\": {\"nested\": \"value\"}"
    "}";
    
    json_value_t *parsed = json_parse(json_str, strlen(json_str));
    if (!parsed) {
        log_test("FAIL: Failed to parse valid JSON string");
        return 0;
    }
    
    // Verify the parsed JSON structure
    json_value_t *str_val = json_object_get(parsed, "string_key");
    if (!str_val || strcmp(json_get_string(str_val), "string_value") != 0) {
        log_test("FAIL: String value not correctly parsed");
        return 0;
    }
    
    json_value_t *num_val = json_object_get(parsed, "number_key");
    if (!num_val || json_get_number(num_val) != 123.456) {
        log_test("FAIL: Number value not correctly parsed");
        return 0;
    }
    
    json_value_t *bool_val = json_object_get(parsed, "bool_key");
    if (!bool_val || json_get_boolean(bool_val) != 1) {
        log_test("FAIL: Boolean value not correctly parsed");
        return 0;
    }
    
    json_value_t *null_val = json_object_get(parsed, "null_key");
    if (!null_val || json_get_type(null_val) != JSON_NULL) {
        log_test("FAIL: Null value not correctly parsed");
        return 0;
    }
    
    json_value_t *arr_val = json_object_get(parsed, "array_key");
    if (!arr_val || json_get_type(arr_val) != JSON_ARRAY || json_array_size(arr_val) != 4) {
        log_test("FAIL: Array value not correctly parsed");
        return 0;
    }
    
    json_value_t *obj_val = json_object_get(parsed, "object_key");
    if (!obj_val || json_get_type(obj_val) != JSON_OBJECT) {
        log_test("FAIL: Object value not correctly parsed");
        return 0;
    }
    
    json_value_t *nested_val = json_object_get(obj_val, "nested");
    if (!nested_val || strcmp(json_get_string(nested_val), "value") != 0) {
        log_test("FAIL: Nested object value not correctly parsed");
        return 0;
    }
    
    log_test("PASS: JSON parsing works correctly");
    return 1;
}

/* Test JSON serialization */
int test_json_serialize() {
    log_test("Testing JSON serialization");
    
    // Create a complex JSON structure
    json_value_t *obj = json_create_object();
    json_object_set(obj, "string_key", json_create_string("string_value"));
    json_object_set(obj, "number_key", json_create_number(123.456));
    json_object_set(obj, "bool_key", json_create_boolean(1));
    json_object_set(obj, "null_key", json_create_null());
    
    json_value_t *arr = json_create_array();
    json_array_append(arr, json_create_number(1));
    json_array_append(arr, json_create_number(2));
    json_array_append(arr, json_create_string("three"));
    json_array_append(arr, json_create_object());
    json_object_set(obj, "array_key", arr);
    
    json_value_t *nested_obj = json_create_object();
    json_object_set(nested_obj, "nested", json_create_string("value"));
    json_object_set(obj, "object_key", nested_obj);
    
    // Serialize to string
    char json_str[MAX_JSON_SAMPLE];
    size_t len = json_serialize(obj, json_str, MAX_JSON_SAMPLE);
    
    if (len == 0 || len >= MAX_JSON_SAMPLE) {
        log_test("FAIL: JSON serialization failed or produced too large output");
        return 0;
    }
    
    // Parse the serialized string back to verify round-trip
    json_value_t *reparsed = json_parse(json_str, len);
    if (!reparsed) {
        log_test("FAIL: Failed to parse serialized JSON string");
        return 0;
    }
    
    // Verify the structure (basic check)
    json_value_t *str_val = json_object_get(reparsed, "string_key");
    if (!str_val || strcmp(json_get_string(str_val), "string_value") != 0) {
        log_test("FAIL: String value not correctly round-tripped");
        return 0;
    }
    
    json_value_t *arr_val = json_object_get(reparsed, "array_key");
    if (!arr_val || json_get_type(arr_val) != JSON_ARRAY || json_array_size(arr_val) != 4) {
        log_test("FAIL: Array value not correctly round-tripped");
        return 0;
    }
    
    log_test("PASS: JSON serialization works correctly");
    return 1;
}

/* Test handling of invalid JSON inputs */
int test_json_invalid_inputs() {
    log_test("Testing handling of invalid JSON inputs");
    
    // Missing closing brace
    const char *invalid_json1 = "{\"key\": \"value\"";
    json_value_t *result1 = json_parse(invalid_json1, strlen(invalid_json1));
    if (result1) {
        log_test("FAIL: Parser accepted invalid JSON with missing closing brace");
        return 0;
    }
    
    // Invalid array
    const char *invalid_json2 = "[1, 2, 3,]";
    json_value_t *result2 = json_parse(invalid_json2, strlen(invalid_json2));
    if (result2) {
        log_test("FAIL: Parser accepted invalid JSON with trailing comma in array");
        return 0;
    }
    
    // Invalid key (no quotes)
    const char *invalid_json3 = "{key: \"value\"}";
    json_value_t *result3 = json_parse(invalid_json3, strlen(invalid_json3));
    if (result3) {
        log_test("FAIL: Parser accepted invalid JSON with unquoted key");
        return 0;
    }
    
    log_test("PASS: Invalid JSON handling works correctly");
    return 1;
}

int main() {
    int success_count = 0;
    int total_tests = 0;
    
    test_init();
    
    // Run the tests
    total_tests++;
    success_count += test_json_object_ops();
    
    total_tests++;
    success_count += test_json_array_ops();
    
    total_tests++;
    success_count += test_json_parse();
    
    total_tests++;
    success_count += test_json_serialize();
    
    total_tests++;
    success_count += test_json_invalid_inputs();
    
    // Print summary
    printf("\nTest Summary: %d/%d tests passed\n", success_count, total_tests);
    log_test("\nDetails: JSON utilities test suite");
    
    if (success_count == total_tests) {
        log_test("PASS: All JSON utility tests passed");
    } else {
        log_test("FAIL: Some JSON utility tests failed");
    }
    
    test_cleanup();
    return (success_count == total_tests) ? EXIT_SUCCESS : EXIT_FAILURE;
}