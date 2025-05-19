#include "core/server.h"
#include "core/thread_pool.h"
#include "api/api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

/* Global variables */
static volatile int g_shutdown_requested = 0;
static volatile int g_test_completed = 0;
static int g_signal_pipe[2] = {-1, -1};

/* Forward declarations */
void* server_thread_func(void* arg);
void* client_thread_func(void* arg);
void signal_handler(int sig);
void handle_test_client(void* arg);

/*
 * Test parameters
 */
#define DEFAULT_PORT 5050  /* Use non-standard port for testing */
#define DEFAULT_NUM_CLIENTS 10
#define DEFAULT_CLIENT_DELAY 100000  /* 100ms delay between client connections */
#define DEFAULT_TEST_DURATION 10  /* 10 seconds test duration */

/*
 * Client parameters
 */
typedef struct {
    int client_id;
    char* server_host;
    int server_port;
    int delay_ms;
} client_params_t;

/*
 * Signal handler
 */
void signal_handler(int sig) {
    printf("Received signal %d\n", sig);
    g_shutdown_requested = 1;
    
    /* Signal through pipe */
    if (g_signal_pipe[1] >= 0) {
        char byte = 1;
        write(g_signal_pipe[1], &byte, 1);
    }
}

/*
 * Server thread function
 */
void* server_thread_func(void* arg) {
    server_config_t* config = (server_config_t*)arg;
    
    printf("Server thread starting on port %d\n", config->port);
    
    /* Create a minimal API context for testing */
    api_context_t* api_ctx = malloc(sizeof(api_context_t));
    if (!api_ctx) {
        fprintf(stderr, "Error: Failed to allocate memory for API context\n");
        return NULL;
    }
    
    /* Initialize API context */
    memset(api_ctx, 0, sizeof(api_context_t));
    
    /* Create signal pipe */
    if (pipe(g_signal_pipe) < 0) {
        fprintf(stderr, "Error: Failed to create signal pipe\n");
        free(api_ctx);
        return NULL;
    }
    
    /* Set up signal handler */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    
    /* Initialize socket */
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        fprintf(stderr, "Error: Failed to create socket\n");
        free(api_ctx);
        return NULL;
    }
    
    /* Set socket options */
    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        fprintf(stderr, "Error: Failed to set SO_REUSEADDR\n");
        close(socket_fd);
        free(api_ctx);
        return NULL;
    }
    
    /* Prepare address structure */
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(config->port);
    
    /* Bind socket */
    if (bind(socket_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        fprintf(stderr, "Error: Failed to bind socket to port %d\n", config->port);
        close(socket_fd);
        free(api_ctx);
        return NULL;
    }
    
    /* Listen for connections */
    if (listen(socket_fd, config->max_connections) < 0) {
        fprintf(stderr, "Error: Failed to listen on socket\n");
        close(socket_fd);
        free(api_ctx);
        return NULL;
    }
    
    /* Store socket fd in config */
    config->socket_fd = socket_fd;
    
    printf("Server socket initialized (fd=%d, port=%d)\n", socket_fd, config->port);
    
    /* Create thread pool configuration */
    thread_pool_config_t pool_config = {
        .min_threads = config->max_connections / 4,
        .max_threads = config->max_connections,
        .queue_size = config->max_connections * 2,
        .idle_timeout = 60
    };
    
    /* Create thread pool */
    thread_pool_t* pool = thread_pool_create_config(&pool_config);
    if (!pool) {
        fprintf(stderr, "Error: Failed to create thread pool\n");
        close(socket_fd);
        free(api_ctx);
        return NULL;
    }
    
    /* Store thread pool in config */
    config->thread_pool = pool;
    
    printf("Thread pool created with %d-%d threads\n", 
           pool_config.min_threads, pool_config.max_threads);
    
    /* Set up file descriptor set for select */
    fd_set read_fds;
    int max_fd = socket_fd;
    
    /* If signal pipe is active, include it in select */
    if (g_signal_pipe[0] >= 0 && g_signal_pipe[0] > max_fd) {
        max_fd = g_signal_pipe[0];
    }
    
    printf("Server accepting connections\n");
    
    /* Accept loop */
    while (!g_shutdown_requested) {
        /* Clear and set file descriptor set */
        FD_ZERO(&read_fds);
        FD_SET(socket_fd, &read_fds);
        
        /* Add signal pipe to set */
        if (g_signal_pipe[0] >= 0) {
            FD_SET(g_signal_pipe[0], &read_fds);
        }
        
        /* Wait for activity with timeout */
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
        
        /* Check for errors */
        if (activity < 0) {
            if (errno == EINTR) {
                continue;  /* Interrupted system call */
            }
            
            fprintf(stderr, "Error: select() failed\n");
            break;
        }
        
        /* Check for timeout */
        if (activity == 0) {
            continue;  /* No activity, continue loop */
        }
        
        /* Check for signal pipe activity */
        if (g_signal_pipe[0] >= 0 && FD_ISSET(g_signal_pipe[0], &read_fds)) {
            char buffer[10];
            read(g_signal_pipe[0], buffer, sizeof(buffer));
            printf("Server received shutdown signal\n");
            break;
        }
        
        /* Check for socket activity */
        if (FD_ISSET(socket_fd, &read_fds)) {
            /* Accept connection */
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(socket_fd, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_fd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;  /* Non-blocking socket with no connections ready */
                }
                
                fprintf(stderr, "Error: accept() failed\n");
                continue;
            }
            
            /* Create client connection structure */
            client_conn_t* client = malloc(sizeof(client_conn_t));
            if (!client) {
                fprintf(stderr, "Error: Failed to allocate memory for client connection\n");
                close(client_fd);
                continue;
            }
            
            /* Initialize client connection */
            client->client_fd = client_fd;
            client->address = client_addr;
            client->api_ctx = api_ctx;
            
            /* Add client to thread pool */
            if (thread_pool_add_work(pool, handle_test_client, client) != 0) {
                fprintf(stderr, "Error: Failed to add client to thread pool\n");
                free(client);
                close(client_fd);
                continue;
            }
            
            printf("Accepted connection from %s:%d (fd=%d)\n", 
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), client_fd);
        }
    }
    
    printf("Server shutting down\n");
    
    /* Close socket */
    if (socket_fd > 0) {
        close(socket_fd);
    }
    
    /* Destroy thread pool */
    if (pool) {
        thread_pool_destroy(pool);
    }
    
    /* Free API context */
    free(api_ctx);
    
    /* Close signal pipe */
    for (int i = 0; i < 2; i++) {
        if (g_signal_pipe[i] >= 0) {
            close(g_signal_pipe[i]);
            g_signal_pipe[i] = -1;
        }
    }
    
    printf("Server cleanup complete\n");
    
    return NULL;
}

