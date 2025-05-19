#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>

int main() {
    printf("JSONdb Socket Binding Test\n");
    printf("==========================\n");
    
    /* Test daemonization with proper socket handling */
    printf("Testing daemonization with proper socket handling:\n");
    
    /* Create socket BEFORE fork */
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        printf("Failed to create socket: %s (errno=%d)\n", strerror(errno), errno);
        return 1;
    }
    
    printf("Socket created with fd=%d\n", socket_fd);
    
    /* Set socket options */
    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        printf("Failed to set SO_REUSEADDR: %s (errno=%d)\n", strerror(errno), errno);
        close(socket_fd);
        return 1;
    }
    
    printf("Socket options set\n");
    
    /* Fork the process */
    printf("Forking process...\n");
    pid_t pid = fork();
    
    if (pid < 0) {
        printf("Failed to fork process: %s (errno=%d)\n", strerror(errno), errno);
        close(socket_fd);
        return 1;
    }
    
    if (pid > 0) {
        /* Parent process */
        printf("Parent process (PID: %d) - child PID: %d\n", getpid(), pid);
        printf("Parent process exiting\n");
        
        /* Close socket in parent */
        close(socket_fd);
        
        /* Wait briefly to allow child to start */
        sleep(1);
        
        /* Check if child process bound successfully */
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "netstat -tuln | grep 5000 || echo 'Port 5000 not found'");
        printf("Checking port 5000 status from parent:\n");
        system(cmd);
        
        return 0;
    }
    
    /* Child process continues */
    printf("Child process (PID: %d)\n", getpid());
    
    /* Verify socket is still valid */
    int socket_error = 0;
    socklen_t error_len = sizeof(socket_error);
    if (getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_len) < 0) {
        printf("Child: Failed to check socket state: %s (errno=%d)\n", strerror(errno), errno);
    } else if (socket_error != 0) {
        printf("Child: Socket has error state: %s (error=%d)\n", strerror(socket_error), socket_error);
    } else {
        printf("Child: Socket is valid after fork\n");
    }
    
    /* Prepare address struct */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(5000);
    
    printf("Child: Attempting to bind to port 5000 (socket_fd=%d)...\n", socket_fd);
    
    /* Bind socket */
    if (bind(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("Child: Failed to bind socket: %s (errno=%d)\n", strerror(errno), errno);
        close(socket_fd);
        return 1;
    }
    
    printf("Child: Bind successful\n");
    
    /* Listen for connections */
    if (listen(socket_fd, 5) < 0) {
        printf("Child: Failed to listen: %s (errno=%d)\n", strerror(errno), errno);
        close(socket_fd);
        return 1;
    }
    
    printf("Child: Listening on port 5000\n");
    printf("Child: Successfully bound and listening on port 5000.\n");
    
    /* Run netstat to confirm */
    system("netstat -tuln | grep 5000");
    
    /* Wait for a connection */
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    printf("Child: Waiting for connections...\n");
    
    /* Create a log file */
    FILE* log_file = fopen("/tmp/jsondb_socket_test.log", "w");
    if (log_file) {
        fprintf(log_file, "Child process (PID: %d) waiting for connections on port 5000\n", getpid());
        fflush(log_file);
    }
    
    /* Set timeout for accept */
    fd_set read_fds;
    struct timeval tv;
    tv.tv_sec = 10;  /* 10 second timeout */
    tv.tv_usec = 0;
    
    FD_ZERO(&read_fds);
    FD_SET(socket_fd, &read_fds);
    
    /* Wait for connection with timeout */
    int select_result = select(socket_fd + 1, &read_fds, NULL, NULL, &tv);
    
    if (select_result < 0) {
        printf("Child: Select failed: %s (errno=%d)\n", strerror(errno), errno);
        if (log_file) {
            fprintf(log_file, "Select failed: %s (errno=%d)\n", strerror(errno), errno);
            fflush(log_file);
        }
    } else if (select_result == 0) {
        printf("Child: Timeout waiting for connections\n");
        if (log_file) {
            fprintf(log_file, "Timeout waiting for connections\n");
            fflush(log_file);
        }
    } else {
        /* Connection available */
        int client_fd = accept(socket_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            printf("Child: Failed to accept connection: %s (errno=%d)\n", strerror(errno), errno);
            if (log_file) {
                fprintf(log_file, "Failed to accept connection: %s (errno=%d)\n", strerror(errno), errno);
                fflush(log_file);
            }
        } else {
            printf("Child: Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            if (log_file) {
                fprintf(log_file, "Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                fflush(log_file);
            }
            
            /* Simple HTTP response */
            char buffer[8192] = {0};
            ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
            
            if (bytes_read > 0) {
                printf("Child: Received request:\n%s\n", buffer);
                if (log_file) {
                    fprintf(log_file, "Received request:\n%s\n", buffer);
                    fflush(log_file);
                }
                
                char response[] = "HTTP/1.1 200 OK\r\n"
                                  "Content-Type: application/json\r\n"
                                  "Connection: close\r\n"
                                  "\r\n"
                                  "{\"status\":\"success\",\"message\":\"JSONdb socket test is working\"}\r\n";
                
                write(client_fd, response, strlen(response));
                printf("Child: Sent HTTP response\n");
                if (log_file) {
                    fprintf(log_file, "Sent HTTP response\n");
                    fflush(log_file);
                }
            }
            
            close(client_fd);
        }
    }
    
    if (log_file) {
        fprintf(log_file, "Test completed, closing socket\n");
        fclose(log_file);
    }
    
    /* All done */
    close(socket_fd);
    return 0;
}