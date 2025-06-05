#ifndef MMAP_STORAGE_H
#define MMAP_STORAGE_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <sys/mman.h>
#include <stdatomic.h>

/* Memory-mapped storage for ultra-high performance
 * Targets: 1B+ documents, <100μs latency
 */

/* Storage magic and version */
#define MMAP_MAGIC 0x4D4D4150  /* "MMAP" */
#define MMAP_VERSION 1

/* Default sizes and limits */
#define DEFAULT_MMAP_SIZE (1ULL << 24)      /* 16MB initial */
#define MAX_MMAP_SIZE (1ULL << 40)          /* 1TB max */
#define PARTITION_COUNT 1024                 /* Default partitions */
#define PAGE_SIZE 4096                       /* OS page size */
#define CACHE_LINE_SIZE 64                   /* CPU cache line */

/* Align to cache line to prevent false sharing */
#define CACHE_ALIGNED __attribute__((aligned(CACHE_LINE_SIZE)))

/* Storage header - persisted to disk */
typedef struct CACHE_ALIGNED storage_header {
    uint32_t magic;
    uint32_t version;
    uint64_t file_size;
    uint64_t data_offset;
    uint64_t index_offset;
    uint64_t free_offset;
    
    /* Metadata */
    uint64_t doc_count;
    uint64_t deleted_count;
    uint64_t last_compact_time;
    uint64_t checksum;
    
    /* Partition info */
    uint32_t partition_count;
    uint32_t partition_size;
    
    /* Reserved for future use */
    uint8_t reserved[PAGE_SIZE - 88];
} storage_header_t;

/* Free space entry */
typedef struct free_entry {
    uint64_t offset;
    uint64_t size;
    struct free_entry* next;
} free_entry_t;

/* Document entry in storage */
typedef struct CACHE_ALIGNED doc_entry {
    uint32_t magic;         /* 0xDOCU */
    uint32_t flags;         /* Compression, encryption, etc */
    uint64_t key_hash;      /* For fast lookup */
    uint32_t key_len;
    uint32_t value_len;
    uint64_t timestamp;     /* For MVCC */
    uint64_t next_offset;   /* For hash collision chain */
    /* Followed by key and value data */
} doc_entry_t;

/* Memory-mapped storage handle */
typedef struct mmap_storage {
    int fd;
    void* base_addr;
    size_t mapped_size;
    size_t file_size;
    
    /* Mapped regions */
    storage_header_t* header;
    void* data_region;
    void* index_region;
    
    /* Concurrency control */
    pthread_rwlock_t resize_lock;
    pthread_mutex_t alloc_lock;
    
    /* Free space management */
    free_entry_t* free_list;
    size_t total_free;
    
    /* Statistics */
    _Atomic uint64_t read_count;
    _Atomic uint64_t write_count;
    _Atomic uint64_t cache_hits;
    _Atomic uint64_t cache_misses;
} mmap_storage_t;

/* Partition for sharding large collections */
typedef struct CACHE_ALIGNED partition {
    uint32_t id;
    mmap_storage_t* storage;
    pthread_spinlock_t lock;
    
    /* Bloom filter for existence checks */
    void* bloom_filter;
    
    /* Statistics */
    _Atomic uint64_t doc_count;
    _Atomic uint64_t total_size;
} partition_t;

/* Partitioned collection */
typedef struct partitioned_collection {
    char name[256];
    uint32_t partition_count;
    partition_t** partitions;
    
    /* Hash function for routing */
    uint32_t (*hash_fn)(const void* key, size_t len);
    
    /* Global statistics */
    _Atomic uint64_t total_docs;
    _Atomic uint64_t total_size;
} partitioned_collection_t;

/* API Functions */

/* Initialize mmap storage */
mmap_storage_t* mmap_storage_create(const char* path, size_t initial_size);
void mmap_storage_destroy(mmap_storage_t* storage);

/* Storage operations */
int mmap_storage_put(mmap_storage_t* storage, const void* key, size_t key_len,
                     const void* value, size_t value_len);
int mmap_storage_get(mmap_storage_t* storage, const void* key, size_t key_len,
                     void** value, size_t* value_len);
int mmap_storage_get_by_offset(mmap_storage_t* storage, uint64_t offset,
                              void** key, size_t* key_len,
                              void** value, size_t* value_len);
int mmap_storage_delete(mmap_storage_t* storage, const void* key, size_t key_len);

/* Batch operations for efficiency */
int mmap_storage_put_batch(mmap_storage_t* storage, 
                          const void** keys, const size_t* key_lens,
                          const void** values, const size_t* value_lens,
                          size_t count);

/* Maintenance operations */
int mmap_storage_compact(mmap_storage_t* storage);
int mmap_storage_resize(mmap_storage_t* storage, size_t new_size);
int mmap_storage_sync(mmap_storage_t* storage);

/* Partitioned operations */
partitioned_collection_t* partitioned_collection_create(const char* name, 
                                                       uint32_t partition_count);
void partitioned_collection_destroy(partitioned_collection_t* collection);

int partitioned_put(partitioned_collection_t* collection,
                   const void* key, size_t key_len,
                   const void* value, size_t value_len);
int partitioned_get(partitioned_collection_t* collection,
                   const void* key, size_t key_len,
                   void** value, size_t* value_len);

/* Utility functions */
static inline uint32_t get_partition(partitioned_collection_t* collection,
                                   const void* key, size_t key_len) {
    return collection->hash_fn(key, key_len) % collection->partition_count;
}

/* Memory barriers for lock-free operations */
#define memory_barrier() __sync_synchronize()
#define load_acquire(ptr) __atomic_load_n(ptr, __ATOMIC_ACQUIRE)
#define store_release(ptr, val) __atomic_store_n(ptr, val, __ATOMIC_RELEASE)

#endif /* MMAP_STORAGE_H */