#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>

int main() {
    printf("Socket client test\n");
    
    // Create socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("Error creating socket: %s (errno=%d)\n", strerror(errno), errno);
        return 1;
    }
    printf("Socket created successfully, fd=%d\n", sockfd);
    
    // Prepare server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // localhost
    server_addr.sin_port = htons(5000);
    
    // Connect to server
    printf("Attempting to connect to 127.0.0.1:5000...\n");
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Error connecting to server: %s (errno=%d)\n", strerror(errno), errno);
        close(sockfd);
        return 1;
    }
    
    printf("Connected to server successfully!\n");
    
    // Send a simple HTTP request
    const char *request = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    if (write(sockfd, request, strlen(request)) < 0) {
        printf("Error sending request: %s (errno=%d)\n", strerror(errno), errno);
        close(sockfd);
        return 1;
    }
    
    printf("Request sent successfully\n");
    
    // Read response
    char buffer[1024] = {0};
    int bytes_read = read(sockfd, buffer, sizeof(buffer) - 1);
    if (bytes_read < 0) {
        printf("Error reading response: %s (errno=%d)\n", strerror(errno), errno);
        close(sockfd);
        return 1;
    }
    
    if (bytes_read == 0) {
        printf("Server closed the connection without sending data\n");
    } else {
        printf("Received %d bytes from server:\n%s\n", bytes_read, buffer);
    }
    
    // Clean up
    close(sockfd);
    return 0;
}