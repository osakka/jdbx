#include "jsondb/utils/input_validation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*
 * Basic test framework for input validation functions
 */

#define TEST_START(name) printf("Running test: %s... ", name)
#define TEST_PASS() printf("PASS\n")
#define TEST_FAIL(msg) do { printf("FAIL: %s\n", msg); exit(1); } while(0)

/* Test validation_error_string function */
void test_validation_error_string() {
    TEST_START("validation_error_string");
    
    /* Test some error codes */
    assert(strcmp(validation_error_string(VALIDATION_SUCCESS), "Validation succeeded") == 0);
    assert(strcmp(validation_error_string(VALIDATION_ERROR_NULL_INPUT), "Null input provided") == 0);
    assert(strcmp(validation_error_string(VALIDATION_ERROR_EMPTY_INPUT), "Empty input provided") == 0);
    assert(strcmp(validation_error_string(VALIDATION_ERROR_TOO_LONG), "Input exceeds maximum length") == 0);
    
    TEST_PASS();
}

/* Test validate_string function */
void test_validate_string() {
    TEST_START("validate_string");
    
    /* Test valid strings */
    assert(validate_string("test", 1, 10, NULL) == VALIDATION_SUCCESS);
    assert(validate_string("abcdef", 1, 10, "abcdef") == VALIDATION_SUCCESS);
    assert(validate_string("1234", 1, 10, "0123456789") == VALIDATION_SUCCESS);
    
    /* Test null input */
    assert(validate_string(NULL, 1, 10, NULL) == VALIDATION_ERROR_NULL_INPUT);
    
    /* Test empty input */
    assert(validate_string("", 1, 10, NULL) == VALIDATION_ERROR_EMPTY_INPUT);
    
    /* Test too long input */
    assert(validate_string("12345678901", 1, 10, NULL) == VALIDATION_ERROR_TOO_LONG);
    
    /* Test invalid characters */
    assert(validate_string("abc123", 1, 10, "abc") == VALIDATION_ERROR_INVALID_CHARS);
    
    TEST_PASS();
}

/* Test validate_alphanumeric function */
void test_validate_alphanumeric() {
    TEST_START("validate_alphanumeric");
    
    /* Test valid alphanumeric strings */
    assert(validate_alphanumeric("abc123", 1, 10) == VALIDATION_SUCCESS);
    assert(validate_alphanumeric("ABC123", 1, 10) == VALIDATION_SUCCESS);
    assert(validate_alphanumeric("a1B2c3", 1, 10) == VALIDATION_SUCCESS);
    
    /* Test null input */
    assert(validate_alphanumeric(NULL, 1, 10) == VALIDATION_ERROR_NULL_INPUT);
    
    /* Test empty input */
    assert(validate_alphanumeric("", 1, 10) == VALIDATION_ERROR_EMPTY_INPUT);
    
    /* Test too long input */
    assert(validate_alphanumeric("12345678901", 1, 10) == VALIDATION_ERROR_TOO_LONG);
    
    /* Test invalid characters */
    assert(validate_alphanumeric("abc_123", 1, 10) == VALIDATION_ERROR_INVALID_CHARS);
    assert(validate_alphanumeric("abc-123", 1, 10) == VALIDATION_ERROR_INVALID_CHARS);
    assert(validate_alphanumeric("abc$123", 1, 10) == VALIDATION_ERROR_INVALID_CHARS);
    
    TEST_PASS();
}

/* Test validate_identifier function */
void test_validate_identifier() {
    TEST_START("validate_identifier");
    
    /* Test valid identifiers */
    assert(validate_identifier("test", 1, 10) == VALIDATION_SUCCESS);
    assert(validate_identifier("test123", 1, 10) == VALIDATION_SUCCESS);
    assert(validate_identifier("test_123", 1, 10) == VALIDATION_SUCCESS);
    assert(validate_identifier("a_1", 1, 10) == VALIDATION_SUCCESS);
    
    /* Test null input */
    assert(validate_identifier(NULL, 1, 10) == VALIDATION_ERROR_NULL_INPUT);
    
    /* Test empty input */
    assert(validate_identifier("", 1, 10) == VALIDATION_ERROR_EMPTY_INPUT);
    
    /* Test too long input */
    assert(validate_identifier("test12345678", 1, 10) == VALIDATION_ERROR_TOO_LONG);
    
    /* Test invalid first character */
    assert(validate_identifier("1test", 1, 10) == VALIDATION_ERROR_INVALID_FORMAT);
    assert(validate_identifier("_test", 1, 10) == VALIDATION_ERROR_INVALID_FORMAT);
    
    /* Test invalid characters */
    assert(validate_identifier("test-123", 1, 10) == VALIDATION_ERROR_INVALID_CHARS);
    assert(validate_identifier("test$123", 1, 10) == VALIDATION_ERROR_INVALID_CHARS);
    
    TEST_PASS();
}

