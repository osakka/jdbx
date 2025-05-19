#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    int port = 5000;
    
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    printf("Minimal socket binding test on port %d\n", port);
    
    /* Create socket */
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        fprintf(stderr, "Failed to create socket: %s (errno=%d)\n", 
                strerror(errno), errno);
        return 1;
    }
    printf("Socket created successfully (fd=%d)\n", socket_fd);
    
    /* Set socket options */
    int reuse = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        fprintf(stderr, "Failed to set socket options: %s (errno=%d)\n", 
                strerror(errno), errno);
        close(socket_fd);
        return 1;
    }
    printf("Socket option SO_REUSEADDR set successfully\n");
    
    /* Prepare the address structure */
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;
    printf("Using INADDR_ANY (0.0.0.0) for binding\n");
    
    /* Bind socket */
    printf("Binding socket to 0.0.0.0:%d...\n", port);
    if (bind(socket_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        fprintf(stderr, "Failed to bind socket: %s (errno=%d)\n", 
                strerror(errno), errno);
        close(socket_fd);
        return 1;
    }
    printf("Socket bound successfully\n");
    
    /* Listen */
    printf("Setting socket to listen state...\n");
    if (listen(socket_fd, 10) < 0) {
        fprintf(stderr, "Failed to listen on socket: %s (errno=%d)\n", 
                strerror(errno), errno);
        close(socket_fd);
        return 1;
    }
    printf("Socket listening successfully\n");
    
    /* Verify socket state */
    int acceptconn = 0;
    socklen_t acceptconn_len = sizeof(acceptconn);
    if (getsockopt(socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &acceptconn, &acceptconn_len) < 0) {
        fprintf(stderr, "Failed to check SO_ACCEPTCONN: %s (errno=%d)\n", 
                strerror(errno), errno);
    } else {
        printf("Socket listening state: %s\n", 
               acceptconn ? "LISTENING" : "NOT LISTENING");
        
        if (!acceptconn) {
            fprintf(stderr, "Socket is not in listening state despite successful listen() call\n");
            close(socket_fd);
            return 1;
        }
    }
    
    printf("Socket initialization complete (socket_fd=%d, port=%d)\n", socket_fd, port);
    printf("\nSocket test successful! Socket is properly bound and listening.\n");
    
    /* Close socket and exit */
    close(socket_fd);
    return 0;
}