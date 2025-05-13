#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>

#define TEST_LOG_FILE "../test_logs/integration_server_api.log"
#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 8192
#define REQUEST_TIMEOUT_SEC 5

FILE *log_file = NULL;

/* Logging functions */
void log_test(const char *message) {
    if (log_file) {
        fprintf(log_file, "[%s] %s\n", __TIME__, message);
    }
    printf("%s\n", message);
}

void log_test_f(const char *format, ...) {
    char buffer[1024];
    va_list args;
    
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    log_test(buffer);
}

void test_init() {
    log_file = fopen(TEST_LOG_FILE, "w");
    if (!log_file) {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
    }
    
    log_test("Starting server API integration tests");
}

void test_cleanup() {
    if (log_file) {
        fprintf(log_file, "Time: %ld ms\n", clock() / (CLOCKS_PER_SEC / 1000));
        fclose(log_file);
    }
}

/* Helper function to make an HTTP request */
char* make_http_request(const char* method, const char* path, const char* body, int* status_code) {
    int sockfd;
    struct sockaddr_in server_addr;
    char request[BUFFER_SIZE];
    char *response = (char*)malloc(BUFFER_SIZE);
    
    if (!response) {
        log_test("Failed to allocate memory for response");
        return NULL;
    }
    
    // Initialize response buffer
    memset(response, 0, BUFFER_SIZE);
    
    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        log_test("Socket creation failed");
        free(response);
        return NULL;
    }
    
    // Set timeout
    struct timeval timeout;
    timeout.tv_sec = REQUEST_TIMEOUT_SEC;
    timeout.tv_usec = 0;
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        log_test("setsockopt failed");
        close(sockfd);
        free(response);
        return NULL;
    }
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        log_test("setsockopt failed");
        close(sockfd);
        free(response);
        return NULL;
    }
    
    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    
    if (inet_pton(AF_INET, SERVER_HOST, &server_addr.sin_addr) <= 0) {
        log_test("Invalid address / Address not supported");
        close(sockfd);
        free(response);
        return NULL;
    }
    
    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_test_f("Connection failed to %s:%d - Is the server running?", SERVER_HOST, SERVER_PORT);
        close(sockfd);
        free(response);
        return NULL;
    }
    
    // Prepare HTTP request
    int content_length = body ? strlen(body) : 0;
    
    snprintf(request, BUFFER_SIZE,
        "%s %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Connection: close\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s",
        method, path, SERVER_HOST, SERVER_PORT,
        content_length, body ? body : "");
    
    // Send request
    if (send(sockfd, request, strlen(request), 0) < 0) {
        log_test("Failed to send request");
        close(sockfd);
        free(response);
        return NULL;
    }
    
    // Receive response
    int total_bytes = 0;
    int bytes_received = 0;
    
    while ((bytes_received = recv(sockfd, response + total_bytes, 
                                 BUFFER_SIZE - total_bytes - 1, 0)) > 0) {
        total_bytes += bytes_received;
        if (total_bytes >= BUFFER_SIZE - 1) {
            break;
        }
    }
    
    close(sockfd);
    
    if (total_bytes <= 0) {
        log_test("No response received from server");
        free(response);
        return NULL;
    }
    
    response[total_bytes] = '\0';
    
    // Extract status code
    if (status_code) {
        char protocol[32];
        if (sscanf(response, "%s %d", protocol, status_code) != 2) {
            *status_code = -1;
        }
    }
    
    return response;
}

/* Health API tests */
int test_health_endpoint() {
    log_test("Testing /health endpoint");
    
    int status_code;
    char *response = make_http_request("GET", "/health", NULL, &status_code);
    
    if (!response) {
        log_test("FAIL: Failed to get response from /health endpoint");
        return 0;
    }
    
    if (status_code != 200) {
        log_test_f("FAIL: Unexpected status code %d from /health endpoint", status_code);
        free(response);
        return 0;
    }
    
    // Check for expected fields in response
    if (strstr(response, "\"status\"") == NULL || 
        strstr(response, "\"timestamp\"") == NULL) {
        log_test("FAIL: Health response missing required fields");
        free(response);
        return 0;
    }
    
    log_test("PASS: Health endpoint returned expected response");
    free(response);
    return 1;
}

/* Metrics API test */
int test_metrics_endpoint() {
    log_test("Testing /metrics endpoint");
    
    int status_code;
    char *response = make_http_request("GET", "/metrics", NULL, &status_code);
    
    if (!response) {
        log_test("FAIL: Failed to get response from /metrics endpoint");
        return 0;
    }
    
    if (status_code != 200) {
        log_test_f("FAIL: Unexpected status code %d from /metrics endpoint", status_code);
        free(response);
        return 0;
    }
    
    // Check for expected fields in metrics response
    if (strstr(response, "\"system_metrics\"") == NULL) {
        log_test("FAIL: Metrics response missing system_metrics field");
        free(response);
        return 0;
    }
    
    log_test("PASS: Metrics endpoint returned expected response");
    free(response);
    return 1;
}

