#include "core/thread_pool.h"
#include "core/server.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <time.h>

/* Test task function */
void test_task(void* arg) {
    int task_id = *((int*)arg);
    pid_t tid = syscall(SYS_gettid);
    
    /* Get current time */
    time_t now;
    struct tm* tm_info;
    char time_str[26];
    
    time(&now);
    tm_info = localtime(&now);
    strftime(time_str, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    
    printf("[%s] Task %d running on thread ID %lu (system_tid=%d)\n", 
           time_str, task_id, (unsigned long)pthread_self(), tid);
    
    /* Simulate work with random duration */
    unsigned int sleep_time = rand() % 2000000 + 500000; /* 0.5-2.5 seconds */
    usleep(sleep_time);
    
    printf("[%s] Task %d completed (sleep time: %.2f seconds)\n", 
           time_str, task_id, sleep_time / 1000000.0);
    
    free(arg);
}

/* Simple HTTP client task function */
void http_task(void* arg) {
    int task_id = *((int*)arg);
    pid_t tid = syscall(SYS_gettid);
    
    /* Get current time */
    time_t now;
    struct tm* tm_info;
    char time_str[26];
    
    time(&now);
    tm_info = localtime(&now);
    strftime(time_str, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    
    printf("[%s] HTTP client %d simulated on thread ID %lu (system_tid=%d)\n", 
           time_str, task_id, (unsigned long)pthread_self(), tid);
    
    /* Simulate HTTP processing with random duration */
    unsigned int sleep_time = rand() % 1000000 + 200000; /* 0.2-1.2 seconds */
    usleep(sleep_time);
    
    printf("[%s] HTTP client %d completed (processing time: %.2f seconds)\n", 
           time_str, task_id, sleep_time / 1000000.0);
    
    free(arg);
}

int main(int argc, char* argv[]) {
    /* Parse command line arguments */
    int num_tasks = 20;  /* Default */
    int min_threads = 4;  /* Default */
    int max_threads = 10;  /* Default */
    
    if (argc > 1) {
        num_tasks = atoi(argv[1]);
    }
    
    if (argc > 2) {
        min_threads = atoi(argv[2]);
    }
    
    if (argc > 3) {
        max_threads = atoi(argv[3]);
    }
    
    /* Display test parameters */
    printf("Thread Pool Test\n");
    printf("===============\n");
    printf("Tasks: %d\n", num_tasks);
    printf("Min threads: %d\n", min_threads);
    printf("Max threads: %d\n", max_threads);
    printf("\n");
    
    /* Seed random number generator */
    srand(time(NULL));
    
    /* Create thread pool configuration */
    thread_pool_config_t config = {
        .min_threads = min_threads,
        .max_threads = max_threads,
        .queue_size = num_tasks * 2,
        .idle_timeout = 5  /* 5 seconds idle timeout */
    };
    
    /* Create thread pool */
    thread_pool_t* pool = thread_pool_create_config(&config);
    if (!pool) {
        fprintf(stderr, "Failed to create thread pool\n");
        return 1;
    }
    
    printf("Thread pool created successfully with %d-%d threads\n", min_threads, max_threads);
    
    /* Submit regular tasks */
    printf("\nSubmitting %d regular tasks...\n", num_tasks / 2);
    for (int i = 0; i < num_tasks / 2; i++) {
        int* task_id = malloc(sizeof(int));
        if (!task_id) {
            fprintf(stderr, "Failed to allocate memory for task ID\n");
            continue;
        }
        
        *task_id = i + 1;
        
        if (thread_pool_add_work(pool, test_task, task_id) != 0) {
            fprintf(stderr, "Failed to add task %d to thread pool\n", i + 1);
            free(task_id);
        } else {
            printf("Task %d added to thread pool\n", i + 1);
        }
        
        /* Slight delay between task submissions */
        usleep(100000);  /* 100ms */
    }
    
    /* Wait for a while to let some tasks complete */
    printf("\nWaiting for 3 seconds...\n");
    sleep(3);
    
    /* Get thread pool statistics */
    int active_threads = 0;
    int queue_size = 0;
    uint64_t tasks_processed = 0;
    
    thread_pool_stats(pool, &active_threads, &queue_size, &tasks_processed);
    
    printf("\nThread pool statistics after first batch:\n");
    printf("Active threads: %d\n", active_threads);
    printf("Queue size: %d\n", queue_size);
    printf("Tasks processed: %llu\n", (unsigned long long)tasks_processed);
    
    /* Submit HTTP client tasks */
    printf("\nSubmitting %d HTTP client tasks...\n", num_tasks / 2);
    for (int i = 0; i < num_tasks / 2; i++) {
        int* task_id = malloc(sizeof(int));
        if (!task_id) {
            fprintf(stderr, "Failed to allocate memory for task ID\n");
            continue;
        }
        
        *task_id = i + 1;
        
        if (thread_pool_add_work(pool, http_task, task_id) != 0) {
            fprintf(stderr, "Failed to add HTTP client %d to thread pool\n", i + 1);
            free(task_id);
        } else {
            printf("HTTP client %d added to thread pool\n", i + 1);
        }
        
        /* No delay between HTTP task submissions to simulate burst traffic */
    }
    
    /* Wait for all tasks to complete */
    printf("\nWaiting for all tasks to complete...\n");
    thread_pool_wait(pool);
    
    /* Get final thread pool statistics */
    thread_pool_stats(pool, &active_threads, &queue_size, &tasks_processed);
    
    printf("\nFinal thread pool statistics:\n");
    printf("Active threads: %d\n", active_threads);
    printf("Queue size: %d\n", queue_size);
    printf("Tasks processed: %llu\n", (unsigned long long)tasks_processed);
    
    /* Destroy thread pool */
    thread_pool_destroy(pool);
    
    printf("\nThread pool test completed successfully\n");
    
    return 0;
}