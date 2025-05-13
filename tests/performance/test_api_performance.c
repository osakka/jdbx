#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <pthread.h>

#define TEST_LOG_FILE "../test_logs/performance_api.log"
#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 8192
#define REQUEST_TIMEOUT_SEC 5

// Test configuration
#define NUM_THREADS 10
#define REQUESTS_PER_THREAD 100
#define DOCUMENT_SIZE 2048  // Approximate size of test document in bytes
#define TEST_COLLECTION "perf_test_collection"

FILE *log_file = NULL;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Logging functions */
void log_test(const char *message) {
    pthread_mutex_lock(&log_mutex);
    if (log_file) {
        fprintf(log_file, "[%s] %s\n", __TIME__, message);
    }
    printf("%s\n", message);
    pthread_mutex_unlock(&log_mutex);
}

void log_test_f(const char *format, ...) {
    char buffer[1024];
    va_list args;
    
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    log_test(buffer);
}

void log_performance(const char *operation, double time_ms, int num_operations) {
    double ops_per_sec = (double)num_operations / (time_ms / 1000.0);
    pthread_mutex_lock(&log_mutex);
    if (log_file) {
        fprintf(log_file, "PERF: %s - %.2f ms total, %.2f ops/sec\n", 
                operation, time_ms, ops_per_sec);
    }
    printf("PERF: %s - %.2f ms total, %.2f ops/sec\n", 
            operation, time_ms, ops_per_sec);
    pthread_mutex_unlock(&log_mutex);
}

void test_init() {
    log_file = fopen(TEST_LOG_FILE, "w");
    if (!log_file) {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
    }
    
    log_test("Starting API performance tests");
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
        return NULL;
    }
    
    // Initialize response buffer
    memset(response, 0, BUFFER_SIZE);
    
    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        free(response);
        return NULL;
    }
    
    // Set timeout
    struct timeval timeout;
    timeout.tv_sec = REQUEST_TIMEOUT_SEC;
    timeout.tv_usec = 0;
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0 ||
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        close(sockfd);
        free(response);
        return NULL;
    }
    
    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    
    if (inet_pton(AF_INET, SERVER_HOST, &server_addr.sin_addr) <= 0) {
        close(sockfd);
        free(response);
        return NULL;
    }
    
    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
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

/* Generate a test document with specified ID */
char* generate_test_document(const char* id, int size) {
    char* document = (char*)malloc(size);
    if (!document) {
        return NULL;
    }
    
    // Base document structure
    int written = snprintf(document, size,
        "{\"collection\":\"%s\",\"document\":{"
        "\"id\":\"%s\","
        "\"timestamp\":%ld,"
        "\"name\":\"Performance Test Document %s\","
        "\"value\":%d,",
        TEST_COLLECTION, id, time(NULL), id, rand() % 1000);
    
    // Add padding data to reach desired size
    int remaining = size - written - 50; // Reserve space for closing braces
    if (remaining > 0) {
        strcat(document, "\"data\":\"");
        char* data_ptr = document + strlen(document);
        
        // Fill with random alphanumeric data
        const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        for (int i = 0; i < remaining; i++) {
            data_ptr[i] = charset[rand() % (sizeof(charset) - 1)];
        }
        data_ptr[remaining] = '\0';
        strcat(document, "\"");
    }
    
    // Close the JSON document
    strcat(document, "}}");
    return document;
}

/* Thread data structure */
typedef struct {
    int thread_id;
    char **document_ids;
    int num_requests;
    double insert_time;
    double get_time;
    double update_time;
    double delete_time;
    int success_count;
} thread_data_t;

/* Thread function to test document creation */
void* test_document_creation_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    int successes = 0;
    
    clock_t start_time = clock();
    
    for (int i = 0; i < data->num_requests; i++) {
        char doc_id[64];
        snprintf(doc_id, sizeof(doc_id), "thread_%d_doc_%d", data->thread_id, i);
        
        // Store document ID for later tests
        if (data->document_ids) {
            data->document_ids[i] = strdup(doc_id);
        }
        
        // Generate test document
        char* document = generate_test_document(doc_id, DOCUMENT_SIZE);
        if (!document) {
            continue;
        }
        
        // Make request
        int status_code;
        char* response = make_http_request("POST", "/api/document", document, &status_code);
        
        free(document);
        
        if (response && (status_code == 200 || status_code == 201)) {
            successes++;
        }
        
        if (response) {
            free(response);
        }
    }
    
    clock_t end_time = clock();
    data->insert_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC * 1000.0;
    data->success_count = successes;
    
    return NULL;
}

