#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>

/**
 * Comprehensive server socket binding test
 * 
 * This test program simulates the server initialization sequence
 * and tests both hostname resolution and thread pool initialization.
 */

#define MAX_THREAD_POOL_SIZE 10
#define DEFAULT_PORT 6789
#define DEFAULT_HOST "0.0.0.0"

/* Thread pool implementation */
typedef struct {
    pthread_t* threads;
    int thread_count;
    int is_running;
} simple_thread_pool_t;

/* Global flag for clean shutdown */
volatile int shutdown_requested = 0;

/* Signal handler for clean shutdown */
void handle_signal(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    shutdown_requested = 1;
}

/* Function to resolve hostname to IP */
int resolve_hostname(const char* hostname, char* ip_str, size_t ip_str_size) {
    struct addrinfo hints, *result, *rp;
    int status;
    
    if (!hostname || !ip_str || ip_str_size == 0) {
        return -1;
    }
    
    /* Set hints for getaddrinfo */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;     /* IPv4 only */
    hints.ai_socktype = SOCK_STREAM;
    
    /* Get address info */
    status = getaddrinfo(hostname, NULL, &hints, &result);
    if (status != 0) {
        printf("Error resolving hostname '%s': %s\n", hostname, gai_strerror(status));
        return -1;
    }
    
    /* Get first result */
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        struct sockaddr_in* addr = (struct sockaddr_in*)rp->ai_addr;
        if (inet_ntop(AF_INET, &addr->sin_addr, ip_str, ip_str_size) != NULL) {
            freeaddrinfo(result);
            return 0;
        }
    }
    
    freeaddrinfo(result);
    return -1;
}

/* Worker thread function */
void* worker_thread(void* arg) {
    int thread_id = (int)(intptr_t)arg;
    printf("Thread %d started\n", thread_id);
    
    while (!shutdown_requested) {
        /* Just sleep to simulate being ready for work */
        sleep(1);
    }
    
    printf("Thread %d exiting\n", thread_id);
    return NULL;
}

/* Initialize a simple thread pool */
simple_thread_pool_t* init_thread_pool(int size) {
    simple_thread_pool_t* pool = malloc(sizeof(simple_thread_pool_t));
    if (!pool) {
        perror("Failed to allocate thread pool structure");
        return NULL;
    }
    
    /* Initialize pool */
    pool->threads = malloc(sizeof(pthread_t) * size);
    if (!pool->threads) {
        perror("Failed to allocate thread array");
        free(pool);
        return NULL;
    }
    
    pool->thread_count = 0;
    pool->is_running = 1;
    
    /* Create worker threads */
    for (int i = 0; i < size; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker_thread, (void*)(intptr_t)i) != 0) {
            perror("Failed to create thread");
            /* Clean up already created threads */
            pool->is_running = 0;
            for (int j = 0; j < i; j++) {
                pthread_join(pool->threads[j], NULL);
            }
            free(pool->threads);
            free(pool);
            return NULL;
        }
        pool->thread_count++;
    }
    
    printf("Thread pool initialized with %d threads\n", pool->thread_count);
    return pool;
}

/* Destroy thread pool */
void destroy_thread_pool(simple_thread_pool_t* pool) {
    if (!pool) {
        return;
    }
    
    /* Signal shutdown */
    pool->is_running = 0;
    
    /* Wait for all threads to complete */
    for (int i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    
    /* Free resources */
    free(pool->threads);
    free(pool);
    
    printf("Thread pool destroyed\n");
}

/* Initialize socket and bind */
int init_socket(const char* host, int port) {
    int socket_fd;
    struct sockaddr_in address;
    
    printf("Initializing socket on %s:%d\n", host, port);
    
    /* Create socket */
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("Failed to create socket");
        return -1;
    }
    
    printf("Socket created successfully (fd=%d)\n", socket_fd);
    
    /* Set socket options */
    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Failed to set SO_REUSEADDR");
        close(socket_fd);
        return -1;
    }
    
    /* Set socket to non-blocking */
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0 || fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("Failed to set socket to non-blocking mode");
        close(socket_fd);
        return -1;
    }
    
    /* Create address structure */
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    
    /* Resolve host */
    if (strcmp(host, "0.0.0.0") == 0 || strcmp(host, "any") == 0) {
        address.sin_addr.s_addr = INADDR_ANY;
        printf("Using INADDR_ANY for binding\n");
    } else if (strcmp(host, "127.0.0.1") == 0 || strcmp(host, "localhost") == 0) {
        address.sin_addr.s_addr = inet_addr("127.0.0.1");
        printf("Using localhost (127.0.0.1) for binding\n");
    } else {
        char resolved_ip[INET_ADDRSTRLEN];
        if (resolve_hostname(host, resolved_ip, INET_ADDRSTRLEN) == 0) {
            printf("Resolved hostname '%s' to IP: %s\n", host, resolved_ip);
            if (inet_pton(AF_INET, resolved_ip, &address.sin_addr) <= 0) {
                printf("Failed to convert IP address: %s\n", strerror(errno));
                close(socket_fd);
                return -1;
            }
        } else {
            printf("Failed to resolve hostname, trying direct conversion\n");
            if (inet_pton(AF_INET, host, &address.sin_addr) <= 0) {
                printf("Failed to convert IP address: %s\n", strerror(errno));
                close(socket_fd);
                return -1;
            }
        }
    }
    
    /* Bind socket */
    if (bind(socket_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        printf("Failed to bind socket: %s (errno=%d)\n", strerror(errno), errno);
        close(socket_fd);
        return -1;
    }
    
    printf("Socket bound successfully\n");
    
    /* Set socket to listen state */
    if (listen(socket_fd, 5) < 0) {
        printf("Failed to listen on socket: %s (errno=%d)\n", strerror(errno), errno);
        close(socket_fd);
        return -1;
    }
    
    printf("Socket now listening\n");
    
    /* Verify listening state */
    int acceptconn = 0;
    socklen_t acceptconn_len = sizeof(acceptconn);
    if (getsockopt(socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &acceptconn, &acceptconn_len) < 0) {
        printf("Failed to check SO_ACCEPTCONN: %s (errno=%d)\n", strerror(errno), errno);
    } else {
        printf("Socket listening state: %s\n", acceptconn ? "LISTENING" : "NOT LISTENING");
    }
    
    /* Run netstat to verify port is visible */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "netstat -tuln | grep %d || echo 'Port %d NOT FOUND in netstat'", 
            port, port);
    printf("\nRunning: %s\n", cmd);
    system(cmd);
    
    return socket_fd;
}

