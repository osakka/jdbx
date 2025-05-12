#include "../include/transaction.h"
#include "../include/json.h"
#include "../include/server.h"
#include "../include/utils/json_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int main() {
    printf("Testing JSON helpers...\n");
    
    /* Test http_response_json_string function */
    const char* test_json = "{\"test\":\"value\"}";
    http_response_t* response = http_response_json_string(test_json, 200);
    
    assert(response != NULL);
    assert(response->status == 200);
    assert(strcmp(response->body, test_json) == 0);
    assert(strcmp(response->content_type, "application/json") == 0);
    
    free_http_response(response);
    
    printf("Test passed!\n");
    
    return 0;
}