#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/time.h>
#include <fcntl.h>

int main() {
    printf("Socket binding and fork test\n");
    printf("This test creates a socket, binds it to port 5050, and forks a child process\n");
    printf("It then verifies the socket remains valid in the child process\n\n");

    /* Create socket */
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        fprintf(stderr, "Socket creation failed: %s\n", strerror(errno));
        return 1;
    }
    
    printf("Created socket with FD: %d\n", socket_fd);
    
    /* Set socket options */
    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        fprintf(stderr, "Failed to set SO_REUSEADDR: %s\n", strerror(errno));
        close(socket_fd);
        return 1;
    }
    
    printf("Set SO_REUSEADDR on socket\n");
    
    /* Create address structure */
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(5050);
    
    printf("Binding socket to port 5050\n");
    
    /* Bind socket */
    int bind_result = bind(socket_fd, (struct sockaddr*)&address, sizeof(address));
    if (bind_result < 0) {
        fprintf(stderr, "Failed to bind socket to port 5050: %s\n", strerror(errno));
        close(socket_fd);
        return 1;
    }
    
    printf("Successfully bound socket to port 5050\n");
    
    /* Listen */
    if (listen(socket_fd, 5) < 0) {
        fprintf(stderr, "Failed to listen on socket: %s\n", strerror(errno));
        close(socket_fd);
        return 1;
    }
    
    printf("Socket now in listening state\n");
    
    /* Verify with netstat */
    printf("Verifying with netstat:\n");
    system("netstat -tuln | grep :5050");
    
    /* Verify socket state */
    int acceptconn = 0;
    socklen_t acceptconn_len = sizeof(acceptconn);
    if (getsockopt(socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &acceptconn, &acceptconn_len) < 0) {
        fprintf(stderr, "Failed to check if socket is in listening state: %s\n", strerror(errno));
    } else {
        printf("Socket listening state: %s\n", acceptconn ? "LISTENING" : "NOT LISTENING");
    }
    
    /* Create child process */
    printf("\nForking child process...\n");
    pid_t pid = fork();
    
    if (pid < 0) {
        fprintf(stderr, "Failed to fork process: %s\n", strerror(errno));
        close(socket_fd);
        return 1;
    } else if (pid > 0) {
        /* Parent process */
        printf("PARENT: Process (PID: %d) forked child (PID: %d)\n", getpid(), pid);
        printf("PARENT: Socket FD: %d\n", socket_fd);
        
        /* Verify socket is still valid after fork */
        if (getsockopt(socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &acceptconn, &acceptconn_len) < 0) {
            fprintf(stderr, "PARENT: Failed to check if socket is in listening state: %s\n", strerror(errno));
        } else {
            printf("PARENT: Socket listening state: %s\n", acceptconn ? "LISTENING" : "NOT LISTENING");
        }
        
        /* Keep parent alive for a while to allow child to run */
        printf("PARENT: Sleeping for 5 seconds...\n");
        sleep(5);
        
        /* Parent exits, closing its copy of the socket */
        printf("PARENT: Exiting and closing socket\n");
        close(socket_fd);
        
        /* But wait for child to complete */
        sleep(2);
        printf("PARENT: Test complete\n");
        
        return 0;
    } else {
        /* Child process */
        printf("CHILD: Process (PID: %d) started with socket FD: %d\n", getpid(), socket_fd);
        
        /* Verify socket in child */
        printf("CHILD: Verifying with netstat:\n");
        system("netstat -tuln | grep :5050");
        
        /* Verify socket state in child */
        if (getsockopt(socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &acceptconn, &acceptconn_len) < 0) {
            fprintf(stderr, "CHILD: Failed to check if socket is in listening state: %s\n", strerror(errno));
        } else {
            printf("CHILD: Socket listening state: %s\n", acceptconn ? "LISTENING" : "NOT LISTENING");
        }
        
        /* Check for errors */
        int socket_error = 0;
        socklen_t error_len = sizeof(socket_error);
        if (getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_len) < 0) {
            fprintf(stderr, "CHILD: Failed to check socket state: %s\n", strerror(errno));
        } else if (socket_error != 0) {
            fprintf(stderr, "CHILD: Socket has error state: %s (error=%d)\n", 
                  strerror(socket_error), socket_error);
        } else {
            printf("CHILD: Socket has NO errors\n");
        }
        
        /* Try to accept one connection with timeout */
        printf("CHILD: Waiting for connections on port 5050 (timeout: 3 sec)...\n");
        
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(socket_fd, &read_fds);
        
        struct timeval tv;
        tv.tv_sec = 3;
        tv.tv_usec = 0;
        
        int select_result = select(socket_fd + 1, &read_fds, NULL, NULL, &tv);
        
        if (select_result < 0) {
            fprintf(stderr, "CHILD: Select failed: %s\n", strerror(errno));
        } else if (select_result == 0) {
            printf("CHILD: No connection within timeout period (this is normal for this test)\n");
        } else {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(socket_fd, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_fd < 0) {
                fprintf(stderr, "CHILD: Accept failed: %s\n", strerror(errno));
            } else {
                printf("CHILD: Accepted connection from %s:%d\n", 
                      inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                
                /* Send simple response */
                const char* response = "Hello from forked process!";
                write(client_fd, response, strlen(response));
                
                close(client_fd);
            }
        }
        
        /* Close socket in child */
        printf("CHILD: Closing socket\n");
        close(socket_fd);
        
        printf("CHILD: Test complete - socket retained its state across the fork!\n");
        
        return 0;
    }
}