/* Thread function to test document retrieval */
void* test_document_retrieval_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    int successes = 0;
    
    clock_t start_time = clock();
    
    for (int i = 0; i < data->num_requests; i++) {
        if (!data->document_ids || !data->document_ids[i]) {
            continue;
        }
        
        char path[256];
        snprintf(path, sizeof(path), "/api/document/%s/%s", 
                TEST_COLLECTION, data->document_ids[i]);
        
        // Make request
        int status_code;
        char* response = make_http_request("GET", path, NULL, &status_code);
        
        if (response && status_code == 200) {
            successes++;
        }
        
        if (response) {
            free(response);
        }
    }
    
    clock_t end_time = clock();
    data->get_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC * 1000.0;
    data->success_count = successes;
    
    return NULL;
}

/* Thread function to test document updates */
void* test_document_update_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    int successes = 0;
    
    clock_t start_time = clock();
    
    for (int i = 0; i < data->num_requests; i++) {
        if (!data->document_ids || !data->document_ids[i]) {
            continue;
        }
        
        // Generate updated document
        char update_doc[DOCUMENT_SIZE];
        snprintf(update_doc, sizeof(update_doc),
            "{\"collection\":\"%s\",\"id\":\"%s\",\"document\":{"
            "\"id\":\"%s\","
            "\"timestamp\":%ld,"
            "\"name\":\"Updated Performance Test Document %s\","
            "\"value\":%d,"
            "\"updated\":true}}",
            TEST_COLLECTION, data->document_ids[i], 
            data->document_ids[i], time(NULL), 
            data->document_ids[i], rand() % 1000);
        
        // Make request
        int status_code;
        char* response = make_http_request("PUT", "/api/document", update_doc, &status_code);
        
        if (response && status_code == 200) {
            successes++;
        }
        
        if (response) {
            free(response);
        }
    }
    
    clock_t end_time = clock();
    data->update_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC * 1000.0;
    data->success_count = successes;
    
    return NULL;
}

/* Thread function to test document deletion */
void* test_document_deletion_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    int successes = 0;
    
    clock_t start_time = clock();
    
    for (int i = 0; i < data->num_requests; i++) {
        if (!data->document_ids || !data->document_ids[i]) {
            continue;
        }
        
        char path[256];
        snprintf(path, sizeof(path), "/api/document/%s/%s", 
                TEST_COLLECTION, data->document_ids[i]);
        
        // Make request
        int status_code;
        char* response = make_http_request("DELETE", path, NULL, &status_code);
        
        if (response && status_code == 200) {
            successes++;
        }
        
        if (response) {
            free(response);
        }
    }
    
    clock_t end_time = clock();
    data->delete_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC * 1000.0;
    data->success_count = successes;
    
    return NULL;
}

/* Test performance of document creation API */
void test_document_creation_performance() {
    log_test("Testing document creation performance");
    
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    
    // Allocate memory for document IDs
    char*** document_ids = (char***)malloc(sizeof(char**) * NUM_THREADS);
    for (int i = 0; i < NUM_THREADS; i++) {
        document_ids[i] = (char**)malloc(sizeof(char*) * REQUESTS_PER_THREAD);
        memset(document_ids[i], 0, sizeof(char*) * REQUESTS_PER_THREAD);
    }
    
    // Initialize thread data and start threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].document_ids = document_ids[i];
        thread_data[i].num_requests = REQUESTS_PER_THREAD;
        thread_data[i].success_count = 0;
        
        if (pthread_create(&threads[i], NULL, test_document_creation_thread, &thread_data[i]) != 0) {
            log_test_f("Failed to create thread %d", i);
            exit(EXIT_FAILURE);
        }
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Calculate statistics
    double total_time = 0.0;
    int total_success = 0;
    
    for (int i = 0; i < NUM_THREADS; i++) {
        total_time += thread_data[i].insert_time;
        total_success += thread_data[i].success_count;
    }
    
    double avg_time = total_time / NUM_THREADS;
    double total_requests = NUM_THREADS * REQUESTS_PER_THREAD;
    double success_rate = (total_success / total_requests) * 100.0;
    
    log_test_f("Document creation: %d threads x %d requests", NUM_THREADS, REQUESTS_PER_THREAD);
    log_test_f("Success rate: %.2f%% (%d/%d)", success_rate, total_success, (int)total_requests);
    log_performance("Document Creation API", avg_time, REQUESTS_PER_THREAD);
    
    // Keep document IDs for subsequent tests
    // Store document_ids for later freeing
    return;
}