/* Test validate_path function */
void test_validate_path() {
    TEST_START("validate_path");
    
    /* Test valid paths */
    assert(validate_path("/etc/passwd") == VALIDATION_SUCCESS);
    assert(validate_path("./file.txt") == VALIDATION_SUCCESS);
    assert(validate_path("file.txt") == VALIDATION_SUCCESS);
    
    /* Test null input */
    assert(validate_path(NULL) == VALIDATION_ERROR_NULL_INPUT);
    
    /* Test empty input */
    assert(validate_path("") == VALIDATION_ERROR_EMPTY_INPUT);
    
    /* Test path traversal */
    assert(validate_path("../file.txt") == VALIDATION_ERROR_PATH_TRAVERSAL);
    assert(validate_path("directory/../file.txt") == VALIDATION_ERROR_PATH_TRAVERSAL);
    assert(validate_path("..") == VALIDATION_ERROR_PATH_TRAVERSAL);
    assert(validate_path(".") == VALIDATION_ERROR_PATH_TRAVERSAL);
    
    TEST_PASS();
}

/* Test validate_int_range function */
void test_validate_int_range() {
    TEST_START("validate_int_range");
    
    /* Test in range */
    assert(validate_int_range(5, 0, 10) == VALIDATION_SUCCESS);
    assert(validate_int_range(0, 0, 10) == VALIDATION_SUCCESS);
    assert(validate_int_range(10, 0, 10) == VALIDATION_SUCCESS);
    
    /* Test out of range */
    assert(validate_int_range(-1, 0, 10) == VALIDATION_ERROR_OUT_OF_RANGE);
    assert(validate_int_range(11, 0, 10) == VALIDATION_ERROR_OUT_OF_RANGE);
    
    TEST_PASS();
}

/* Test validate_double_range function */
void test_validate_double_range() {
    TEST_START("validate_double_range");
    
    /* Test in range */
    assert(validate_double_range(5.0, 0.0, 10.0) == VALIDATION_SUCCESS);
    assert(validate_double_range(0.0, 0.0, 10.0) == VALIDATION_SUCCESS);
    assert(validate_double_range(10.0, 0.0, 10.0) == VALIDATION_SUCCESS);
    assert(validate_double_range(0.5, 0.0, 10.0) == VALIDATION_SUCCESS);
    
    /* Test out of range */
    assert(validate_double_range(-0.1, 0.0, 10.0) == VALIDATION_ERROR_OUT_OF_RANGE);
    assert(validate_double_range(10.1, 0.0, 10.0) == VALIDATION_ERROR_OUT_OF_RANGE);
    
    TEST_PASS();
}

/* Test validate_json function */
void test_validate_json() {
    TEST_START("validate_json");
    
    /* Test valid JSON */
    assert(validate_json("{\"key\":\"value\"}", 15, NULL) == VALIDATION_SUCCESS);
    assert(validate_json("[1,2,3]", 7, NULL) == VALIDATION_SUCCESS);
    assert(validate_json("null", 4, NULL) == VALIDATION_SUCCESS);
    
    /* Test null input */
    assert(validate_json(NULL, 0, NULL) == VALIDATION_ERROR_NULL_INPUT);
    
    /* Test empty input */
    assert(validate_json("", 0, NULL) == VALIDATION_ERROR_EMPTY_INPUT);
    
    /* Test invalid JSON */
    assert(validate_json("{key:value}", 12, NULL) == VALIDATION_ERROR_JSON_FORMAT);
    assert(validate_json("[1,2,", 5, NULL) == VALIDATION_ERROR_JSON_FORMAT);
    
    TEST_PASS();
}