/*
 * Handle test client in worker thread
 */
void handle_test_client(void* arg) {
    client_conn_t* client = (client_conn_t*)arg;
    int client_fd = client->client_fd;
    
    /* Get thread ID for logging */
    pthread_t tid = pthread_self();
    pid_t system_tid = syscall(SYS_gettid);
    
    printf("Handling client on thread ID %lu (system_tid=%d)\n", 
           (unsigned long)tid, system_tid);
    
    /* Read request */
    char buffer[4096] = {0};
    int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    
    if (bytes_read <= 0) {
        printf("Client connection closed or error\n");
        close(client_fd);
        free(client);
        return;
    }
    
    /* Null-terminate request */
    buffer[bytes_read] = '\0';
    
    /* Process request */
    printf("Received request:\n%s\n", buffer);
    
    /* Simulate processing with random delay */
    unsigned int sleep_time = rand() % 500000 + 100000;  /* 0.1-0.6 seconds */
    usleep(sleep_time);
    
    /* Prepare response */
    char response[4096];
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/plain\r\n"
             "Content-Length: 15\r\n"
             "Connection: close\r\n"
             "\r\n"
             "Hello, Client!\r\n");
    
    /* Send response */
    write(client_fd, response, strlen(response));
    
    /* Close connection */
    close(client_fd);
    
    printf("Client handled successfully on thread ID %lu (system_tid=%d)\n", 
           (unsigned long)tid, system_tid);
    
    /* Free client data */
    free(client);
}

/*
 * Client thread function
 */
void* client_thread_func(void* arg) {
    client_params_t* params = (client_params_t*)arg;
    
    printf("Client %d started\n", params->client_id);
    
    /* Sleep for the specified delay */
    if (params->delay_ms > 0) {
        usleep(params->delay_ms * 1000);
    }
    
    /* Create socket */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        fprintf(stderr, "Client %d: Failed to create socket\n", params->client_id);
        free(params);
        return NULL;
    }
    
    /* Prepare server address */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(params->server_port);
    
    /* Convert hostname to IP address */
    if (inet_pton(AF_INET, params->server_host, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Client %d: Invalid server address\n", params->client_id);
        close(sock);
        free(params);
        return NULL;
    }
    
    /* Connect to server */
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "Client %d: Failed to connect to server\n", params->client_id);
        close(sock);
        free(params);
        return NULL;
    }
    
    printf("Client %d connected to server\n", params->client_id);
    
    /* Prepare HTTP request */
    char request[1024];
    snprintf(request, sizeof(request),
             "GET / HTTP/1.1\r\n"
             "Host: %s:%d\r\n"
             "User-Agent: ThreadPoolTest/%d\r\n"
             "Accept: */*\r\n"
             "Connection: close\r\n"
             "\r\n",
             params->server_host, params->server_port, params->client_id);
    
    /* Send request */
    if (write(sock, request, strlen(request)) < 0) {
        fprintf(stderr, "Client %d: Failed to send request\n", params->client_id);
        close(sock);
        free(params);
        return NULL;
    }
    
    printf("Client %d sent request\n", params->client_id);
    
    /* Read response */
    char buffer[4096] = {0};
    int bytes_read = read(sock, buffer, sizeof(buffer) - 1);
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("Client %d received response:\n%s\n", params->client_id, buffer);
    } else {
        printf("Client %d: Server closed connection or error\n", params->client_id);
    }
    
    /* Close socket */
    close(sock);
    
    printf("Client %d completed\n", params->client_id);
    
    /* Free parameters */
    free(params);
    
    return NULL;
}

