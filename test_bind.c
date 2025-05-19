#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>

int main() {
    /* Create socket */
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    /* Set socket options */
    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(socket_fd);
        return 1;
    }

    /* Create address structure */
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; /* Bind to all interfaces */
    address.sin_port = htons(8080);       /* Port 8080 */

    /* Bind socket */
    if (bind(socket_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Socket bind failed");
        printf("Error binding to port 5000: %s\n", strerror(errno));
        close(socket_fd);
        return 1;
    }

    /* Listen for connections */
    if (listen(socket_fd, 5) < 0) {
        perror("Socket listen failed");
        printf("Error listening on port 5000: %s\n", strerror(errno));
        close(socket_fd);
        return 1;
    }

    printf("Successfully bound to port 5000 and listening for connections\n");
    printf("Press Enter to exit...\n");
    getchar();

    /* Close socket */
    close(socket_fd);
    return 0;
}