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
        fprintf(stderr, "Socket creation failed: %s\n", strerror(errno));
        return 1;
    }
    
    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        fprintf(stderr, "Failed to set SO_REUSEADDR: %s\n", strerror(errno));
        close(socket_fd);
        return 1;
    }
    
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(5000);
    
    int bind_result = bind(socket_fd, (struct sockaddr*)&address, sizeof(address));
    if (bind_result < 0) {
        fprintf(stderr, "Failed to bind socket to port 5000: %s\n", strerror(errno));
        close(socket_fd);
        return 1;
    }
    
    if (listen(socket_fd, 5) < 0) {
        fprintf(stderr, "Failed to listen on socket: %s\n", strerror(errno));
        close(socket_fd);
        return 1;
    }
    
    printf("Socket bound to port 5000 and listening\n");
    system("netstat -tuln  < /dev/null |  grep :5000");
    
    printf("Press Enter to exit\n");
    getchar();
    
    close(socket_fd);
    return 0;
}
