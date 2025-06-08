/**
 * Lock-free work queue implementation for JSONdb
 * 
 * Phase 3 optimization: Lock-free concurrency for maximum throughput
 * Uses atomic operations and memory barriers for thread-safe operations
 * without locking overhead.
 */

#include "core/lockfree_queue.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Queue node structure with atomic next pointer */
typedef struct queue_node {
    _Atomic(struct queue_node*) next;
    work_item_t work;
} queue_node_t;

/* Lock-free queue structure */
struct lockfree_queue {
    _Atomic(queue_node_t*) head;
    _Atomic(queue_node_t*) tail;
    _Atomic(size_t) size;
    _Atomic(uint64_t) enqueue_count;
    _Atomic(uint64_t) dequeue_count;
    size_t max_size;
    
    /* Memory pool for nodes to reduce allocation overhead */
    _Atomic(queue_node_t*) free_nodes;
    size_t node_pool_size;
    _Atomic(size_t) nodes_allocated;
};

/* Forward declarations */
static queue_node_t* allocate_node(lockfree_queue_t* queue);
static void free_node(lockfree_queue_t* queue, queue_node_t* node);
static void cleanup_node_pool(lockfree_queue_t* queue);

/**
 * Create a new lock-free queue
 * 
 * @param max_size Maximum queue size (0 = unlimited)
 * @param node_pool_size Initial node pool size for performance
 * @return New queue or NULL on failure
 */
lockfree_queue_t* lockfree_queue_create(size_t max_size, size_t node_pool_size) {
    lockfree_queue_t* queue = buffer_pool_alloc(sizeof(lockfree_queue_t));
    if (!queue) {
        return NULL;
    }
    
    /* Create sentinel node for lock-free operations */
    queue_node_t* sentinel = buffer_pool_alloc(sizeof(queue_node_t));
    if (!sentinel) {
        buffer_pool_free(queue);
        return NULL;
    }
    
    /* Initialize sentinel node */
    atomic_store(&sentinel->next, NULL);
    memset(&sentinel->work, 0, sizeof(work_item_t));
    
    /* Initialize queue state */
    atomic_store(&queue->head, sentinel);
    atomic_store(&queue->tail, sentinel);
    atomic_store(&queue->size, 0);
    atomic_store(&queue->enqueue_count, 0);
    atomic_store(&queue->dequeue_count, 0);
    queue->max_size = max_size;
    
    /* Initialize node pool */
    atomic_store(&queue->free_nodes, NULL);
    queue->node_pool_size = node_pool_size > 0 ? node_pool_size : 32;
    atomic_store(&queue->nodes_allocated, 1); /* Sentinel node */
    
    /* Pre-allocate node pool for performance */
    for (size_t i = 0; i < queue->node_pool_size; i++) {
        queue_node_t* node = buffer_pool_alloc(sizeof(queue_node_t));
        if (node) {
            atomic_store(&node->next, atomic_load(&queue->free_nodes));
            atomic_store(&queue->free_nodes, node);
            atomic_fetch_add(&queue->nodes_allocated, 1);
        }
    }
    
    if (g_logger) {
        LOG_DEBUG("Created lock-free queue (max_size=%zu, pool_size=%zu)", 
                  max_size, queue->node_pool_size);
    }
    
    return queue;
}

/**
 * Destroy lock-free queue and free all resources
 * 
 * @param queue Queue to destroy
 */
void lockfree_queue_destroy(lockfree_queue_t* queue) {
    if (!queue) return;
    
    /* Drain all remaining work items */
    work_item_t item;
    while (lockfree_queue_dequeue(queue, &item)) {
        /* Work items are drained but not executed during cleanup */
    }
    
    /* Clean up node pool */
    cleanup_node_pool(queue);
    
    /* Free sentinel node */
    queue_node_t* sentinel = atomic_load(&queue->head);
    if (sentinel) {
        buffer_pool_free(sentinel);
    }
    
    size_t final_size = atomic_load(&queue->size);
    uint64_t enqueues = atomic_load(&queue->enqueue_count);
    uint64_t dequeues = atomic_load(&queue->dequeue_count);
    
    if (g_logger) {
        LOG_DEBUG("Destroyed lock-free queue (final_size=%zu, enqueues=%lu, dequeues=%lu)", 
                  final_size, enqueues, dequeues);
    }
    
    buffer_pool_free(queue);
}

/**
 * Enqueue work item (lock-free, thread-safe)
 * 
 * @param queue Queue to enqueue to
 * @param item Work item to enqueue
 * @return 1 on success, 0 on failure (queue full)
 */
int lockfree_queue_enqueue(lockfree_queue_t* queue, const work_item_t* item) {
    if (!queue || !item) return 0;
    
    /* Check size limit (soft check - may occasionally exceed due to race conditions) */
    if (queue->max_size > 0 && atomic_load(&queue->size) >= queue->max_size) {
        return 0;
    }
    
    /* Allocate new node */
    queue_node_t* node = allocate_node(queue);
    if (!node) {
        return 0;
    }
    
    /* Initialize node */
    atomic_store(&node->next, NULL);
    node->work = *item;
    
    /* Michael & Scott lock-free enqueue algorithm */
    while (1) {
        queue_node_t* tail = atomic_load(&queue->tail);
        queue_node_t* next = atomic_load(&tail->next);
        
        /* Check if tail is still consistent */
        if (tail == atomic_load(&queue->tail)) {
            if (next == NULL) {
                /* Tail is pointing to the last node, try to link new node */
                if (atomic_compare_exchange_weak(&tail->next, &next, node)) {
                    /* Enqueue successful, try to advance tail pointer */
                    atomic_compare_exchange_weak(&queue->tail, &tail, node);
                    break;
                }
            } else {
                /* Tail is not pointing to the last node, help advance it */
                atomic_compare_exchange_weak(&queue->tail, &tail, next);
            }
        }
    }
    
    /* Update counters */
    atomic_fetch_add(&queue->size, 1);
    atomic_fetch_add(&queue->enqueue_count, 1);
    
    return 1;
}

