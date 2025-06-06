/**
 * Lock-free work queue for JSONdb thread pool
 * 
 * Phase 3 optimization: Lock-free concurrency for maximum throughput
 */

#ifndef LOCKFREE_QUEUE_H
#define LOCKFREE_QUEUE_H

#include <stddef.h>
#include <stdint.h>

/* Work item structure for the lock-free queue */
typedef struct {
    void (*function)(void* data);
    void* data;
    size_t data_size;
} work_item_t;

/* Opaque queue structure */
typedef struct lockfree_queue lockfree_queue_t;

/* Queue statistics */
typedef struct {
    size_t current_size;
    size_t max_size;
    uint64_t total_enqueues;
    uint64_t total_dequeues;
    size_t nodes_allocated;
    size_t node_pool_size;
} queue_stats_t;

/**
 * Create a new lock-free queue
 * 
 * @param max_size Maximum queue size (0 = unlimited)
 * @param node_pool_size Initial node pool size for performance
 * @return New queue or NULL on failure
 */
lockfree_queue_t* lockfree_queue_create(size_t max_size, size_t node_pool_size);

/**
 * Destroy lock-free queue and free all resources
 * 
 * @param queue Queue to destroy
 */
void lockfree_queue_destroy(lockfree_queue_t* queue);

/**
 * Enqueue work item (lock-free, thread-safe)
 * 
 * @param queue Queue to enqueue to
 * @param item Work item to enqueue
 * @return 1 on success, 0 on failure (queue full)
 */
int lockfree_queue_enqueue(lockfree_queue_t* queue, const work_item_t* item);

/**
 * Dequeue work item (lock-free, thread-safe)
 * 
 * @param queue Queue to dequeue from
 * @param item Output work item
 * @return 1 if item dequeued, 0 if queue empty
 */
int lockfree_queue_dequeue(lockfree_queue_t* queue, work_item_t* item);

/**
 * Get current queue size (approximate due to concurrent operations)
 * 
 * @param queue Queue to check
 * @return Current size
 */
size_t lockfree_queue_size(lockfree_queue_t* queue);

/**
 * Check if queue is empty (approximate due to concurrent operations)
 * 
 * @param queue Queue to check
 * @return 1 if empty, 0 if not empty
 */
int lockfree_queue_empty(lockfree_queue_t* queue);

/**
 * Get queue statistics
 * 
 * @param queue Queue to get stats for
 * @param stats Output statistics structure
 */
void lockfree_queue_get_stats(lockfree_queue_t* queue, queue_stats_t* stats);

/**
 * Drain all items from queue without executing them
 * Useful for graceful shutdown
 * 
 * @param queue Queue to drain
 * @return Number of items drained
 */
size_t lockfree_queue_drain(lockfree_queue_t* queue);

#endif /* LOCKFREE_QUEUE_H */