/* Test performance of document retrieval API */
void test_document_retrieval_performance(char*** document_ids) {
    log_test("Testing document retrieval performance");
    
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    
    // Initialize thread data and start threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].document_ids = document_ids[i];
        thread_data[i].num_requests = REQUESTS_PER_THREAD;
        thread_data[i].success_count = 0;
        
        if (pthread_create(&threads[i], NULL, test_document_retrieval_thread, &thread_data[i]) != 0) {
            log_test_f("Failed to create thread %d", i);
            exit(EXIT_FAILURE);
        }
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Calculate statistics
    double total_time = 0.0;
    int total_success = 0;
    
    for (int i = 0; i < NUM_THREADS; i++) {
        total_time += thread_data[i].get_time;
        total_success += thread_data[i].success_count;
    }
    
    double avg_time = total_time / NUM_THREADS;
    double total_requests = NUM_THREADS * REQUESTS_PER_THREAD;
    double success_rate = (total_success / total_requests) * 100.0;
    
    log_test_f("Document retrieval: %d threads x %d requests", NUM_THREADS, REQUESTS_PER_THREAD);
    log_test_f("Success rate: %.2f%% (%d/%d)", success_rate, total_success, (int)total_requests);
    log_performance("Document Retrieval API", avg_time, REQUESTS_PER_THREAD);
}

/* Test performance of document update API */
void test_document_update_performance(char*** document_ids) {
    log_test("Testing document update performance");
    
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    
    // Initialize thread data and start threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].document_ids = document_ids[i];
        thread_data[i].num_requests = REQUESTS_PER_THREAD;
        thread_data[i].success_count = 0;
        
        if (pthread_create(&threads[i], NULL, test_document_update_thread, &thread_data[i]) != 0) {
            log_test_f("Failed to create thread %d", i);
            exit(EXIT_FAILURE);
        }
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Calculate statistics
    double total_time = 0.0;
    int total_success = 0;
    
    for (int i = 0; i < NUM_THREADS; i++) {
        total_time += thread_data[i].update_time;
        total_success += thread_data[i].success_count;
    }
    
    double avg_time = total_time / NUM_THREADS;
    double total_requests = NUM_THREADS * REQUESTS_PER_THREAD;
    double success_rate = (total_success / total_requests) * 100.0;
    
    log_test_f("Document update: %d threads x %d requests", NUM_THREADS, REQUESTS_PER_THREAD);
    log_test_f("Success rate: %.2f%% (%d/%d)", success_rate, total_success, (int)total_requests);
    log_performance("Document Update API", avg_time, REQUESTS_PER_THREAD);
}

/* Test performance of document deletion API */
void test_document_deletion_performance(char*** document_ids) {
    log_test("Testing document deletion performance");
    
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    
    // Initialize thread data and start threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].document_ids = document_ids[i];
        thread_data[i].num_requests = REQUESTS_PER_THREAD;
        thread_data[i].success_count = 0;
        
        if (pthread_create(&threads[i], NULL, test_document_deletion_thread, &thread_data[i]) != 0) {
            log_test_f("Failed to create thread %d", i);
            exit(EXIT_FAILURE);
        }
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Calculate statistics
    double total_time = 0.0;
    int total_success = 0;
    
    for (int i = 0; i < NUM_THREADS; i++) {
        total_time += thread_data[i].delete_time;
        total_success += thread_data[i].success_count;
    }
    
    double avg_time = total_time / NUM_THREADS;
    double total_requests = NUM_THREADS * REQUESTS_PER_THREAD;
    double success_rate = (total_success / total_requests) * 100.0;
    
    log_test_f("Document deletion: %d threads x %d requests", NUM_THREADS, REQUESTS_PER_THREAD);
    log_test_f("Success rate: %.2f%% (%d/%d)", success_rate, total_success, (int)total_requests);
    log_performance("Document Deletion API", avg_time, REQUESTS_PER_THREAD);
    
    // Free document IDs
    for (int i = 0; i < NUM_THREADS; i++) {
        for (int j = 0; j < REQUESTS_PER_THREAD; j++) {
            if (document_ids[i][j]) {
                free(document_ids[i][j]);
            }
        }
        free(document_ids[i]);
    }
    free(document_ids);
}