/**
 * Dequeue work item (lock-free, thread-safe)
 * 
 * @param queue Queue to dequeue from
 * @param item Output work item
 * @return 1 if item dequeued, 0 if queue empty
 */
int lockfree_queue_dequeue(lockfree_queue_t* queue, work_item_t* item) {
    if (!queue || !item) return 0;
    
    /* Michael & Scott lock-free dequeue algorithm */
    while (1) {
        queue_node_t* head = atomic_load(&queue->head);
        queue_node_t* tail = atomic_load(&queue->tail);
        queue_node_t* next = atomic_load(&head->next);
        
        /* Check if head is still consistent */
        if (head == atomic_load(&queue->head)) {
            if (head == tail) {
                if (next == NULL) {
                    /* Queue is empty */
                    return 0;
                } else {
                    /* Tail is falling behind, help advance it */
                    atomic_compare_exchange_weak(&queue->tail, &tail, next);
                }
            } else {
                if (next == NULL) {
                    /* Inconsistent state, try again */
                    continue;
                }
                
                /* Read data before potential dequeue */
                *item = next->work;
                
                /* Try to advance head pointer */
                if (atomic_compare_exchange_weak(&queue->head, &head, next)) {
                    /* Dequeue successful */
                    free_node(queue, head);
                    break;
                }
            }
        }
    }
    
    /* Update counters */
    atomic_fetch_sub(&queue->size, 1);
    atomic_fetch_add(&queue->dequeue_count, 1);
    
    return 1;
}

/**
 * Get current queue size (approximate due to concurrent operations)
 * 
 * @param queue Queue to check
 * @return Current size
 */
size_t lockfree_queue_size(lockfree_queue_t* queue) {
    if (!queue) return 0;
    return atomic_load(&queue->size);
}

/**
 * Check if queue is empty (approximate due to concurrent operations)
 * 
 * @param queue Queue to check
 * @return 1 if empty, 0 if not empty
 */
int lockfree_queue_empty(lockfree_queue_t* queue) {
    if (!queue) return 1;
    return atomic_load(&queue->size) == 0;
}

/**
 * Get queue statistics
 * 
 * @param queue Queue to get stats for
 * @param stats Output statistics structure
 */
void lockfree_queue_get_stats(lockfree_queue_t* queue, queue_stats_t* stats) {
    if (!queue || !stats) return;
    
    stats->current_size = atomic_load(&queue->size);
    stats->max_size = queue->max_size;
    stats->total_enqueues = atomic_load(&queue->enqueue_count);
    stats->total_dequeues = atomic_load(&queue->dequeue_count);
    stats->nodes_allocated = atomic_load(&queue->nodes_allocated);
    stats->node_pool_size = queue->node_pool_size;
}

/**
 * Allocate a node from the pool or create new one
 */
static queue_node_t* allocate_node(lockfree_queue_t* queue) {
    /* Try to get node from free pool first */
    queue_node_t* node = atomic_load(&queue->free_nodes);
    while (node) {
        queue_node_t* next = atomic_load(&node->next);
        if (atomic_compare_exchange_weak(&queue->free_nodes, &node, next)) {
            /* Successfully got node from pool */
            return node;
        }
        /* CAS failed, try again */
        node = atomic_load(&queue->free_nodes);
    }
    
    /* Pool is empty, allocate new node */
    node = buffer_pool_alloc(sizeof(queue_node_t));
    if (node) {
        atomic_fetch_add(&queue->nodes_allocated, 1);
    }
    
    return node;
}

/**
 * Return a node to the pool or free it
 */
static void free_node(lockfree_queue_t* queue, queue_node_t* node) {
    if (!node) return;
    
    /* Clear the node data */
    memset(&node->work, 0, sizeof(work_item_t));
    
    /* Try to return to pool */
    queue_node_t* free_head = atomic_load(&queue->free_nodes);
    do {
        atomic_store(&node->next, free_head);
    } while (!atomic_compare_exchange_weak(&queue->free_nodes, &free_head, node));
}

/**
 * Clean up the node pool
 */
static void cleanup_node_pool(lockfree_queue_t* queue) {
    queue_node_t* node = atomic_load(&queue->free_nodes);
    
    while (node) {
        queue_node_t* next = atomic_load(&node->next);
        buffer_pool_free(node);
        node = next;
    }
    
    atomic_store(&queue->free_nodes, NULL);
}

/**
 * Drain all items from queue without executing them
 * Useful for graceful shutdown
 * 
 * @param queue Queue to drain
 * @return Number of items drained
 */
size_t lockfree_queue_drain(lockfree_queue_t* queue) {
    if (!queue) return 0;
    
    size_t drained = 0;
    work_item_t item;
    
    while (lockfree_queue_dequeue(queue, &item)) {
        drained++;
    }
    
    if (g_logger && drained > 0) {
        LOG_DEBUG("Drained %zu items from lock-free queue", drained);
    }
    
    return drained;
}