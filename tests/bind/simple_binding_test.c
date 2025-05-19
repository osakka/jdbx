#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

int main(int argc, char *argv[]) {
    int port = 5000;
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    printf("Testing socket binding on port %d\n", port);
    
    // Create socket
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("Socket creation failed");
        return 1;
    }
    printf("Socket created with fd=%d\n", sock_fd);
    
    // Set socket options
    int opt = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(sock_fd);
        return 1;
    }
    printf("Set SO_REUSEADDR successfully\n");
    
    // Create address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    // Bind
    printf("Binding to port %d...\n", port);
    if (bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        close(sock_fd);
        return 1;
    }
    printf("Bind successful\n");
    
    // Listen
    printf("Setting socket to listen state...\n");
    if (listen(sock_fd, 5) < 0) {
        perror("Listen failed");
        close(sock_fd);
        return 1;
    }
    printf("Listen successful\n");
    
    // Run netstat to verify
    printf("Verifying port binding with netstat:\n");
    system("netstat -tuln | grep LISTEN | grep -w 5000 || echo 'Port 5000 not found in netstat'");
    
    // Accept connections in a loop
    printf("Waiting for connections on port %d (press Ctrl+C to exit)...\n", port);
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(sock_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("Accept failed");
            continue;
        }
        
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        printf("Connection from %s:%d\n", client_ip, ntohs(client_addr.sin_port));
        
        // Send a simple response
        const char *response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 16\r\n\r\nBinding test OK\r\n";
        send(client_fd, response, strlen(response), 0);
        
        close(client_fd);
    }
    
    // Cleanup
    close(sock_fd);
    return 0;
}