/* Test validate_url function */
void test_validate_url() {
    TEST_START("validate_url");
    
    /* Test valid URLs */
    assert(validate_url("http://example.com") == VALIDATION_SUCCESS);
    assert(validate_url("https://example.com/path") == VALIDATION_SUCCESS);
    assert(validate_url("ftp://example.com:21") == VALIDATION_SUCCESS);
    
    /* Test null input */
    assert(validate_url(NULL) == VALIDATION_ERROR_NULL_INPUT);
    
    /* Test empty input */
    assert(validate_url("") == VALIDATION_ERROR_EMPTY_INPUT);
    
    /* Test invalid URLs */
    assert(validate_url("not a url") == VALIDATION_ERROR_INVALID_FORMAT);
    assert(validate_url("http:example.com") == VALIDATION_ERROR_INVALID_FORMAT);
    assert(validate_url("http://") == VALIDATION_ERROR_INVALID_FORMAT);
    
    TEST_PASS();
}

/* Test validate_email function */
void test_validate_email() {
    TEST_START("validate_email");
    
    /* Test valid emails */
    assert(validate_email("user@example.com") == VALIDATION_SUCCESS);
    assert(validate_email("user.name@example.com") == VALIDATION_SUCCESS);
    assert(validate_email("user+tag@example.com") == VALIDATION_SUCCESS);
    
    /* Test null input */
    assert(validate_email(NULL) == VALIDATION_ERROR_NULL_INPUT);
    
    /* Test empty input */
    assert(validate_email("") == VALIDATION_ERROR_EMPTY_INPUT);
    
    /* Test invalid emails */
    assert(validate_email("not an email") == VALIDATION_ERROR_INVALID_FORMAT);
    assert(validate_email("user@") == VALIDATION_ERROR_INVALID_FORMAT);
    assert(validate_email("@example.com") == VALIDATION_ERROR_INVALID_FORMAT);
    assert(validate_email("user@example") == VALIDATION_ERROR_INVALID_FORMAT);
    
    TEST_PASS();
}

/* Test validate_ip_address function */
void test_validate_ip_address() {
    TEST_START("validate_ip_address");
    
    /* Test valid IPv4 addresses */
    assert(validate_ip_address("192.168.1.1") == VALIDATION_SUCCESS);
    assert(validate_ip_address("127.0.0.1") == VALIDATION_SUCCESS);
    assert(validate_ip_address("255.255.255.255") == VALIDATION_SUCCESS);
    
    /* Test valid IPv6 addresses */
    assert(validate_ip_address("2001:db8::1") == VALIDATION_SUCCESS);
    assert(validate_ip_address("::1") == VALIDATION_SUCCESS);
    assert(validate_ip_address("2001:db8:0:0:0:0:0:1") == VALIDATION_SUCCESS);
    
    /* Test null input */
    assert(validate_ip_address(NULL) == VALIDATION_ERROR_NULL_INPUT);
    
    /* Test empty input */
    assert(validate_ip_address("") == VALIDATION_ERROR_EMPTY_INPUT);
    
    /* Test invalid IP addresses */
    assert(validate_ip_address("not an ip") == VALIDATION_ERROR_INVALID_FORMAT);
    assert(validate_ip_address("300.168.1.1") == VALIDATION_ERROR_INVALID_FORMAT);
    assert(validate_ip_address("192.168.1") == VALIDATION_ERROR_INVALID_FORMAT);
    assert(validate_ip_address("2001:db8::zzz") == VALIDATION_ERROR_INVALID_FORMAT);
    
    TEST_PASS();
}

/* Test sanitize_string function */
void test_sanitize_string() {
    TEST_START("sanitize_string");
    
    char output[256];
    
    /* Test normal string */
    assert(sanitize_string("Hello, World!", output, sizeof(output)) == 0);
    assert(strcmp(output, "Hello, World!") == 0);
    
    /* Test HTML special characters */
    assert(sanitize_string("<script>alert('XSS');</script>", output, sizeof(output)) == 0);
    assert(strcmp(output, "&lt;script&gt;alert(&apos;XSS&apos;);&lt;/script&gt;") == 0);
    
    /* Test null input */
    assert(sanitize_string(NULL, output, sizeof(output)) == -1);
    
    /* Test null output */
    assert(sanitize_string("test", NULL, 0) == -1);
    
    TEST_PASS();
}

