#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

/* Thread function prototype */
void* accept_thread(void* arg);

int main() {
    printf("=== MINIMAL SERVER TEST ===\n");
    
    /* Create socket */
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        printf("Failed to create socket: %s (errno=%d)\n", strerror(errno), errno);
        return 1;
    }
    
    printf("Created socket fd=%d in PID %d\n", socket_fd, getpid());
    
    /* Set socket options */
    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        printf("Failed to set SO_REUSEADDR: %s (errno=%d)\n", strerror(errno), errno);
        close(socket_fd);
        return 1;
    }
    
    /* Prepare address structure */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9000);  /* Use port 9000 for testing */
    
    printf("Attempting to bind socket %d to port 9000 in PID %d\n", socket_fd, getpid());
    
    /* Bind socket */
    if (bind(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("Failed to bind socket: %s (errno=%d)\n", strerror(errno), errno);
        close(socket_fd);
        return 1;
    }
    
    printf("Successfully bound socket to port 9000\n");
    
    /* Listen for connections */
    if (listen(socket_fd, 5) < 0) {
        printf("Failed to listen on socket: %s (errno=%d)\n", strerror(errno), errno);
        close(socket_fd);
        return 1;
    }
    
    printf("Socket is now listening on port 9000\n");
    
    /* Create a thread to accept connections */
    pthread_t thread;
    if (pthread_create(&thread, NULL, accept_thread, &socket_fd) != 0) {
        printf("Failed to create thread: %s (errno=%d)\n", strerror(errno), errno);
        close(socket_fd);
        return 1;
    }
    
    printf("Created accept thread successfully\n");
    
    /* Wait for the thread to exit (it won't, but this keeps the main thread alive) */
    pthread_join(thread, NULL);
    
    /* Cleanup and exit */
    close(socket_fd);
    return 0;
}

/* Thread function to accept connections */
void* accept_thread(void* arg) {
    int socket_fd = *(int*)arg;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    printf("Accept thread started with socket_fd=%d in thread ID %lu\n", 
           socket_fd, (unsigned long)pthread_self());
    
    /* Keep accepting connections forever */
    while (1) {
        printf("Waiting for connections on port 9000...\n");
        
        int client_fd = accept(socket_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            printf("Failed to accept connection: %s (errno=%d)\n", strerror(errno), errno);
            continue;
        }
        
        printf("Accepted connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), 
               ntohs(client_addr.sin_port));
        
        /* Send a simple HTTP response */
        const char* response = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"status\":\"success\",\"message\":\"Minimal server test is working\"}\r\n";
        
        write(client_fd, response, strlen(response));
        close(client_fd);
    }
    
    return NULL;
}