/* Test bulk operations performance */
void test_bulk_operations_performance() {
    log_test("Testing bulk operations performance");
    
    // Create a bulk document with multiple operations
    char bulk_path[64];
    snprintf(bulk_path, sizeof(bulk_path), "/api/bulk/%s", TEST_COLLECTION);
    
    // Generate bulk document with multiple inserts
    char* bulk_doc = (char*)malloc(DOCUMENT_SIZE * 10);
    if (!bulk_doc) {
        log_test("Failed to allocate memory for bulk document");
        return;
    }
    
    strcpy(bulk_doc, "{\"operations\":[");
    
    for (int i = 0; i < 10; i++) {
        char doc_id[32];
        snprintf(doc_id, sizeof(doc_id), "bulk_doc_%d", i);
        
        char doc_fragment[DOCUMENT_SIZE / 5];
        snprintf(doc_fragment, sizeof(doc_fragment),
            "%s{\"operation\":\"insert\",\"id\":\"%s\",\"document\":{"
            "\"id\":\"%s\",\"name\":\"Bulk Test %d\",\"value\":%d}}",
            (i > 0 ? "," : ""), doc_id, doc_id, i, rand() % 1000);
        
        strcat(bulk_doc, doc_fragment);
    }
    
    strcat(bulk_doc, "]}");
    
    // Measure bulk insert performance
    clock_t start_time = clock();
    
    int status_code;
    char* response = make_http_request("POST", bulk_path, bulk_doc, &status_code);
    
    clock_t end_time = clock();
    double time_ms = ((double)(end_time - start_time)) / CLOCKS_PER_SEC * 1000.0;
    
    if (response && status_code == 200) {
        log_performance("Bulk Insert (10 documents)", time_ms, 10);
    } else {
        log_test("FAIL: Bulk insert test failed");
    }
    
    if (response) {
        free(response);
    }
    
    // Test bulk update
    strcpy(bulk_doc, "{\"operations\":[");
    
    for (int i = 0; i < 10; i++) {
        char doc_id[32];
        snprintf(doc_id, sizeof(doc_id), "bulk_doc_%d", i);
        
        char doc_fragment[DOCUMENT_SIZE / 5];
        snprintf(doc_fragment, sizeof(doc_fragment),
            "%s{\"operation\":\"update\",\"id\":\"%s\",\"document\":{"
            "\"id\":\"%s\",\"name\":\"Updated Bulk Test %d\",\"value\":%d,\"updated\":true}}",
            (i > 0 ? "," : ""), doc_id, doc_id, i, rand() % 1000);
        
        strcat(bulk_doc, doc_fragment);
    }
    
    strcat(bulk_doc, "]}");
    
    // Measure bulk update performance
    start_time = clock();
    
    response = make_http_request("PUT", bulk_path, bulk_doc, &status_code);
    
    end_time = clock();
    time_ms = ((double)(end_time - start_time)) / CLOCKS_PER_SEC * 1000.0;
    
    if (response && status_code == 200) {
        log_performance("Bulk Update (10 documents)", time_ms, 10);
    } else {
        log_test("FAIL: Bulk update test failed");
    }
    
    if (response) {
        free(response);
    }
    
    // Test bulk delete
    strcpy(bulk_doc, "{\"operations\":[");
    
    for (int i = 0; i < 10; i++) {
        char doc_id[32];
        snprintf(doc_id, sizeof(doc_id), "bulk_doc_%d", i);
        
        char doc_fragment[DOCUMENT_SIZE / 10];
        snprintf(doc_fragment, sizeof(doc_fragment),
            "%s{\"operation\":\"delete\",\"id\":\"%s\"}",
            (i > 0 ? "," : ""), doc_id);
        
        strcat(bulk_doc, doc_fragment);
    }
    
    strcat(bulk_doc, "]}");
    
    // Measure bulk delete performance
    start_time = clock();
    
    response = make_http_request("DELETE", bulk_path, bulk_doc, &status_code);
    
    end_time = clock();
    time_ms = ((double)(end_time - start_time)) / CLOCKS_PER_SEC * 1000.0;
    
    if (response && status_code == 200) {
        log_performance("Bulk Delete (10 documents)", time_ms, 10);
    } else {
        log_test("FAIL: Bulk delete test failed");
    }
    
    if (response) {
        free(response);
    }
    
    free(bulk_doc);
}

int main() {
    // Seed random number generator
    srand(time(NULL));
    
    test_init();
    
    log_test_f("Testing server API at %s:%d", SERVER_HOST, SERVER_PORT);
    log_test("Note: Server must be running for these tests");
    
    // Check if server is running
    int status_code;
    char* response = make_http_request("GET", "/health", NULL, &status_code);
    
    if (!response || status_code != 200) {
        log_test("FAIL: Server is not running or health check failed");
        if (response) free(response);
        test_cleanup();
        return EXIT_FAILURE;
    }
    
    free(response);
    
    // Allocate memory for document IDs
    char*** document_ids = (char***)malloc(sizeof(char**) * NUM_THREADS);
    for (int i = 0; i < NUM_THREADS; i++) {
        document_ids[i] = (char**)malloc(sizeof(char*) * REQUESTS_PER_THREAD);
        memset(document_ids[i], 0, sizeof(char*) * REQUESTS_PER_THREAD);
    }
    
    // Run the tests
    test_document_creation_performance();
    test_document_retrieval_performance(document_ids);
    test_document_update_performance(document_ids);
    test_document_deletion_performance(document_ids);
    test_bulk_operations_performance();
    
    test_cleanup();
    return EXIT_SUCCESS;
}