#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <stdarg.h>

/* Simple TCP port binding test utility
 * This utility attempts to bind to a TCP port and reports detailed
 * diagnostic information about any failures
 */
 
int g_socket_fd = -1;
int g_port = 5000;
int g_verbose = 1;
int g_debug_fd = -1;
int g_running = 1;

/* Log message to both stdout and debug file */
void log_msg(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    
    /* Format message with timestamp */
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    
    int prefix_len = strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S] ", tm_info);
    vsnprintf(buffer + prefix_len, sizeof(buffer) - prefix_len, format, args);
    va_end(args);
    
    /* Add newline if needed */
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len-1] != '\n') {
        if (len < sizeof(buffer) - 1) {
            buffer[len] = '\n';
            buffer[len+1] = '\0';
            len++;
        }
    }
    
    /* Print to stdout */
    fputs(buffer, stdout);
    fflush(stdout);
    
    /* Write to debug file if open */
    if (g_debug_fd >= 0) {
        write(g_debug_fd, buffer, len);
    }
}

/* Check if port is in use */
int check_port_in_use(int port) {
    int test_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (test_sock < 0) {
        log_msg("Socket creation failed: %s (errno=%d)\n", strerror(errno), errno);
        return -1;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    int result = bind(test_sock, (struct sockaddr*)&addr, sizeof(addr));
    int bind_errno = errno;
    
    /* Always close the socket */
    close(test_sock);
    
    if (result < 0) {
        log_msg("Port %d is in use: %s (errno=%d)\n", port, strerror(bind_errno), bind_errno);
        return 1;
    }
    
    log_msg("Port %d is available\n", port);
    return 0;
}

/* Run netstat to check for port usage */
void check_netstat(int port) {
    char cmd[256];
    char output[1024] = {0};
    FILE *fp;
    
    log_msg("Checking netstat for port %d\n", port);
    
    snprintf(cmd, sizeof(cmd), 
            "netstat -tuln | grep ':%d ' 2>/dev/null || ss -tuln | grep ':%d ' 2>/dev/null", 
            port, port);
            
    fp = popen(cmd, "r");
    if (fp) {
        if (fread(output, 1, sizeof(output) - 1, fp) > 0) {
            output[sizeof(output) - 1] = '\0';
            log_msg("Port %d found in netstat/ss: %s\n", port, output);
        } else {
            log_msg("Port %d NOT found in netstat/ss\n", port);
        }
        pclose(fp);
    }
}

/* Check processes using this port */
void check_processes_using_port(int port) {
    char cmd[256];
    char output[1024] = {0};
    FILE *fp;
    
    log_msg("Checking processes using port %d\n", port);
    
    snprintf(cmd, sizeof(cmd), 
            "lsof -i :%d 2>/dev/null || echo 'No process found using port %d'", 
            port, port);
            
    fp = popen(cmd, "r");
    if (fp) {
        if (fread(output, 1, sizeof(output) - 1, fp) > 0) {
            output[sizeof(output) - 1] = '\0';
            log_msg("Processes using port %d:\n%s\n", port, output);
        }
        pclose(fp);
    }
}

/* Signal handler */
void signal_handler(int signum) {
    log_msg("Received signal %d (%s)\n", signum, strsignal(signum));
    
    if (signum == SIGINT || signum == SIGTERM) {
        g_running = 0;
    }
}

/* Cleanup resources */
void cleanup() {
    if (g_socket_fd >= 0) {
        log_msg("Closing socket %d\n", g_socket_fd);
        close(g_socket_fd);
        g_socket_fd = -1;
    }
    
    if (g_debug_fd >= 0) {
        close(g_debug_fd);
        g_debug_fd = -1;
    }
}

/* Print system information */
void print_system_info() {
    log_msg("System Information:");
    
    /* Hostname */
    char hostname[256] = {0};
    if (gethostname(hostname, sizeof(hostname) - 1) == 0) {
        log_msg("  Hostname: %s", hostname);
    }
    
    /* Get kernel version */
    FILE* fp = popen("uname -a", "r");
    if (fp) {
        char buffer[512] = {0};
        if (fgets(buffer, sizeof(buffer) - 1, fp)) {
            buffer[strcspn(buffer, "\n")] = '\0';
            log_msg("  Kernel: %s", buffer);
        }
        pclose(fp);
    }
    
    /* Check for containers */
    fp = popen("grep -c docker /proc/1/cgroup 2>/dev/null || echo 0", "r");
    if (fp) {
        char buffer[16] = {0};
        if (fgets(buffer, sizeof(buffer) - 1, fp)) {
            int count = atoi(buffer);
            log_msg("  Running in Docker: %s", count > 0 ? "Yes" : "No");
        }
        pclose(fp);
    }
    
    /* Check firewall status */
    log_msg("  Firewall Status:");
    fp = popen("command -v iptables >/dev/null && iptables -L -n | head -n 20", "r");
    if (fp) {
        char buffer[4096] = {0};
        size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, fp);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            log_msg("    IPTables:\n%s", buffer);
        } else {
            log_msg("    IPTables: Not available or empty");
        }
        pclose(fp);
    }
}