/* Test sanitize_collection_name function */
void test_sanitize_collection_name() {
    TEST_START("sanitize_collection_name");
    
    char output[256];
    
    /* Test valid collection name */
    assert(sanitize_collection_name("users", output, sizeof(output)) == 0);
    assert(strcmp(output, "users") == 0);
    
    /* Test with invalid characters */
    assert(sanitize_collection_name("users-table", output, sizeof(output)) == 0);
    assert(strcmp(output, "userstable") == 0);
    
    /* Test with invalid first character */
    assert(sanitize_collection_name("1users", output, sizeof(output)) == 0);
    assert(strcmp(output, "users") == 0);
    
    /* Test null input */
    assert(sanitize_collection_name(NULL, output, sizeof(output)) == -1);
    
    /* Test null output */
    assert(sanitize_collection_name("test", NULL, 0) == -1);
    
    TEST_PASS();
}

/* Test sanitize_document_id function */
void test_sanitize_document_id() {
    TEST_START("sanitize_document_id");
    
    char output[256];
    
    /* Test valid document ID */
    assert(sanitize_document_id("user-123", output, sizeof(output)) == 0);
    assert(strcmp(output, "user-123") == 0);
    
    /* Test with invalid characters */
    assert(sanitize_document_id("user/123", output, sizeof(output)) == 0);
    assert(strcmp(output, "user123") == 0);
    
    /* Test null input */
    assert(sanitize_document_id(NULL, output, sizeof(output)) == -1);
    
    /* Test null output */
    assert(sanitize_document_id("test", NULL, 0) == -1);
    
    TEST_PASS();
}

/* Test validate_collection_name function */
void test_validate_collection_name() {
    TEST_START("validate_collection_name");
    
    /* Test valid collection names */
    assert(validate_collection_name("users") == VALIDATION_SUCCESS);
    assert(validate_collection_name("user_data") == VALIDATION_SUCCESS);
    assert(validate_collection_name("u") == VALIDATION_SUCCESS);
    
    /* Test null input */
    assert(validate_collection_name(NULL) == VALIDATION_ERROR_NULL_INPUT);
    
    /* Test empty input */
    assert(validate_collection_name("") == VALIDATION_ERROR_EMPTY_INPUT);
    
    /* Test too long input */
    char long_name[200];
    memset(long_name, 'a', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';
    assert(validate_collection_name(long_name) == VALIDATION_ERROR_TOO_LONG);
    
    /* Test invalid first character */
    assert(validate_collection_name("1users") == VALIDATION_ERROR_INVALID_FORMAT);
    assert(validate_collection_name("_users") == VALIDATION_ERROR_INVALID_FORMAT);
    
    /* Test invalid characters */
    assert(validate_collection_name("users-table") == VALIDATION_ERROR_INVALID_CHARS);
    
    TEST_PASS();
}

/* Test validate_document_id function */
void test_validate_document_id() {
    TEST_START("validate_document_id");
    
    /* Test valid document IDs */
    assert(validate_document_id("doc123") == VALIDATION_SUCCESS);
    assert(validate_document_id("doc-123") == VALIDATION_SUCCESS);
    assert(validate_document_id("doc_123") == VALIDATION_SUCCESS);
    assert(validate_document_id("doc.123") == VALIDATION_SUCCESS);
    
    /* Test null input */
    assert(validate_document_id(NULL) == VALIDATION_ERROR_NULL_INPUT);
    
    /* Test empty input */
    assert(validate_document_id("") == VALIDATION_ERROR_EMPTY_INPUT);
    
    /* Test too long input */
    char long_id[300];
    memset(long_id, 'a', sizeof(long_id) - 1);
    long_id[sizeof(long_id) - 1] = '\0';
    assert(validate_document_id(long_id) == VALIDATION_ERROR_TOO_LONG);
    
    /* Test invalid characters */
    assert(validate_document_id("doc/123") == VALIDATION_ERROR_INVALID_CHARS);
    assert(validate_document_id("doc#123") == VALIDATION_ERROR_INVALID_CHARS);
    
    TEST_PASS();
}

int main() {
    printf("Running input validation tests...\n");
    
    test_validation_error_string();
    test_validate_string();
    test_validate_alphanumeric();
    test_validate_identifier();
    test_validate_path();
    test_validate_int_range();
    test_validate_double_range();
    test_validate_json();
    test_validate_url();
    test_validate_email();
    test_validate_ip_address();
    test_sanitize_string();
    test_sanitize_collection_name();
    test_sanitize_document_id();
    test_validate_collection_name();
    test_validate_document_id();
    
    printf("All input validation tests passed!\n");
    return 0;
}