/* Accept loop function */
void accept_loop(int socket_fd) {
    int client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    fd_set read_fds;
    struct timeval tv;
    
    printf("Starting accept loop on socket %d\n", socket_fd);
    
    while (!shutdown_requested) {
        /* Clear and set file descriptor set */
        FD_ZERO(&read_fds);
        FD_SET(socket_fd, &read_fds);
        
        /* Set timeout */
        tv.tv_sec = 1;  /* 1 second timeout */
        tv.tv_usec = 0;
        
        /* Wait for activity */
        int activity = select(socket_fd + 1, &read_fds, NULL, NULL, &tv);
        
        if (activity < 0 && errno != EINTR) {
            perror("Select error");
            break;
        }
        
        /* Timeout or no activity */
        if (activity <= 0) {
            continue;
        }
        
        /* Check for socket activity */
        if (FD_ISSET(socket_fd, &read_fds)) {
            client_fd = accept(socket_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    /* Non-blocking socket with no connections ready */
                    continue;
                }
                perror("Accept error");
                continue;
            }
            
            /* Connection accepted */
            printf("Accepted connection from %s:%d (fd=%d)\n", 
                  inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), client_fd);
            
            /* Just close the connection for this test */
            close(client_fd);
        }
    }
}

int main(int argc, char** argv) {
    int port = DEFAULT_PORT;
    char* host = DEFAULT_HOST;
    
    /* Parse command line arguments */
    if (argc > 1) {
        host = argv[1];
    }
    if (argc > 2) {
        port = atoi(argv[2]);
    }
    
    printf("Server Socket Binding Test\n");
    printf("==========================\n\n");
    printf("Using host: %s\n", host);
    printf("Using port: %d\n", port);
    
    /* Set up signal handler */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    /* Test hostname resolution */
    if (strcmp(host, "0.0.0.0") != 0 && strcmp(host, "127.0.0.1") != 0) {
        char resolved_ip[INET_ADDRSTRLEN];
        printf("\nTesting hostname resolution for '%s'...\n", host);
        if (resolve_hostname(host, resolved_ip, INET_ADDRSTRLEN) == 0) {
            printf("Success: Resolved '%s' to IP: %s\n", host, resolved_ip);
        } else {
            printf("Warning: Failed to resolve hostname '%s'\n", host);
            printf("Continuing with test anyway...\n");
        }
    }
    
    /* Initialize thread pool */
    printf("\nInitializing thread pool...\n");
    simple_thread_pool_t* pool = init_thread_pool(4);
    if (!pool) {
        fprintf(stderr, "Failed to initialize thread pool\n");
        return 1;
    }
    
    /* Initialize socket */
    printf("\nInitializing socket...\n");
    int socket_fd = init_socket(host, port);
    if (socket_fd < 0) {
        fprintf(stderr, "Failed to initialize socket\n");
        destroy_thread_pool(pool);
        return 1;
    }
    
    /* Start accept loop */
    printf("\nStarting accept loop (Press Ctrl+C to exit)...\n");
    accept_loop(socket_fd);
    
    /* Clean up */
    printf("\nCleaning up resources...\n");
    close(socket_fd);
    destroy_thread_pool(pool);
    
    printf("\nTest completed successfully\n");
    return 0;
}