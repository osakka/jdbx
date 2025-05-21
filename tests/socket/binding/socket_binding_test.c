#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>

#define DEFAULT_PORT 5000
#define DEFAULT_HOST "0.0.0.0"
#define BACKLOG 10

int create_socket_and_bind(const char *host, int port) {
    int server_fd;
    struct addrinfo hints, *res, *p;
    int yes = 1;
    char port_str[6];
    int rv;

    // Convert port to string
    snprintf(port_str, sizeof(port_str), "%d", port);

    // Clear hints structure
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     // Use IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;     // Fill in IP for me

    // Get address info for the host
    if ((rv = getaddrinfo(host, port_str, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rv));
        return -1;
    }

    // Loop through results and bind to first available
    for (p = res; p != NULL; p = p->ai_next) {
        // Create socket
        server_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (server_fd == -1) {
            perror("socket");
            continue;
        }

        // Set socket options
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
            perror("setsockopt");
            close(server_fd);
            continue;
        }

        // Bind socket
        if (bind(server_fd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("bind");
            close(server_fd);
            continue;
        }

        // If we got here, we successfully bound
        break;
    }

    // No address worked
    if (p == NULL) {
        fprintf(stderr, "Failed to bind to any address\n");
        freeaddrinfo(res);
        return -1;
    }

    // Store the successful binding information
    char ipstr[INET6_ADDRSTRLEN];
    void *addr;
    
    if (p->ai_family == AF_INET) { // IPv4
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
        addr = &(ipv4->sin_addr);
    } else { // IPv6
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
        addr = &(ipv6->sin6_addr);
    }
    
    // Convert IP to string
    inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
    printf("Successfully bound to %s:%d\n", ipstr, port);

    freeaddrinfo(res);
    return server_fd;
}

int main(int argc, char *argv[]) {
    const char *host = DEFAULT_HOST;
    int port = DEFAULT_PORT;
    int server_fd;

    // Parse arguments
    if (argc > 1) {
        host = argv[1];
    }
    if (argc > 2) {
        port = atoi(argv[2]);
    }

    printf("Starting socket binding test on %s:%d\n", host, port);

    // Create and bind socket
    server_fd = create_socket_and_bind(host, port);
    if (server_fd < 0) {
        fprintf(stderr, "Failed to create and bind socket\n");
        return 1;
    }

    // Start listening
    if (listen(server_fd, BACKLOG) == -1) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Socket bound and listening on %s:%d (server_fd: %d)\n", host, port, server_fd);
    printf("Press Ctrl+C to exit...\n");
    
    // Keep the program running
    while (1) {
        sleep(1);
    }

    close(server_fd);
    return 0;
}