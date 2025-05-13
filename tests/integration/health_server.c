#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/sysinfo.h>

#define PORT 8080
#define BUFFER_SIZE 8192

/* Server start time */
static time_t server_start_time = 0;

/* Get system uptime in seconds */
time_t get_uptime() {
    if (server_start_time == 0) {
        return 0;
    }
    
    return time(NULL) - server_start_time;
}

/* Get system load average */
double get_load_average() {
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        return -1.0;
    }
    
    /* Convert load average to double */
    return (double)info.loads[0] / 65536.0;
}

/* Get memory usage information */
void get_memory_info(unsigned long *total, unsigned long *free, unsigned long *used) {
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        *total = 0;
        *free = 0;
        *used = 0;
        return;
    }
    
    *total = info.totalram * info.mem_unit;
    *free = info.freeram * info.mem_unit;
    *used = *total - *free;
}

/* Get process memory usage (RSS) */
unsigned long get_process_memory() {
    FILE *f;
    unsigned long rss = 0;
    char buf[256];
    
    /* Open /proc/self/statm for reading process memory stats */
    f = fopen("/proc/self/statm", "r");
    if (!f) {
        return 0;
    }
    
    /* Format is: size resident shared text lib data dt */
    if (fgets(buf, sizeof(buf), f)) {
        unsigned long size, resident;
        if (sscanf(buf, "%lu %lu", &size, &resident) == 2) {
            /* Convert to KB - page size is typically 4KB */
            rss = resident * 4;
        }
    }
    
    fclose(f);
    return rss;
}

/* Generate health check response */
void generate_health_response(char *response, size_t max_size) {
    /* Add HTTP headers */
    int offset = snprintf(response, max_size,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Connection: close\r\n"
        "\r\n");
    
    /* Get uptime */
    time_t uptime = get_uptime();
    int days = uptime / 86400;
    int hours = (uptime % 86400) / 3600;
    int minutes = (uptime % 3600) / 60;
    int seconds = uptime % 60;
    
    /* Format uptime string */
    char uptime_str[64];
    snprintf(uptime_str, sizeof(uptime_str), "%dd %dh %dm %ds", days, hours, minutes, seconds);
    
    /* Get memory info */
    unsigned long total_mem, free_mem, used_mem;
    get_memory_info(&total_mem, &free_mem, &used_mem);
    
    /* Get process memory */
    unsigned long process_mem = get_process_memory();
    
    /* Get load average */
    double load = get_load_average();
    
    /* Build JSON response */
    offset += snprintf(response + offset, max_size - offset,
        "{\n"
        "  \"status\": \"ok\",\n"
        "  \"timestamp\": %ld,\n"
        "  \"uptime_seconds\": %ld,\n"
        "  \"uptime\": \"%s\",\n"
        "  \"load_average\": %.2f,\n"
        "  \"memory\": {\n"
        "    \"total_kb\": %lu,\n"
        "    \"free_kb\": %lu,\n"
        "    \"used_kb\": %lu,\n"
        "    \"process_kb\": %lu\n"
        "  },\n"
        "  \"metrics\": {\n"
        "    \"available\": true\n"
        "  }\n"
        "}",
        (long)time(NULL),
        (long)uptime,
        uptime_str,
        load,
        total_mem / 1024,
        free_mem / 1024,
        used_mem / 1024,
        process_mem
    );
}

/* Generate metrics response */
void generate_metrics_response(char *response, size_t max_size) {
    /* Add HTTP headers */
    int offset = snprintf(response, max_size,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Connection: close\r\n"
        "\r\n");
    
    /* Build JSON response */
    offset += snprintf(response + offset, max_size - offset,
        "{\n"
        "  \"metrics\": {\n"
        "    \"server_requests_total\": 1,\n"
        "    \"server_request_duration_seconds\": 0.001,\n"
        "    \"active_connections\": 1\n"
        "  }\n"
        "}"
    );
}

/* Generate metrics available response */
void generate_metrics_available_response(char *response, size_t max_size) {
    /* Add HTTP headers */
    int offset = snprintf(response, max_size,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Connection: close\r\n"
        "\r\n");
    
    /* Build JSON response */
    offset += snprintf(response + offset, max_size - offset,
        "{\n"
        "  \"metrics\": [\n"
        "    \"server_requests_total\",\n"
        "    \"server_request_duration_seconds\",\n"
        "    \"db_operations_total\",\n"
        "    \"db_operation_duration_seconds\",\n"
        "    \"active_connections\",\n"
        "    \"collection_documents_total\",\n"
        "    \"document_size_bytes\",\n"
        "    \"api_errors_total\",\n"
        "    \"system_memory_bytes\",\n"
        "    \"cache_size_bytes\",\n"
        "    \"cache_hits_total\",\n"
        "    \"cache_misses_total\"\n"
        "  ]\n"
        "}"
    );
}

/* Handle HTTP request */
void handle_request(int client_socket, const char *request) {
    char response[BUFFER_SIZE] = {0};
    
    /* Check request path */
    if (strstr(request, "GET /health")) {
        generate_health_response(response, sizeof(response));
    } else if (strstr(request, "GET /metrics")) {
        if (strstr(request, "GET /metrics/available")) {
            generate_metrics_available_response(response, sizeof(response));
        } else {
            generate_metrics_response(response, sizeof(response));
        }
    } else {
        /* Default response for unknown paths */
        snprintf(response, sizeof(response),
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: application/json\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"error\":\"Not found\"}"
        );
    }
    
    /* Send response */
    write(client_socket, response, strlen(response));
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};
    
    /* Initialize server start time */
    server_start_time = time(NULL);
    
    /* Create socket */
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    /* Set socket options */
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }
    
    /* Set address */
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    /* Bind socket */
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    
    /* Listen for connections */
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Health monitoring server running on port %d\n", PORT);
    printf("Endpoints available:\n");
    printf("  - /health - Health check endpoint\n");
    printf("  - /metrics - Metrics endpoint\n");
    printf("  - /metrics/available - Available metrics endpoint\n");
    
    /* Accept and handle connections */
    while (1) {
        printf("Waiting for connection...\n");
        
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }
        
        /* Read request */
        read(client_socket, buffer, BUFFER_SIZE);
        printf("Request: %.100s\n", buffer);
        
        /* Handle request */
        handle_request(client_socket, buffer);
        
        /* Close connection */
        close(client_socket);
        
        /* Clear buffer */
        memset(buffer, 0, BUFFER_SIZE);
    }
    
    return 0;
}