int main(int argc, char* argv[]) {
    /* Parse arguments */
    if (argc > 1) {
        g_port = atoi(argv[1]);
    }
    
    if (g_port <= 0 || g_port > 65535) {
        printf("Invalid port number. Using default port 5000\n");
        g_port = 5000;
    }
    
    /* Open debug log */
    char log_path[256];
    snprintf(log_path, sizeof(log_path), "/tmp/port_binding_test_%d.log", g_port);
    g_debug_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    
    if (g_debug_fd < 0) {
        printf("Warning: Failed to open debug log: %s\n", strerror(errno));
    }
    
    /* Set up signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Print header */
    log_msg("=== TCP Port Binding Test ===");
    log_msg("Testing port: %d", g_port);
    log_msg("Debug log: %s", log_path);
    log_msg("PID: %d", getpid());
    
    /* Print system information */
    print_system_info();
    
    /* Check if port is in use */
    if (check_port_in_use(g_port) != 0) {
        check_netstat(g_port);
        check_processes_using_port(g_port);
        log_msg("Port %d is in use, cannot continue testing", g_port);
        cleanup();
        return 1;
    }
    
    /* Create socket */
    log_msg("Creating TCP socket");
    g_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_socket_fd < 0) {
        log_msg("Socket creation failed: %s (errno=%d)", strerror(errno), errno);
        cleanup();
        return 1;
    }
    log_msg("Socket created successfully (fd=%d)", g_socket_fd);
    
    /* Set socket options */
    int opt = 1;
    if (setsockopt(g_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log_msg("Failed to set SO_REUSEADDR: %s (errno=%d)", strerror(errno), errno);
        /* Continue anyway */
    } else {
        log_msg("Set SO_REUSEADDR successfully");
    }
    
    #ifdef SO_REUSEPORT
    if (setsockopt(g_socket_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        log_msg("Failed to set SO_REUSEPORT: %s (errno=%d)", strerror(errno), errno);
    } else {
        log_msg("Set SO_REUSEPORT successfully");
    }
    #else
    log_msg("SO_REUSEPORT not supported on this system");
    #endif
    
    /* Prepare address structure */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(g_port);
    
    log_msg("Prepared address structure:");
    log_msg("  Family: %d (AF_INET)", addr.sin_family);
    log_msg("  Address: INADDR_ANY (0.0.0.0)");
    log_msg("  Port: %d", g_port);
    
    /* Bind socket */
    log_msg("Binding socket to port %d", g_port);
    int bind_result = bind(g_socket_fd, (struct sockaddr*)&addr, sizeof(addr));
    if (bind_result < 0) {
        log_msg("ERROR: Bind failed: %s (errno=%d)", strerror(errno), errno);
        check_netstat(g_port);
        check_processes_using_port(g_port);
        cleanup();
        return 1;
    }
    log_msg("SUCCESS: Socket bound to port %d", g_port);
    
    /* Listen on socket */
    log_msg("Setting socket to listen state");
    if (listen(g_socket_fd, 10) < 0) {
        log_msg("ERROR: Listen failed: %s (errno=%d)", strerror(errno), errno);
        cleanup();
        return 1;
    }
    log_msg("SUCCESS: Socket now listening on port %d", g_port);
    
    /* Verify socket state */
    int socket_error = 0;
    socklen_t error_len = sizeof(socket_error);
    if (getsockopt(g_socket_fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_len) < 0) {
        log_msg("Failed to check socket error: %s (errno=%d)", strerror(errno), errno);
    } else if (socket_error != 0) {
        log_msg("Socket has error state: %s (error=%d)", strerror(socket_error), socket_error);
    } else {
        log_msg("Socket has no errors");
    }
    
    /* Check listening state */
    int acceptconn = 0;
    socklen_t acceptconn_len = sizeof(acceptconn);
    if (getsockopt(g_socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &acceptconn, &acceptconn_len) < 0) {
        log_msg("Failed to check listening state: %s (errno=%d)", strerror(errno), errno);
    } else {
        log_msg("Socket listening state: %s", acceptconn ? "LISTENING" : "NOT LISTENING");
    }
    
    /* Check netstat */
    check_netstat(g_port);
    
    /* Accept connections */
    log_msg("Starting accept loop. Press Ctrl+C to exit.");
    
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    while (g_running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(g_socket_fd, &read_fds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int select_result = select(g_socket_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (select_result < 0) {
            if (errno == EINTR) continue;
            log_msg("Select failed: %s (errno=%d)", strerror(errno), errno);
            break;
        }
        
        if (select_result == 0) {
            /* Timeout, continue */
            continue;
        }
        
        /* Accept connection */
        int client_fd = accept(g_socket_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            log_msg("Accept failed: %s (errno=%d)", strerror(errno), errno);
            continue;
        }
        
        log_msg("Accepted connection from %s:%d", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        /* Send a simple response */
        const char* response = "HTTP/1.1 200 OK\r\nContent-Length: 22\r\nContent-Type: text/plain\r\n\r\nPort binding successful\r\n";
        write(client_fd, response, strlen(response));
        
        /* Close connection */
        close(client_fd);
    }
    
    /* Cleanup */
    log_msg("Test complete");
    cleanup();
    
    return 0;
}