#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>

int main(int argc, char* argv[]) {
    printf("Socket binding test program starting\n");
    
    // Parse port number
    int port = 8080;
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    printf("Using port: %d\n", port);
    
    // Create socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("Error creating socket: %s (errno=%d)\n", strerror(errno), errno);
        return 1;
    }
    printf("Socket created successfully, fd=%d\n", sockfd);
    
    // Set socket options
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        printf("Error setting socket options (SO_REUSEADDR): %s (errno=%d)\n", strerror(errno), errno);
        close(sockfd);
        return 1;
    }
    printf("Socket option SO_REUSEADDR set successfully\n");

#ifdef SO_REUSEPORT
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        printf("Warning: Failed to set SO_REUSEPORT: %s (errno=%d)\n", strerror(errno), errno);
    } else {
        printf("Socket option SO_REUSEPORT set successfully\n");
    }
#else
    printf("SO_REUSEPORT not defined on this system\n");
#endif
    
    // Prepare address structure
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;  // bind to all interfaces
    addr.sin_port = htons(port);
    
    printf("Address structure prepared:\n");
    printf("  Family: %d\n", addr.sin_family);
    printf("  Port: %d (network byte order: %d)\n", port, addr.sin_port);
    printf("  Address: INADDR_ANY (%s)\n", inet_ntoa(addr.sin_addr));
    
    // Bind socket
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("Error binding socket: %s (errno=%d)\n", strerror(errno), errno);
        close(sockfd);
        return 1;
    }
    printf("Socket bound successfully to port %d\n", port);
    
    // Listen on socket
    if (listen(sockfd, 10) < 0) {
        printf("Error listening on socket: %s (errno=%d)\n", strerror(errno), errno);
        close(sockfd);
        return 1;
    }
    printf("Socket now listening on port %d\n", port);
    
    // Check socket state
    int socket_error = 0;
    socklen_t error_len = sizeof(socket_error);
    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &socket_error, &error_len) < 0) {
        printf("Error checking socket state: %s (errno=%d)\n", strerror(errno), errno);
    } else if (socket_error != 0) {
        printf("Socket has error state: %s (errno=%d)\n", strerror(socket_error), socket_error);
    } else {
        printf("Socket has no errors\n");
    }
    
    // Check if socket is in listening state
    int socket_status = 0;
    socklen_t status_len = sizeof(socket_status);
    if (getsockopt(sockfd, SOL_SOCKET, SO_ACCEPTCONN, &socket_status, &status_len) < 0) {
        printf("Error checking if socket is in listening state: %s (errno=%d)\n", strerror(errno), errno);
    } else if (socket_status == 0) {
        printf("Socket is NOT in listening state\n");
    } else {
        printf("Socket is in listening state\n");
    }
    
    // Try to run netstat
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "netstat -tuln | grep %d || ss -tuln | grep %d", port, port);
    printf("Running command: %s\n", cmd);
    system(cmd);
    
    // Accept connections
    printf("\nWaiting for connections on port %d...\n", port);
    printf("Press Ctrl+C to exit\n\n");
    
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    while (1) {
        // Accept a connection
        int client_fd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            printf("Error accepting connection: %s (errno=%d)\n", strerror(errno), errno);
            sleep(1);
            continue;
        }
        
        printf("Connection accepted from %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        // Send a simple response
        const char *response = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\nContent-Type: text/plain\r\n\r\nHello, World!";
        write(client_fd, response, strlen(response));
        
        // Close the connection
        close(client_fd);
    }
    
    // Clean up (will never reach here)
    close(sockfd);
    
    return 0;
}