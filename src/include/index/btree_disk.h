#ifndef BTREE_DISK_H
#define BTREE_DISK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>
#include "storage/mmap_storage.h"
#include "utils/skiplist.h"
#include "utils/cache.h"

/* Disk-based B+tree for efficient range queries and sorted access
 * Optimized for SSD with 4KB pages
 */

#define BTREE_PAGE_SIZE 4096
#define BTREE_DEFAULT_ORDER 200  /* ~200 keys per node for 4KB pages */
#define BTREE_CACHE_SIZE 10000   /* Cache 10K nodes = 40MB */

/* B+tree node types */
typedef enum btree_node_type {
    BTREE_LEAF = 0,
    BTREE_INTERNAL = 1
} btree_node_type_t;

/* On-disk B+tree node structure */
typedef struct btree_node {
    uint16_t type;          /* Node type */
    uint16_t num_keys;      /* Number of keys in this node */
    uint32_t level;         /* Level in tree (leaves = 0) */
    uint64_t page_id;       /* Unique page identifier */
    
    /* Parent pointer for traversal */
    uint64_t parent_page_id;
    
    /* Sibling pointers for range scans */
    uint64_t prev_page_id;
    uint64_t next_page_id;
    
    /* Keys - variable length encoded */
    uint32_t key_offsets[BTREE_DEFAULT_ORDER];
    uint32_t key_lengths[BTREE_DEFAULT_ORDER];
    
    /* For internal nodes: child page IDs */
    /* For leaf nodes: value offsets in data file */
    uint64_t pointers[BTREE_DEFAULT_ORDER + 1];
    
    /* Variable length key data follows */
    uint8_t key_data[];
} btree_node_t;

/* In-memory write buffer entry */
typedef struct write_buffer_entry {
    void* key;
    size_t key_len;
    uint64_t value_offset;
    enum { WB_INSERT, WB_DELETE } type;
} write_buffer_entry_t;

/* B+tree structure */
typedef struct btree_disk {
    /* Storage backend */
    mmap_storage_t* storage;
    
    /* Tree metadata */
    uint64_t root_page_id;
    uint32_t order;
    uint64_t height;
    uint64_t num_keys;
    
    /* Node cache for hot nodes */
    cache_t* node_cache;
    
    /* Write buffer for batching updates */
    skiplist_t* write_buffer;
    size_t buffer_size_limit;
    _Atomic size_t current_buffer_size;
    pthread_mutex_t flush_mutex;
    
    /* Key comparison function */
    int (*compare)(const void* a, size_t a_len, const void* b, size_t b_len);
    
    /* Statistics */
    _Atomic uint64_t cache_hits;
    _Atomic uint64_t cache_misses;
    _Atomic uint64_t splits;
    _Atomic uint64_t merges;
} btree_disk_t;

/* Cursor for tree traversal */
typedef struct btree_cursor {
    btree_disk_t* tree;
    btree_node_t* current_node;
    uint16_t current_index;
    bool end_of_tree;
    
    /* Path from root to current position */
    uint64_t* path_pages;
    uint16_t* path_indexes;
    uint32_t path_length;
} btree_cursor_t;

/* Create and destroy B+tree */
btree_disk_t* btree_disk_create(const char* path, uint32_t order,
                               int (*compare)(const void*, size_t, const void*, size_t));
void btree_disk_destroy(btree_disk_t* tree);

/* Basic operations */
int btree_disk_insert(btree_disk_t* tree, const void* key, size_t key_len,
                     uint64_t value_offset);
int btree_disk_delete(btree_disk_t* tree, const void* key, size_t key_len);
int btree_disk_search(btree_disk_t* tree, const void* key, size_t key_len,
                     uint64_t* value_offset);

/* Batch operations */
int btree_disk_bulk_load(btree_disk_t* tree, 
                        const void** keys, const size_t* key_lens,
                        const uint64_t* value_offsets, size_t count);

/* Range operations */
btree_cursor_t* btree_cursor_create(btree_disk_t* tree);
void btree_cursor_destroy(btree_cursor_t* cursor);

int btree_cursor_seek(btree_cursor_t* cursor, const void* key, size_t key_len);
int btree_cursor_seek_first(btree_cursor_t* cursor);
int btree_cursor_seek_last(btree_cursor_t* cursor);

bool btree_cursor_next(btree_cursor_t* cursor, 
                      void** key, size_t* key_len, 
                      uint64_t* value_offset);
bool btree_cursor_prev(btree_cursor_t* cursor,
                      void** key, size_t* key_len,
                      uint64_t* value_offset);

/* Range scan with callback */
typedef void (*btree_scan_callback)(const void* key, size_t key_len,
                                   uint64_t value_offset, void* context);

void btree_disk_range_scan(btree_disk_t* tree,
                          const void* start_key, size_t start_len,
                          const void* end_key, size_t end_len,
                          btree_scan_callback callback, void* context);

/* Maintenance operations */
int btree_disk_flush_buffer(btree_disk_t* tree);
int btree_disk_compact(btree_disk_t* tree);
void btree_disk_stats(btree_disk_t* tree, uint64_t* height, uint64_t* num_keys,
                     uint64_t* cache_hits, uint64_t* cache_misses);

/* Internal helpers */
static inline size_t btree_node_size(btree_node_t* node) {
    size_t size = sizeof(btree_node_t);
    for (uint16_t i = 0; i < node->num_keys; i++) {
        size += node->key_lengths[i];
    }
    return size;
}

static inline void* btree_node_get_key(btree_node_t* node, uint16_t index) {
    if (index >= node->num_keys) return NULL;
    return node->key_data + node->key_offsets[index];
}

#endif /* BTREE_DISK_H */