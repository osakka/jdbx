#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdarg.h>
#include <time.h>

/* Simple thread monitor to debug server thread creation issues */

/* Thread state */
typedef enum {
    THREAD_CREATED,
    THREAD_RUNNING,
    THREAD_STOPPED,
    THREAD_ERROR
} thread_state_t;

/* Thread monitoring struct */
typedef struct {
    pthread_t thread_id;
    thread_state_t state;
    int socket_fd;
    time_t start_time;
    time_t last_update;
    char error_message[256];
} thread_monitor_t;

/* Global variables */
static thread_monitor_t g_thread_monitor = {0};
static int g_debug_fd = -1;
static char g_log_path[256] = "/tmp/thread_debug.log"; 

/* Initialize thread monitor */
int thread_monitor_init(const char* log_path) {
    /* Clear the monitor */
    memset(&g_thread_monitor, 0, sizeof(g_thread_monitor));
    g_thread_monitor.state = THREAD_STOPPED;
    g_thread_monitor.socket_fd = -1;
    
    /* Set log path if provided */
    if (log_path) {
        strncpy(g_log_path, log_path, sizeof(g_log_path) - 1);
    }
    
    /* Open debug file */
    g_debug_fd = open(g_log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (g_debug_fd < 0) {
        fprintf(stderr, "Error: Failed to open thread debug log: %s\n", strerror(errno));
        return 0;
    }
    
    /* Initial log message */
    char msg[512];
    time_t now = time(NULL);
    int len = snprintf(msg, sizeof(msg), 
                      "[%lu] Thread monitor initialized\n", (unsigned long)now);
    write(g_debug_fd, msg, len);
    
    return 1;
}

/* Log message to debug file */
void thread_monitor_log(const char* format, ...) {
    if (g_debug_fd < 0) return;
    
    char buffer[1024];
    va_list args;
    va_start(args, format);
    
    time_t now = time(NULL);
    int prefix_len = snprintf(buffer, sizeof(buffer), "[%lu] ", (unsigned long)now);
    
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
    
    write(g_debug_fd, buffer, len);
}

/* Register thread with monitor */
void thread_monitor_register(pthread_t thread_id, int socket_fd) {
    g_thread_monitor.thread_id = thread_id;
    g_thread_monitor.state = THREAD_CREATED;
    g_thread_monitor.socket_fd = socket_fd;
    g_thread_monitor.start_time = time(NULL);
    g_thread_monitor.last_update = g_thread_monitor.start_time;
    
    thread_monitor_log("Registered thread %lu with socket %d", 
                      (unsigned long)thread_id, socket_fd);
}

/* Update thread state */
void thread_monitor_update_state(thread_state_t state, const char* message) {
    g_thread_monitor.state = state;
    g_thread_monitor.last_update = time(NULL);
    
    if (message) {
        strncpy(g_thread_monitor.error_message, message, sizeof(g_thread_monitor.error_message) - 1);
    } else {
        g_thread_monitor.error_message[0] = '\0';
    }
    
    thread_monitor_log("Thread %lu state updated to %d: %s", 
                      (unsigned long)g_thread_monitor.thread_id, 
                      state, 
                      message ? message : "No message");
}

/* Check if thread is alive */
int thread_monitor_is_alive() {
    if (g_thread_monitor.thread_id == 0) {
        thread_monitor_log("No thread registered");
        return 0;
    }
    
    /* Check thread state */
    int error = pthread_kill(g_thread_monitor.thread_id, 0);
    if (error == 0) {
        thread_monitor_log("Thread %lu is alive", (unsigned long)g_thread_monitor.thread_id);
        return 1;
    } else {
        thread_monitor_log("Thread %lu is not alive: %s (errno=%d)", 
                          (unsigned long)g_thread_monitor.thread_id, 
                          strerror(error), error);
        return 0;
    }
}

/* Check socket state */
int thread_monitor_check_socket() {
    if (g_thread_monitor.socket_fd < 0) {
        thread_monitor_log("Invalid socket descriptor: %d", g_thread_monitor.socket_fd);
        return 0;
    }
    
    /* Check socket error state */
    int socket_error = 0;
    socklen_t error_len = sizeof(socket_error);
    
    if (getsockopt(g_thread_monitor.socket_fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_len) < 0) {
        thread_monitor_log("Failed to check socket %d: %s (errno=%d)", 
                          g_thread_monitor.socket_fd, strerror(errno), errno);
        return 0;
    }
    
    if (socket_error != 0) {
        thread_monitor_log("Socket %d has error: %s (errno=%d)", 
                          g_thread_monitor.socket_fd, strerror(socket_error), socket_error);
        return 0;
    }
    
    /* Check listening state */
    int acceptconn = 0;
    socklen_t acceptconn_len = sizeof(acceptconn);
    
    if (getsockopt(g_thread_monitor.socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &acceptconn, &acceptconn_len) < 0) {
        thread_monitor_log("Failed to check if socket %d is listening: %s (errno=%d)", 
                          g_thread_monitor.socket_fd, strerror(errno), errno);
        return 0;
    }
    
    thread_monitor_log("Socket %d listening state: %s", 
                      g_thread_monitor.socket_fd, 
                      acceptconn ? "LISTENING" : "NOT LISTENING");
                      
    /* Check with netstat */
    thread_monitor_log("Checking netstat for socket visibility");
    
    char cmd[256];
    char output[1024] = {0};
    FILE *fp;
    
    /* Get local socket info */
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    
    if (getsockname(g_thread_monitor.socket_fd, (struct sockaddr*)&addr, &addr_len) == 0) {
        int port = ntohs(addr.sin_port);
        
        snprintf(cmd, sizeof(cmd), 
                "netstat -tuln | grep ':%d ' 2>/dev/null || ss -tuln | grep ':%d ' 2>/dev/null", 
                port, port);
                
        fp = popen(cmd, "r");
        if (fp) {
            if (fread(output, 1, sizeof(output) - 1, fp) > 0) {
                output[sizeof(output) - 1] = '\0';
                thread_monitor_log("Socket found in netstat/ss on port %d: %s", port, output);
            } else {
                thread_monitor_log("Socket NOT found in netstat/ss on port %d", port);
            }
            pclose(fp);
        }
    } else {
        thread_monitor_log("Failed to get socket port: %s (errno=%d)", 
                          strerror(errno), errno);
    }
    
    return acceptconn != 0;
}

/* Print thread status report */
void thread_monitor_status() {
    time_t now = time(NULL);
    
    thread_monitor_log("Thread Monitor Status Report:");
    thread_monitor_log("  Thread ID: %lu", (unsigned long)g_thread_monitor.thread_id);
    thread_monitor_log("  Socket FD: %d", g_thread_monitor.socket_fd);
    thread_monitor_log("  State: %d", g_thread_monitor.state);
    thread_monitor_log("  Started: %lu (%ld seconds ago)", 
                      (unsigned long)g_thread_monitor.start_time, 
                      now - g_thread_monitor.start_time);
    thread_monitor_log("  Last Update: %lu (%ld seconds ago)", 
                      (unsigned long)g_thread_monitor.last_update, 
                      now - g_thread_monitor.last_update);
    
    if (g_thread_monitor.error_message[0] != '\0') {
        thread_monitor_log("  Error: %s", g_thread_monitor.error_message);
    }
    
    /* Check if thread is alive */
    thread_monitor_is_alive();
    
    /* Check socket state */
    thread_monitor_check_socket();
}

/* Cleanup thread monitor */
void thread_monitor_cleanup() {
    thread_monitor_log("Thread monitor cleanup");
    
    if (g_debug_fd >= 0) {
        close(g_debug_fd);
        g_debug_fd = -1;
    }
}

/* Test function for standalone use */
#ifdef THREAD_MONITOR_TEST
void* test_thread(void* arg) {
    int socket_fd = *(int*)arg;
    
    thread_monitor_update_state(THREAD_RUNNING, "Thread started");
    
    /* Simulate thread work */
    for (int i = 0; i < 5; i++) {
        sleep(1);
        thread_monitor_log("Thread working... %d", i);
    }
    
    thread_monitor_update_state(THREAD_STOPPED, "Thread completed");
    return NULL;
}

int main() {
    /* Initialize monitor */
    thread_monitor_init("/tmp/thread_test.log");
    
    /* Create a test socket */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        thread_monitor_log("Failed to create socket: %s", strerror(errno));
        return 1;
    }
    
    /* Set socket options */
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    /* Bind to a test port */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8888);
    
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        thread_monitor_log("Failed to bind: %s", strerror(errno));
        close(sockfd);
        return 1;
    }
    
    /* Listen on socket */
    if (listen(sockfd, 5) < 0) {
        thread_monitor_log("Failed to listen: %s", strerror(errno));
        close(sockfd);
        return 1;
    }
    
    /* Create a test thread */
    pthread_t tid;
    if (pthread_create(&tid, NULL, test_thread, &sockfd) != 0) {
        thread_monitor_log("Failed to create thread: %s", strerror(errno));
        close(sockfd);
        return 1;
    }
    
    /* Register thread */
    thread_monitor_register(tid, sockfd);
    
    /* Print initial status */
    thread_monitor_status();
    
    /* Wait for thread to complete */
    pthread_join(tid, NULL);
    
    /* Print final status */
    thread_monitor_status();
    
    /* Cleanup */
    close(sockfd);
    thread_monitor_cleanup();
    
    return 0;
}
#endif /* THREAD_MONITOR_TEST */