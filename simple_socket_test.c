#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>

int main() {
    printf("Simple socket binding test\n");
    
    int port = 5000;
    
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

    // Prepare address structure
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;  // bind to all interfaces
    addr.sin_port = htons(port);
    
    // Bind socket
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("Error binding socket to port %d: %s (errno=%d)\n", port, strerror(errno), errno);
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
    
    // Show successful bind
    printf("Socket is successfully listening on port %d\n", port);
    printf("Press Enter to exit...\n");
    getchar();
    
    // Clean up
    close(sockfd);
    return 0;
}