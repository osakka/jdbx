# Thread Pool Implementation

This document describes the thread pool implementation for the JDBX server, which is designed to efficiently handle multiple client connections in a scalable and resource-efficient manner.

## Overview

The thread pool provides a way to reuse threads for handling client connections, avoiding the overhead of creating and destroying threads for each connection. It also limits the number of concurrent threads, preventing resource exhaustion under high load.

## Architecture

The thread pool implementation follows a classic producer-consumer pattern with the following components:

1. **Thread Pool**: A central structure that manages worker threads and distributes work
2. **Work Queue**: A queue of tasks waiting to be processed
3. **Worker Threads**: Threads that process tasks from the work queue
4. **Work Items**: Individual tasks that need to be processed

## Key Features

- **Dynamic Sizing**: The thread pool can adjust its size based on load
- **Minimum and Maximum Threads**: Configurable limits to prevent resource exhaustion
- **Idle Thread Cleanup**: Threads beyond the minimum count will exit after being idle
- **Thread Safety**: All operations are thread-safe using mutexes and condition variables
- **Performance Metrics**: Collection of statistics on queue size, thread count, and tasks processed

## Components

### Thread Pool Structure

```c
typedef struct {
    pthread_mutex_t lock;       /* Mutex to protect the queue */
    pthread_cond_t work_cond;   /* Condition to signal work availability */
    pthread_cond_t idle_cond;   /* Condition to signal idle threads */
    
    work_item_t* work_head;     /* Head of work item queue */
    work_item_t* work_tail;     /* Tail of work item queue */
    int queue_size;             /* Current number of work items in the queue */
    int max_queue_size;         /* Maximum number of work items in the queue */
    
    pthread_t* threads;         /* Array of worker thread IDs */
    int thread_count;           /* Current number of threads in the pool */
    int min_threads;            /* Minimum number of threads in the pool */
    int max_threads;            /* Maximum number of threads in the pool */
    int active_threads;         /* Number of threads currently handling work */
    
    int idle_timeout;           /* Timeout in seconds for idle threads before termination */
    int shutdown;               /* Flag to indicate pool shutdown */
    
    /* Performance metrics */
    uint64_t tasks_processed;   /* Total number of tasks processed */
    uint64_t peak_queue_size;   /* Peak queue size observed */
    uint64_t peak_threads;      /* Peak number of threads observed */
} thread_pool_t;
```

### Work Item Structure

```c
typedef struct work_item {
    void (*function)(void*);    /* Function to execute */
    void* argument;             /* Function argument */
    struct work_item* next;     /* Next work item in queue */
} work_item_t;
```

### Configuration Structure

```c
typedef struct {
    int min_threads;            /* Minimum number of threads in the pool */
    int max_threads;            /* Maximum number of threads in the pool */
    int queue_size;             /* Maximum number of work items in the queue */
    int idle_timeout;           /* Timeout in seconds for idle threads before termination */
} thread_pool_config_t;
```

## Usage

### Creating a Thread Pool

```c
/* Create with default configuration */
thread_pool_t* pool = thread_pool_create();

/* Create with custom configuration */
thread_pool_config_t config = {
    .min_threads = 4,
    .max_threads = 16,
    .queue_size = 100,
    .idle_timeout = 60
};
thread_pool_t* pool = thread_pool_create_config(&config);
```

### Adding Work to the Thread Pool

```c
/* Define a task function */
void handle_client(void* client_data) {
    /* Process client connection */
}

/* Add a task to the thread pool */
client_conn_t* client = /* ... */;
thread_pool_add_work(pool, handle_client, client);
```

### Cleaning Up

```c
/* Wait for all tasks to complete */
thread_pool_wait(pool);

/* Destroy the thread pool */
thread_pool_destroy(pool);
```

## Worker Thread Logic

Worker threads follow this pattern:
1. Lock the thread pool
2. If there's no work, wait on the work condition variable
3. When work is available, dequeue a work item
4. Unlock the thread pool
5. Execute the work function
6. Repeat

If a thread remains idle beyond the idle timeout and the pool has more than the minimum number of threads, the idle thread will exit.

## Thread Pool Sizing

The thread pool dynamically adjusts its size based on load:
- It starts with the minimum number of threads
- If all threads are busy and there are tasks in the queue, it adds more threads up to the maximum
- If threads remain idle longer than the idle timeout, they exit (maintaining at least the minimum)

## Server Integration

The server uses the thread pool to handle client connections:
1. The main thread accepts connections on the server socket
2. When a connection is accepted, it creates a client connection structure
3. The client connection is added to the thread pool as a work item
4. A worker thread from the pool processes the client request
5. When the client request is complete, the worker thread is available for the next task

## Benefits

- **Improved Resource Utilization**: Threads are reused instead of created and destroyed for each connection
- **Better Scalability**: The thread pool can handle many concurrent connections with a limited number of threads
- **Reduced Overhead**: Thread creation is expensive, and the thread pool minimizes this cost
- **Controlled Resource Usage**: The thread pool prevents resource exhaustion under high load
- **Predictable Performance**: By limiting concurrency, the system behaves more predictably

## Testing

Testing the thread pool implementation involves:
1. Verifying that tasks are correctly processed
2. Confirming that threads are reused
3. Testing that the thread pool scales correctly under load
4. Ensuring that idle threads exit as expected
5. Checking that the performance metrics are accurate

## Conclusion

The thread pool implementation provides an efficient and scalable way to handle client connections in the JDBX server. It improves performance, resource utilization, and system stability under various load conditions.