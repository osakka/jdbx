#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Simplified version of the necessary structures for testing */
typedef struct {
    int status;
    char* body;
    char* content_type;
    size_t content_length;
    char** headers;
    size_t num_headers;
} http_response_t;

/* Simplified version of http_response_error */
http_response_t* http_response_error(const char* message, int status_code) {
    printf("Error would be: %s (status %d)\n", message, status_code);
    return NULL;
}

/* Implement the function we're testing */
static http_response_t* http_response_json_string(const char* json_str, int status_code) {
    if (!json_str) {
        return http_response_error("Invalid JSON string", 500);
    }
    
    http_response_t* response = (http_response_t*)malloc(sizeof(http_response_t));
    if (!response) {
        return NULL;
    }
    
    response->status = status_code;
    response->body = strdup(json_str);
    response->content_type = strdup("application/json");
    response->content_length = strlen(json_str);
    response->headers = NULL;
    response->num_headers = 0;
    
    return response;
}

/* Simplified free function */
void free_http_response(http_response_t* response) {
    if (response) {
        if (response->body) free(response->body);
        if (response->content_type) free(response->content_type);
        free(response);
    }
}

int main() {
    printf("Testing simplified JSON helpers...\n");
    
    /* Test http_response_json_string function */
    const char* test_json = "{\"test\":\"value\"}";
    http_response_t* response = http_response_json_string(test_json, 200);
    
    assert(response != NULL);
    assert(response->status == 200);
    assert(strcmp(response->body, test_json) == 0);
    assert(strcmp(response->content_type, "application/json") == 0);
    
    free_http_response(response);
    
    printf("Simplified test passed!\n");
    
    return 0;
}