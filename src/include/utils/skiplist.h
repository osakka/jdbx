#ifndef SKIPLIST_H
#define SKIPLIST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include "utils/hazard_pointer.h"
#include "utils/buffer_pool.h"
#include "utils/memory_manager.h"

/* Lock-free skip list for high-performance indexing
 * Supports concurrent reads and writes without locks
 */

#define SKIPLIST_MAX_LEVEL 32
#define SKIPLIST_P 0.5

/* Skip list node */
typedef struct skiplist_node {
    void* key;
    void* value;
    size_t key_len;
    size_t value_len;
    
    /* Atomic next pointers for each level - ensure proper alignment */
    _Alignas(sizeof(_Atomic(struct skiplist_node*))) _Atomic(struct skiplist_node*) next[1]; /* Variable length array */
} skiplist_node_t;

/* Skip list structure */
typedef struct skiplist {
    /* Head node (sentinel) */
    skiplist_node_t* head;
    
    /* Current maximum level - aligned for atomic access */
    _Alignas(sizeof(_Atomic int)) _Atomic int level;
    
    /* Comparison function */
    int (*compare)(const void* a, size_t a_len, const void* b, size_t b_len);
    
    /* Hazard pointer domain for safe reclamation */
    hp_domain_t* hp_domain;
    
    /* Statistics - aligned for atomic access */
    _Alignas(sizeof(_Atomic uint64_t)) _Atomic uint64_t size;
    _Alignas(sizeof(_Atomic uint64_t)) _Atomic uint64_t insert_count;
    _Alignas(sizeof(_Atomic uint64_t)) _Atomic uint64_t delete_count;
    _Alignas(sizeof(_Atomic uint64_t)) _Atomic uint64_t search_count;
} skiplist_t;

/* Create and destroy skip list */
skiplist_t* skiplist_create(int (*compare)(const void*, size_t, const void*, size_t));
void skiplist_destroy(skiplist_t* list);

/* Basic operations */
bool skiplist_insert(skiplist_t* list, const void* key, size_t key_len,
                    const void* value, size_t value_len);
bool skiplist_delete(skiplist_t* list, const void* key, size_t key_len);
void* skiplist_search(skiplist_t* list, const void* key, size_t key_len,
                     size_t* value_len);

/* Range operations */
typedef struct skiplist_iterator {
    skiplist_t* list;
    skiplist_node_t* current;
    hp_record_t* hp_record;
} skiplist_iterator_t;

skiplist_iterator_t* skiplist_iterator_create(skiplist_t* list);
void skiplist_iterator_destroy(skiplist_iterator_t* iter);
bool skiplist_iterator_next(skiplist_iterator_t* iter, 
                           void** key, size_t* key_len,
                           void** value, size_t* value_len);

/* Range scan */
typedef void (*skiplist_scan_callback)(const void* key, size_t key_len,
                                      const void* value, size_t value_len,
                                      void* context);

void skiplist_range_scan(skiplist_t* list,
                        const void* start_key, size_t start_len,
                        const void* end_key, size_t end_len,
                        skiplist_scan_callback callback, void* context);

/* Utility functions */
size_t skiplist_size(skiplist_t* list);
void skiplist_stats(skiplist_t* list, uint64_t* inserts, 
                   uint64_t* deletes, uint64_t* searches);

/* Internal helpers - not for direct use */
static inline skiplist_node_t* skiplist_create_node(int level, 
                                                   const void* key, size_t key_len,
                                                   const void* value, size_t value_len) {
    size_t node_size = sizeof(skiplist_node_t) + 
                      sizeof(_Atomic(skiplist_node_t*)) * (level - 1);
    skiplist_node_t* node = BUFFER_ALLOC(node_size);
    if (!node) return NULL;
    
    /* CRITICAL: Skiplist nodes must survive checkpoint rewinds as they are
     * part of persistent data structures. Promote immediately after allocation. */
    memory_promote(node);
    
    /* Handle NULL key/value (for sentinel nodes) */
    if (key && key_len > 0) {
        node->key = BUFFER_ALLOC(key_len);
        if (!node->key) {
            BUFFER_FREE(node);
            return NULL;
        }
        memory_promote(node->key);  // Promote key storage
        memcpy(node->key, key, key_len);
    } else {
        node->key = NULL;
    }
    
    if (value && value_len > 0) {
        node->value = BUFFER_ALLOC(value_len);
        if (!node->value) {
            BUFFER_FREE(node->key);
            BUFFER_FREE(node);
            return NULL;
        }
        memory_promote(node->value);  // Promote value storage
        memcpy(node->value, value, value_len);
    } else {
        node->value = NULL;
    }
    
    node->key_len = key_len;
    node->value_len = value_len;
    
    /* Initialize next pointers */
    for (int i = 0; i < level; i++) {
        atomic_init(&node->next[i], NULL);
    }
    
    return node;
}

static inline void skiplist_free_node(void* ptr) {
    skiplist_node_t* node = (skiplist_node_t*)ptr;
    BUFFER_FREE(node->key);
    BUFFER_FREE(node->value);
    BUFFER_FREE(node);
}

#endif /* SKIPLIST_H */