/* Available metrics test */
int test_metrics_available_endpoint() {
    log_test("Testing /metrics/available endpoint");
    
    int status_code;
    char *response = make_http_request("GET", "/metrics/available", NULL, &status_code);
    
    if (!response) {
        log_test("FAIL: Failed to get response from /metrics/available endpoint");
        return 0;
    }
    
    if (status_code != 200) {
        log_test_f("FAIL: Unexpected status code %d from /metrics/available endpoint", status_code);
        free(response);
        return 0;
    }
    
    // Check for metrics list
    if (strstr(response, "\"available_metrics\"") == NULL) {
        log_test("FAIL: Available metrics response missing available_metrics field");
        free(response);
        return 0;
    }
    
    log_test("PASS: Available metrics endpoint returned expected response");
    free(response);
    return 1;
}

/* Test document create/read/update/delete operations */
int test_basic_crud_operations() {
    log_test("Testing basic CRUD operations");
    
    // Create a test document
    const char *create_body = "{\"collection\":\"test_collection\",\"document\":{\"name\":\"test document\",\"value\":42}}";
    int status_code;
    char *response = make_http_request("POST", "/api/document", create_body, &status_code);
    
    if (!response) {
        log_test("FAIL: Failed to create test document");
        return 0;
    }
    
    if (status_code != 201 && status_code != 200) {
        log_test_f("FAIL: Unexpected status code %d when creating document", status_code);
        free(response);
        return 0;
    }
    
    // Extract document ID from response
    char *id_start = strstr(response, "\"id\":");
    if (!id_start) {
        log_test("FAIL: Document creation response missing id field");
        free(response);
        return 0;
    }
    
    char document_id[64] = {0};
    if (sscanf(id_start + 5, "\"%63[^\"]\"", document_id) != 1) {
        log_test("FAIL: Failed to extract document ID from response");
        free(response);
        return 0;
    }
    
    free(response);
    log_test_f("Document created with ID: %s", document_id);
    
    // Read the document
    char get_path[128];
    snprintf(get_path, sizeof(get_path), "/api/document/test_collection/%s", document_id);
    
    response = make_http_request("GET", get_path, NULL, &status_code);
    
    if (!response) {
        log_test("FAIL: Failed to read test document");
        return 0;
    }
    
    if (status_code != 200) {
        log_test_f("FAIL: Unexpected status code %d when reading document", status_code);
        free(response);
        return 0;
    }
    
    // Verify document contents
    if (strstr(response, "\"name\":\"test document\"") == NULL || 
        strstr(response, "\"value\":42") == NULL) {
        log_test("FAIL: Document contents not as expected");
        free(response);
        return 0;
    }
    
    free(response);
    
    // Update the document
    char update_body[256];
    snprintf(update_body, sizeof(update_body), 
        "{\"collection\":\"test_collection\",\"id\":\"%s\",\"document\":{\"name\":\"updated document\",\"value\":99}}",
        document_id);
    
    response = make_http_request("PUT", "/api/document", update_body, &status_code);
    
    if (!response) {
        log_test("FAIL: Failed to update test document");
        return 0;
    }
    
    if (status_code != 200) {
        log_test_f("FAIL: Unexpected status code %d when updating document", status_code);
        free(response);
        return 0;
    }
    
    free(response);
    
    // Verify the update
    response = make_http_request("GET", get_path, NULL, &status_code);
    
    if (!response) {
        log_test("FAIL: Failed to read updated document");
        return 0;
    }
    
    if (strstr(response, "\"name\":\"updated document\"") == NULL || 
        strstr(response, "\"value\":99") == NULL) {
        log_test("FAIL: Document update not reflected in contents");
        free(response);
        return 0;
    }
    
    free(response);
    
    // Delete the document
    char delete_path[128];
    snprintf(delete_path, sizeof(delete_path), "/api/document/test_collection/%s", document_id);
    
    response = make_http_request("DELETE", delete_path, NULL, &status_code);
    
    if (!response) {
        log_test("FAIL: Failed to delete test document");
        return 0;
    }
    
    if (status_code != 200) {
        log_test_f("FAIL: Unexpected status code %d when deleting document", status_code);
        free(response);
        return 0;
    }
    
    free(response);
    
    // Verify deletion
    response = make_http_request("GET", get_path, NULL, &status_code);
    
    if (status_code != 404) {
        log_test_f("FAIL: Document still exists after deletion (status: %d)", status_code);
        free(response);
        return 0;
    }
    
    free(response);
    
    log_test("PASS: Basic CRUD operations work correctly");
    return 1;
}

int main() {
    int success_count = 0;
    int total_tests = 0;
    
    test_init();
    
    log_test_f("Testing server API at %s:%d", SERVER_HOST, SERVER_PORT);
    log_test("Note: Server must be running for these tests to pass");
    
    // Run the tests
    total_tests++;
    success_count += test_health_endpoint();
    
    total_tests++;
    success_count += test_metrics_endpoint();
    
    total_tests++;
    success_count += test_metrics_available_endpoint();
    
    total_tests++;
    success_count += test_basic_crud_operations();
    
    // Print summary
    printf("\nTest Summary: %d/%d tests passed\n", success_count, total_tests);
    log_test("\nDetails: Server API integration test suite");
    
    if (success_count == total_tests) {
        log_test("PASS: All server API integration tests passed");
    } else {
        log_test("FAIL: Some server API integration tests failed");
    }
    
    test_cleanup();
    return (success_count == total_tests) ? EXIT_SUCCESS : EXIT_FAILURE;
}