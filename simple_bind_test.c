#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>

int main() {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        printf("Failed to create socket: %s (errno=%d)\n", strerror(errno), errno);
        return 1;
    }
    
    printf("Socket created with fd=%d\n", socket_fd);
    
    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        printf("Failed to set SO_REUSEADDR: %s (errno=%d)\n", strerror(errno), errno);
        close(socket_fd);
        return 1;
    }
    
    printf("Socket options set\n");
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(5000);
    
    printf("Attempting to bind to port 5000...\n");
    
    if (bind(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("Failed to bind socket: %s (errno=%d)\n", strerror(errno), errno);
        close(socket_fd);
        return 1;
    }
    
    printf("Bind successful\n");
    
    if (listen(socket_fd, 5) < 0) {
        printf("Failed to listen: %s (errno=%d)\n", strerror(errno), errno);
        close(socket_fd);
        return 1;
    }
    
    printf("Listening on port 5000\n");
    printf("Successfully bound and listening on port 5000. Press Ctrl+C to exit\n");
    
    /* Run netstat to confirm */
    system("netstat -tuln | grep 5000");
    
    /* Wait for a connection */
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    printf("Waiting for connections...\n");
    
    int client_fd = accept(socket_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        printf("Failed to accept connection: %s (errno=%d)\n", strerror(errno), errno);
    } else {
        printf("Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        close(client_fd);
    }
    
    close(socket_fd);
    return 0;
}