/*
 * Main function
 */
int main(int argc, char* argv[]) {
    /* Parse command line arguments */
    int port = DEFAULT_PORT;
    int num_clients = DEFAULT_NUM_CLIENTS;
    int client_delay = DEFAULT_CLIENT_DELAY;
    int test_duration = DEFAULT_TEST_DURATION;
    
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    if (argc > 2) {
        num_clients = atoi(argv[2]);
    }
    
    if (argc > 3) {
        client_delay = atoi(argv[3]);
    }
    
    if (argc > 4) {
        test_duration = atoi(argv[4]);
    }
    
    /* Display test parameters */
    printf("Server Thread Pool Test\n");
    printf("=====================\n");
    printf("Port: %d\n", port);
    printf("Number of clients: %d\n", num_clients);
    printf("Client delay: %d ms\n", client_delay);
    printf("Test duration: %d seconds\n", test_duration);
    printf("\n");
    
    /* Seed random number generator */
    srand(time(NULL));
    
    /* Create server configuration */
    server_config_t* config = malloc(sizeof(server_config_t));
    if (!config) {
        fprintf(stderr, "Error: Failed to allocate memory for server configuration\n");
        return 1;
    }
    
    /* Initialize server configuration */
    memset(config, 0, sizeof(server_config_t));
    config->port = port;
    config->max_connections = 50;
    config->host = "127.0.0.1";
    
    /* Start server thread */
    pthread_t server_thread;
    if (pthread_create(&server_thread, NULL, server_thread_func, config) != 0) {
        fprintf(stderr, "Error: Failed to create server thread\n");
        free(config);
        return 1;
    }
    
    /* Allow server to start before connecting clients */
    printf("Waiting for server to start...\n");
    sleep(2);
    
    /* Start client threads */
    pthread_t* client_threads = malloc(num_clients * sizeof(pthread_t));
    if (!client_threads) {
        fprintf(stderr, "Error: Failed to allocate memory for client threads\n");
        g_shutdown_requested = 1;
        pthread_join(server_thread, NULL);
        free(config);
        return 1;
    }
    
    printf("Starting client threads...\n");
    
    for (int i = 0; i < num_clients; i++) {
        /* Create client parameters */
        client_params_t* params = malloc(sizeof(client_params_t));
        if (!params) {
            fprintf(stderr, "Error: Failed to allocate memory for client parameters\n");
            continue;
        }
        
        /* Initialize client parameters */
        params->client_id = i + 1;
        params->server_host = "127.0.0.1";
        params->server_port = port;
        params->delay_ms = (client_delay * i) / num_clients;
        
        /* Start client thread */
        if (pthread_create(&client_threads[i], NULL, client_thread_func, params) != 0) {
            fprintf(stderr, "Error: Failed to create client thread %d\n", i + 1);
            free(params);
            continue;
        }
        
        printf("Client thread %d started\n", i + 1);
    }
    
    /* Wait for all client threads to complete */
    printf("Waiting for client threads to complete...\n");
    
    for (int i = 0; i < num_clients; i++) {
        pthread_join(client_threads[i], NULL);
    }
    
    printf("All client threads completed\n");
    
    /* Mark test as completed */
    g_test_completed = 1;
    
    /* Keep server running for specified duration */
    printf("Test complete. Keeping server running for %d seconds...\n", test_duration);
    
    /* Wait for test duration */
    for (int i = 0; i < test_duration && !g_shutdown_requested; i++) {
        sleep(1);
        printf("Server running: %d/%d seconds\n", i + 1, test_duration);
    }
    
    /* Shutdown server */
    printf("Shutting down server...\n");
    g_shutdown_requested = 1;
    
    /* Signal through pipe */
    if (g_signal_pipe[1] >= 0) {
        char byte = 1;
        write(g_signal_pipe[1], &byte, 1);
    }
    
    /* Wait for server thread to complete */
    pthread_join(server_thread, NULL);
    
    /* Cleanup */
    free(client_threads);
    free(config);
    
    printf("Server thread completed\n");
    printf("Test completed successfully\n");
    
    return 0;
}