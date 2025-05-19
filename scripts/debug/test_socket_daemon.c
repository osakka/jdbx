#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

// Global socket descriptor
int g_socket_fd = -1;

// Function to create and bind socket (called in parent process)
int create_and_bind_socket(int port) {
    printf("Creating socket...\n");
    
    // Create socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("Failed to create socket");
        return -1;
    }
    
    printf("Socket created successfully (fd=%d)\n", socket_fd);
    
    // Save global socket descriptor
    g_socket_fd = socket_fd;
    
    // Set socket options
    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Failed to set SO_REUSEADDR");
        close(socket_fd);
        return -1;
    }
    
    printf("Socket options set successfully\n");
    
    return socket_fd;
}

// Function to bind and listen (called in child process after fork)
int bind_and_listen(int socket_fd, int port) {
    printf("Child process binding socket %d...\n", socket_fd);
    
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
        return -1;
    }
    
    printf("Socket bound successfully to port %d\n", port);
    
    // Listen on socket
    int listen_result = listen(socket_fd, 5);
    if (listen_result < 0) {
        printf("Failed to listen on socket: %s (errno=%d)\n", 
               strerror(errno), errno);
        return -1;
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
    
    return 0;
}

// Function to simulate daemonization
int daemonize() {
    pid_t pid;
    
    // Fork process
    printf("Forking process...\n");
    pid = fork();
    
    if (pid < 0) {
        // Fork failed
        perror("Fork failed");
        return -1;
    } else if (pid > 0) {
        // Parent process
        printf("Parent process exiting, child PID: %d\n", pid);
        exit(0);
    }
    
    // Child process continues...
    printf("Child process started (PID: %d)\n", getpid());
    
    // Create new session
    if (setsid() < 0) {
        perror("Failed to create new session");
        return -1;
    }
    
    printf("Created new session\n");
    
    // Close standard file descriptors
    printf("Closing standard file descriptors...\n");
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    // Open /dev/null for standard file descriptors
    open("/dev/null", O_RDONLY);  // STDIN
    open("/dev/null", O_WRONLY);  // STDOUT
    open("/dev/null", O_WRONLY);  // STDERR
    
    return 0;
}

int main(int argc, char *argv[]) {
    int port = 5004;
    int daemon_mode = 0;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-d") == 0) {
            daemon_mode = 1;
        }
    }
    
    printf("Testing socket binding on port %d (daemon mode: %s)\n", 
           port, daemon_mode ? "yes" : "no");
    
    // Create and bind socket
    int socket_fd = create_and_bind_socket(port);
    if (socket_fd < 0) {
        printf("Failed to create socket\n");
        return 1;
    }
    
    if (daemon_mode) {
        // Save a copy of stdout for logging before daemonizing
        int log_fd = dup(STDOUT_FILENO);
        
        printf("Starting in daemon mode...\n");
        
        // Daemonize
        if (daemonize() < 0) {
            fprintf(stderr, "Failed to daemonize\n");
            return 1;
        }
        
        // Restore stdout for logging in daemon
        dup2(log_fd, STDOUT_FILENO);
        close(log_fd);
        
        printf("--- DAEMON PROCESS LOGGING ---\n");
        printf("Daemon process started (PID: %d)\n", getpid());
        printf("Socket descriptor after daemonization: %d\n", socket_fd);
        
        // Check socket descriptor validity
        int socket_error = 0;
        socklen_t error_len = sizeof(socket_error);
        if (getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_len) < 0) {
            printf("Socket validation failed after daemonization: %s (errno=%d)\n", 
                  strerror(errno), errno);
        } else if (socket_error != 0) {
            printf("Socket has error state after daemonization: %s (error=%d)\n", 
                  strerror(socket_error), socket_error);
        } else {
            printf("Socket is valid after daemonization\n");
        }
    }
    
    // Bind and listen in both foreground and daemon mode
    if (bind_and_listen(socket_fd, port) < 0) {
        printf("Failed to bind and listen\n");
        return 1;
    }
    
    // Keep the process running
    printf("Server running on port %d. Press Ctrl+C to exit.\n", port);
    while (1) {
        sleep(1);
    }
    
    return 0;
}