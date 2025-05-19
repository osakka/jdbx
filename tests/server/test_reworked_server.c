#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

#define MAX_CONNECTIONS 10
#define BUFFER_SIZE 1024

/* Simple thread pool implementation for testing */
typedef struct {
    void (*task)(void*);
    void* arg;
} work_t;

typedef struct {
    work_t* queue;
    int size;
    int capacity;
    int front;
    int rear;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    int active_threads;
    int total_threads;
    int shutdown;
} thread_pool_t;

thread_pool_t* pool;

/* Function prototypes */
void* worker_thread(void* arg);
void handle_client(void* client_socket);
void signal_handler(int sig);

/* Initialize thread pool */
thread_pool_t* thread_pool_init(int thread_count, int queue_size) {
    thread_pool_t* pool = malloc(sizeof(thread_pool_t));
    if (!pool) return NULL;
    
    pool->queue = malloc(sizeof(work_t) * queue_size);
    if (!pool->queue) {
        free(pool);
        return NULL;
    }
    
    pool->size = 0;
    pool->capacity = queue_size;
    pool->front = 0;
    pool->rear = 0;
    pool->active_threads = 0;
    pool->total_threads = thread_count;
    pool->shutdown = 0;
    
    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->not_empty, NULL);
    pthread_cond_init(&pool->not_full, NULL);
    
    /* Create worker threads */
    pthread_t thread;
    for (int i = 0; i < thread_count; i++) {
        pthread_create(&thread, NULL, worker_thread, pool);
        pthread_detach(thread);
    }
    
    return pool;
}

/* Add work to thread pool */
int thread_pool_add(thread_pool_t* pool, void (*task)(void*), void* arg) {
    pthread_mutex_lock(&pool->mutex);
    
    /* Wait until queue is not full or shutdown */
    while (pool->size == pool->capacity && !pool->shutdown) {
        pthread_cond_wait(&pool->not_full, &pool->mutex);
    }
    
    /* Return if shutting down */
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->mutex);
        return -1;
    }
    
    /* Add work to queue */
    pool->queue[pool->rear].task = task;
    pool->queue[pool->rear].arg = arg;
    pool->rear = (pool->rear + 1) % pool->capacity;
    pool->size++;
    
    pthread_cond_signal(&pool->not_empty);
    pthread_mutex_unlock(&pool->mutex);
    
    return 0;
}

/* Worker thread function */
void* worker_thread(void* arg) {
    thread_pool_t* pool = (thread_pool_t*)arg;
    
    while (1) {
        pthread_mutex_lock(&pool->mutex);
        
        /* Wait until queue is not empty or shutdown */
        while (pool->size == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->not_empty, &pool->mutex);
        }
        
        /* Exit if shutting down and queue is empty */
        if (pool->shutdown && pool->size == 0) {
            pthread_mutex_unlock(&pool->mutex);
            pthread_exit(NULL);
        }
        
        /* Get work from queue */
        void (*task)(void*) = pool->queue[pool->front].task;
        void* arg = pool->queue[pool->front].arg;
        pool->front = (pool->front + 1) % pool->capacity;
        pool->size--;
        pool->active_threads++;
        
        pthread_cond_signal(&pool->not_full);
        pthread_mutex_unlock(&pool->mutex);
        
        /* Execute task */
        task(arg);
        
        /* Update active threads count */
        pthread_mutex_lock(&pool->mutex);
        pool->active_threads--;
        pthread_mutex_unlock(&pool->mutex);
    }
    
    return NULL;
}

/* Destroy thread pool */
void thread_pool_destroy(thread_pool_t* pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->mutex);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->not_empty);
    pthread_cond_broadcast(&pool->not_full);
    pthread_mutex_unlock(&pool->mutex);
    
    /* Allow threads to exit */
    sleep(1);
    
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->not_empty);
    pthread_cond_destroy(&pool->not_full);
    
    free(pool->queue);
    free(pool);
}

/* Handle client connection */
void handle_client(void* client_socket_ptr) {
    int client_socket = *((int*)client_socket_ptr);
    char buffer[BUFFER_SIZE];
    
    /* Free the socket pointer */
    free(client_socket_ptr);
    
    /* Read from client */
    int bytes = read(client_socket, buffer, BUFFER_SIZE - 1);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("Received: %s\n", buffer);
        
        /* Send response */
        const char* response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 13\r\n\r\nHello, World!";
        write(client_socket, response, strlen(response));
    }
    
    /* Close connection */
    close(client_socket);
}

/* Signal handler */
void signal_handler(int sig) {
    printf("Received signal %d, shutting down...\n", sig);
    thread_pool_destroy(pool);
    exit(0);
}

int main(int argc, char* argv[]) {
    int port = 8888;
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    /* Set up signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Initialize thread pool */
    pool = thread_pool_init(4, MAX_CONNECTIONS * 2);
    if (!pool) {
        fprintf(stderr, "Failed to initialize thread pool\n");
        return 1;
    }
    
    printf("Thread pool initialized with 4 threads\n");
    
    /* Create socket */
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Failed to create socket");
        thread_pool_destroy(pool);
        return 1;
    }
    
    printf("Socket created\n");
    
    /* Set socket options */
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Failed to set socket options");
        close(server_socket);
        thread_pool_destroy(pool);
        return 1;
    }
    
    /* Set socket to non-blocking mode */
    int flags = fcntl(server_socket, F_GETFL, 0);
    fcntl(server_socket, F_SETFL, flags | O_NONBLOCK);
    
    /* Configure server address */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    /* Bind socket */
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to bind socket");
        close(server_socket);
        thread_pool_destroy(pool);
        return 1;
    }
    
    printf("Socket bound to port %d\n", port);
    
    /* Listen for connections */
    if (listen(server_socket, MAX_CONNECTIONS) < 0) {
        perror("Failed to listen on socket");
        close(server_socket);
        thread_pool_destroy(pool);
        return 1;
    }
    
    printf("Server listening on port %d\n", port);
    
    /* Accept loop */
    while (1) {
        /* Use select to wait for connections with timeout */
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);
        
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int select_result = select(server_socket + 1, &read_fds, NULL, NULL, &tv);
        
        if (select_result < 0) {
            perror("Select failed");
            continue;
        }
        
        if (select_result == 0) {
            /* Timeout, no connections available */
            continue;
        }
        
        /* Accept connection */
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_socket < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* No connections available on non-blocking socket */
                continue;
            }
            
            perror("Failed to accept connection");
            continue;
        }
        
        printf("Accepted connection from %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        /* Allocate memory for client socket */
        int* client_socket_ptr = malloc(sizeof(int));
        if (!client_socket_ptr) {
            perror("Failed to allocate memory");
            close(client_socket);
            continue;
        }
        
        *client_socket_ptr = client_socket;
        
        /* Add client handling task to thread pool */
        if (thread_pool_add(pool, handle_client, client_socket_ptr) != 0) {
            perror("Failed to add task to thread pool");
            free(client_socket_ptr);
            close(client_socket);
            continue;
        }
    }
    
    /* Clean up (never reached in this example) */
    close(server_socket);
    thread_pool_destroy(pool);
    
    return 0;
}