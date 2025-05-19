#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {
    int port = 5000;
    if (argc > 1) port = atoi(argv[1]);
    
    /* Create socket */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("Socket creation failed: %s (errno=%d)\n", strerror(errno), errno);
        return 1;
    }
    printf("Socket created successfully (fd=%d)\n", sockfd);
    
    /* Set socket options */
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        printf("Failed to set SO_REUSEADDR: %s (errno=%d)\n", strerror(errno), errno);
    } else {
        printf("Set SO_REUSEADDR successfully\n");
    }
    
    /* Create address structure */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    /* Bind socket */
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("Bind failed: %s (errno=%d)\n", strerror(errno), errno);
        close(sockfd);
        return 1;
    }
    printf("Socket bound successfully to port %d\n", port);
    
    /* Listen */
    if (listen(sockfd, 5) < 0) {
        printf("Listen failed: %s (errno=%d)\n", strerror(errno), errno);
        close(sockfd);
        return 1;
    }
    printf("Socket listening on port %d\n", port);
    
    /* Check socket status */
    int status = 0;
    socklen_t len = sizeof(status);
    if (getsockopt(sockfd, SOL_SOCKET, SO_ACCEPTCONN, &status, &len) < 0) {
        printf("Failed to check socket status: %s (errno=%d)\n", strerror(errno), errno);
    } else {
        printf("Socket listening state: %s\n", status ? "LISTENING" : "NOT LISTENING");
    }
    
    /* Accept connections */
    printf("Waiting for connections... (will exit after first connection)\n");
    
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    /* Set up a timeout using select */
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);
    
    struct timeval tv;
    tv.tv_sec = 30;  /* 30 second timeout */
    tv.tv_usec = 0;
    
    int result = select(sockfd + 1, &read_fds, NULL, NULL, &tv);
    
    if (result < 0) {
        printf("Select failed: %s (errno=%d)\n", strerror(errno), errno);
    } else if (result == 0) {
        printf("Timeout waiting for connection\n");
    } else {
        int client_fd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            printf("Accept failed: %s (errno=%d)\n", strerror(errno), errno);
        } else {
            printf("Accepted connection from %s:%d\n", 
                  inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            
            char response[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 14\r\n\r\nSocket Monitor\r\n";
            write(client_fd, response, strlen(response));
            
            close(client_fd);
        }
    }
    
    printf("Closing socket\n");
    close(sockfd);
    
    return 0;
}
