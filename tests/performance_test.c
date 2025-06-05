#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#define PORT 5000
#define ITERATIONS 100

double time_diff_ms(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000.0 + 
           (end.tv_nsec - start.tv_nsec) / 1000000.0;
}

int main() {
    int sock;
    struct sockaddr_in serv_addr;
    char request[] = "GET /api/health HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n";
    char buffer[4096];
    struct timespec start, end;
    double times[ITERATIONS];
    double total = 0, min_time = 9999, max_time = 0;
    
    /* Create socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }
    
    /* Disable Nagle's algorithm for lower latency */
    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
    
    /* Connect to server */
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        return 1;
    }
    
    /* Warm up */
    for (int i = 0; i < 10; i++) {
        send(sock, request, strlen(request), 0);
        recv(sock, buffer, sizeof(buffer), 0);
    }
    
    /* Benchmark */
    printf("Running %d iterations...\n", ITERATIONS);
    for (int i = 0; i < ITERATIONS; i++) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        /* Send request */
        send(sock, request, strlen(request), 0);
        
        /* Receive response */
        int n = recv(sock, buffer, sizeof(buffer), 0);
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        if (n <= 0) {
            printf("Connection error at iteration %d\n", i);
            break;
        }
        
        times[i] = time_diff_ms(start, end);
        total += times[i];
        if (times[i] < min_time) min_time = times[i];
        if (times[i] > max_time) max_time = times[i];
        
        /* Small delay to avoid overwhelming */
        usleep(10000); /* 10ms */
    }
    
    close(sock);
    
    /* Calculate percentiles */
    double p50 = times[ITERATIONS/2];
    double p95 = times[(int)(ITERATIONS * 0.95)];
    double p99 = times[(int)(ITERATIONS * 0.99)];
    
    printf("\n=== Performance Results ===\n");
    printf("Average: %.3f ms\n", total / ITERATIONS);
    printf("Min:     %.3f ms\n", min_time);
    printf("Max:     %.3f ms\n", max_time);
    printf("P50:     %.3f ms\n", p50);
    printf("P95:     %.3f ms\n", p95);
    printf("P99:     %.3f ms\n", p99);
    
    if (total / ITERATIONS < 1.0) {
        printf("\n✅ SUB-MILLISECOND ACHIEVED!\n");
    } else {
        printf("\n❌ Target: <1ms (currently %.1fx slower)\n", (total / ITERATIONS));
    }
    
    return 0;
}