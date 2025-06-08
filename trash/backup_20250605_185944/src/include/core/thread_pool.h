#ifndef JSONDB_THREAD_POOL_H
#define JSONDB_THREAD_POOL_H

#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>

/**
 * Work item structure for the thread pool
 */
typedef struct work_item {
    void (*function)(void*);    /* Function to execute */
    void* argument;             /* Function argument */
    struct work_item* next;     /* Next work item in queue */
} work_item_t;

/**
 * Thread pool configuration options
 */
typedef struct {
    int min_threads;            /* Minimum number of threads in the pool */
    int max_threads;            /* Maximum number of threads in the pool */
    int queue_size;             /* Maximum number of work items in the queue */
    int idle_timeout;           /* Timeout in seconds for idle threads before termination */
} thread_pool_config_t;

/**
 * Thread pool structure
 */
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

/**
 * Create a new thread pool with default configuration
 * 
 * @return A new thread pool or NULL on failure
 */
thread_pool_t* thread_pool_create(void);

/**
 * Create a new thread pool with custom configuration
 * 
 * @param config Thread pool configuration
 * @return A new thread pool or NULL on failure
 */
thread_pool_t* thread_pool_create_config(thread_pool_config_t* config);

/**
 * Add a work item to the thread pool
 * 
 * @param pool Thread pool
 * @param function Function to execute
 * @param argument Argument to pass to the function
 * @return 0 on success, non-zero on failure
 */
int thread_pool_add_work(thread_pool_t* pool, void (*function)(void*), void* argument);

/**
 * Wait for all work items to be processed
 * 
 * @param pool Thread pool
 */
void thread_pool_wait(thread_pool_t* pool);

/**
 * Get current statistics for the thread pool
 * 
 * @param pool Thread pool
 * @param active_threads Pointer to store number of active threads
 * @param queue_size Pointer to store current queue size
 * @param tasks_processed Pointer to store number of tasks processed
 */
void thread_pool_stats(thread_pool_t* pool, int* active_threads, int* queue_size, uint64_t* tasks_processed);

/**
 * Destroy a thread pool
 * 
 * @param pool Thread pool
 */
void thread_pool_destroy(thread_pool_t* pool);

/**
 * Adjust the number of threads in the pool
 * 
 * @param pool Thread pool
 * @param min_threads New minimum number of threads
 * @param max_threads New maximum number of threads
 * @return 0 on success, non-zero on failure
 */
int thread_pool_adjust(thread_pool_t* pool, int min_threads, int max_threads);

#endif /* JSONDB_THREAD_POOL_H */