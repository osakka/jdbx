#ifndef LSM_TREE_H
#define LSM_TREE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>
#include "utils/skiplist.h"
#include "storage/mmap_storage.h"
#include "index/btree_disk.h"

/* Log-Structured Merge Tree for write-optimized storage
 * Optimized for high write throughput with background compaction
 */

#define LSM_MAX_LEVELS 7
#define LSM_LEVEL_RATIO 10
#define LSM_MEMTABLE_SIZE (64 * 1024 * 1024)  /* 64MB */
#define LSM_BLOOM_BITS_PER_KEY 10

/* SSTable (Sorted String Table) structure */
typedef struct sstable {
    uint64_t id;
    uint32_t level;
    mmap_storage_t* storage;
    
    /* Metadata */
    uint64_t num_entries;
    uint64_t file_size;
    uint64_t min_timestamp;
    uint64_t max_timestamp;
    
    /* Key range for quick filtering */
    void* min_key;
    size_t min_key_len;
    void* max_key;
    size_t max_key_len;
    
    /* Bloom filter for existence checks */
    struct bloom_filter* filter;
    
    /* Index for binary search */
    btree_disk_t* index;
    
    /* Reference count for safe deletion */
    _Atomic int ref_count;
} sstable_t;

/* Compaction strategy */
typedef enum compaction_strategy {
    COMPACTION_LEVELED,      /* Leveled compaction (RocksDB-style) */
    COMPACTION_TIERED,       /* Size-tiered compaction */
    COMPACTION_UNIFIED       /* Unified compaction */
} compaction_strategy_t;

/* Write-ahead log */
typedef struct wal {
    int fd;
    char* path;
    uint64_t size;
    uint64_t sync_offset;
    pthread_mutex_t lock;
} wal_t;

/* LSM tree structure */
typedef struct lsm_tree {
    char* base_path;
    
    /* In-memory components */
    skiplist_t* memtable;          /* Active memtable */
    skiplist_t* immutable_memtable; /* Being flushed */
    _Atomic size_t memtable_size;
    pthread_rwlock_t memtable_lock;
    
    /* On-disk components */
    sstable_t*** levels;           /* Array of SSTables per level */
    size_t* level_counts;          /* Number of SSTables per level */
    size_t* level_capacities;      /* Capacity per level */
    pthread_rwlock_t* level_locks;
    
    /* Write-ahead log */
    wal_t* wal;
    
    /* Compaction */
    compaction_strategy_t strategy;
    pthread_t compaction_thread;
    pthread_cond_t compaction_cond;
    pthread_mutex_t compaction_mutex;
    _Atomic bool compacting[LSM_MAX_LEVELS];
    _Atomic bool shutdown;
    
    /* Key comparison function */
    int (*compare)(const void* a, size_t a_len, const void* b, size_t b_len);
    
    /* Statistics */
    _Atomic uint64_t writes;
    _Atomic uint64_t reads;
    _Atomic uint64_t flushes;
    _Atomic uint64_t compactions;
    _Atomic uint64_t bloom_hits;
    _Atomic uint64_t bloom_misses;
} lsm_tree_t;

/* LSM iterator for merging multiple sources */
typedef struct lsm_iterator {
    lsm_tree_t* tree;
    
    /* Current position in each level */
    struct {
        skiplist_iterator_t* iter;     /* For memtables */
        btree_cursor_t** cursors;      /* For SSTables */
        size_t cursor_count;
        size_t current_cursor;
    } levels[LSM_MAX_LEVELS + 2];      /* +2 for memtables */
    
    /* Merge heap for selecting minimum key */
    struct {
        void* key;
        size_t key_len;
        void* value;
        size_t value_len;
        uint64_t timestamp;
        int source_level;
        int source_index;
    }* heap;
    size_t heap_size;
    size_t heap_capacity;
} lsm_iterator_t;

/* Bloom filter structure */
struct bloom_filter {
    uint8_t* bits;
    size_t size_bytes;
    uint32_t num_hashes;
};

/* Create and destroy LSM tree */
lsm_tree_t* lsm_tree_create(const char* path,
                           int (*compare)(const void*, size_t, const void*, size_t));
void lsm_tree_destroy(lsm_tree_t* tree);

/* Basic operations */
int lsm_tree_put(lsm_tree_t* tree, const void* key, size_t key_len,
                const void* value, size_t value_len);
int lsm_tree_get(lsm_tree_t* tree, const void* key, size_t key_len,
                void** value, size_t* value_len);
int lsm_tree_delete(lsm_tree_t* tree, const void* key, size_t key_len);

/* Batch operations */
typedef struct lsm_batch {
    struct batch_entry {
        enum { LSM_PUT, LSM_DELETE } type;
        void* key;
        size_t key_len;
        void* value;
        size_t value_len;
    }* entries;
    size_t count;
    size_t capacity;
} lsm_batch_t;

lsm_batch_t* lsm_batch_create(void);
void lsm_batch_destroy(lsm_batch_t* batch);
void lsm_batch_put(lsm_batch_t* batch, const void* key, size_t key_len,
                  const void* value, size_t value_len);
void lsm_batch_delete(lsm_batch_t* batch, const void* key, size_t key_len);
int lsm_tree_write_batch(lsm_tree_t* tree, lsm_batch_t* batch);

/* Iterator interface */
lsm_iterator_t* lsm_iterator_create(lsm_tree_t* tree);
void lsm_iterator_destroy(lsm_iterator_t* iter);
void lsm_iterator_seek(lsm_iterator_t* iter, const void* key, size_t key_len);
void lsm_iterator_seek_first(lsm_iterator_t* iter);
bool lsm_iterator_valid(lsm_iterator_t* iter);
void lsm_iterator_next(lsm_iterator_t* iter);
void lsm_iterator_get(lsm_iterator_t* iter, 
                     void** key, size_t* key_len,
                     void** value, size_t* value_len);

/* Maintenance operations */
int lsm_tree_flush(lsm_tree_t* tree);
int lsm_tree_compact_level(lsm_tree_t* tree, int level);
int lsm_tree_compact_all(lsm_tree_t* tree);

/* Statistics */
typedef struct lsm_stats {
    uint64_t total_keys;
    uint64_t total_size;
    uint64_t num_sstables;
    uint64_t memtable_keys;
    size_t memtable_size;
    
    struct {
        size_t count;
        uint64_t total_keys;
        uint64_t total_size;
    } levels[LSM_MAX_LEVELS];
    
    uint64_t writes;
    uint64_t reads;
    uint64_t flushes;
    uint64_t compactions;
} lsm_stats_t;

void lsm_tree_stats(lsm_tree_t* tree, lsm_stats_t* stats);

/* SSTable operations */
sstable_t* sstable_create(const char* path, uint32_t level);
void sstable_destroy(sstable_t* sstable);
int sstable_add_ref(sstable_t* sstable);
int sstable_release(sstable_t* sstable);

/* Bloom filter operations */
struct bloom_filter* bloom_create(size_t expected_items);
void bloom_destroy(struct bloom_filter* filter);
void bloom_add(struct bloom_filter* filter, const void* key, size_t len);
bool bloom_may_contain(struct bloom_filter* filter, const void* key, size_t len);

/* Compaction helpers */
sstable_t* compact_sstables(sstable_t** tables, size_t count, 
                           uint32_t target_level,
                           int (*compare)(const void*, size_t, const void*, size_t));

#endif /* LSM_TREE_H */