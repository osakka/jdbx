#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    int port = 5002;
    
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    printf("Testing socket binding on port %d\n", port);
    
    // Create socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("Failed to create socket");
        return 1;
    }
    
    printf("Socket created successfully (fd=%d)\n", socket_fd);
    
    // Set socket options
    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Failed to set SO_REUSEADDR");
        close(socket_fd);
        return 1;
    }
    
    printf("Socket options set successfully\n");
    
    // Create address structure
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    printf("Address structure created\n");
    
    // Bind socket
    int bind_result = bind(socket_fd, (struct sockaddr*)&address, sizeof(address));
    if (bind_result < 0) {
        printf("Failed to bind socket to port %d: %s (errno=%d)\n", 
               port, strerror(errno), errno);
        close(socket_fd);
        return 1;
    }
    
    printf("Socket bound successfully to port %d\n", port);
    
    // Listen on socket
    int listen_result = listen(socket_fd, 5);
    if (listen_result < 0) {
        printf("Failed to listen on socket: %s (errno=%d)\n", 
               strerror(errno), errno);
        close(socket_fd);
        return 1;
    }
    
    printf("Socket listening successfully\n");
    
    // Verify listening state
    int acceptconn = 0;
    socklen_t acceptconn_len = sizeof(acceptconn);
    if (getsockopt(socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &acceptconn, &acceptconn_len) < 0) {
        printf("Failed to check SO_ACCEPTCONN: %s (errno=%d)\n", 
               strerror(errno), errno);
    } else {
        printf("Socket listening state: %s\n", 
               acceptconn ? "LISTENING" : "NOT LISTENING");
    }
    
    // Run netstat to verify port is visible
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "netstat -tuln | grep %d || echo 'Port %d NOT FOUND in netstat'", 
            port, port);
    printf("\nRunning: %s\n", cmd);
    system(cmd);
    
    // Wait for user input before exiting
    printf("\nPress Enter to close socket and exit...\n");
    getchar();
    
    // Clean up
    close(socket_fd);
    printf("Socket closed\n");
    
    return 0;
}