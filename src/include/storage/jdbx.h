#ifndef JDBX_H
#define JDBX_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <time.h>

/* JDBX Format Constants */
#define JDBX_MAGIC "JDBX"
#define JDBX_VERSION 2  /* Version 2: Integrated WAL - CLEAN CUT! */
#define JDBX_PAGE_SIZE 4096
#define JDBX_HEADER_SIZE JDBX_PAGE_SIZE
#define JDBX_PAGE_HEADER_SIZE 48
#define JDBX_MAX_KEY_SIZE 1024
#define JDBX_CACHE_SIZE 1000
#define JDBX_WAL_PAGES 256  /* 1MB WAL ring buffer (256 * 4KB) */

/* Page types */
typedef enum {
    PAGE_TYPE_FREE = 0,
    PAGE_TYPE_HEADER = 1,
    PAGE_TYPE_BITMAP = 2,
    PAGE_TYPE_DIRECTORY = 3,
    PAGE_TYPE_BTREE_INTERNAL = 4,
    PAGE_TYPE_BTREE_LEAF = 5,
    PAGE_TYPE_OVERFLOW = 6,
    PAGE_TYPE_COLLECTION_META = 7,
    PAGE_TYPE_WAL = 8  /* Integrated WAL page */
} page_type_t;

/* Page flags */
#define PAGE_FLAG_DIRTY     0x01
#define PAGE_FLAG_LOCKED    0x02
#define PAGE_FLAG_COMPRESSED 0x04
#define PAGE_FLAG_CHECKPOINTED 0x08

/* File header - exactly one page */
typedef struct __attribute__((packed)) {
    char magic[4];                  /* "JDBX" */
    uint32_t version;               /* Format version (2 = integrated WAL) */
    uint32_t page_size;             /* Page size (4096) */
    uint64_t total_pages;           /* Total pages in file */
    uint64_t free_pages;            /* Number of free pages */
    uint64_t root_directory_page;   /* Root directory location */
    uint64_t bitmap_start_page;     /* Page allocation bitmap */
    uint64_t transaction_id;        /* Global transaction counter */
    uint64_t last_checkpoint;       /* Last checkpoint txn id */
    /* Integrated WAL fields - CLEAN CUT! */
    uint64_t wal_start_page;        /* First WAL page */
    uint64_t wal_current_page;      /* Current WAL page for writing */
    uint64_t wal_current_offset;    /* Offset within current WAL page */
    uint64_t wal_checkpoint_page;   /* Last checkpointed WAL page */
    uint64_t wal_sequence;          /* Global WAL sequence number */
    uint32_t checksum;              /* CRC32 of header */
    uint8_t reserved[3968];         /* Future use */
} jdbx_header_t;

/* Every page has this header */
typedef struct __attribute__((packed)) {
    uint16_t type;                  /* Page type */
    uint16_t flags;                 /* Page flags */
    uint32_t checksum;              /* Page checksum */
    uint64_t page_id;               /* This page number */
    uint64_t next_page;             /* Next page in chain (0 = none) */
    uint64_t transaction_id;        /* Last modified in txn */
    uint8_t reserved[24];           /* Padding to 48 bytes */
} page_header_t;

/* B-tree node header */
typedef struct __attribute__((packed)) {
    page_header_t header;           /* Common page header */
    uint16_t num_keys;              /* Number of keys */
    uint16_t level;                 /* 0 = leaf, >0 = internal */
    uint32_t total_size;            /* Total size of entries */
    uint64_t right_sibling;         /* Right sibling page */
    uint64_t parent_page;           /* Parent page */
} btree_node_t;

/* B-tree key entry (variable length) */
typedef struct __attribute__((packed)) {
    uint32_t key_size;              /* Key size */
    uint32_t value_size;            /* Value size */
    uint64_t overflow_page;         /* Overflow page if value > threshold */
    /* Followed by key data and value data (or overflow page) */
} btree_entry_t;

/* Collection metadata */
typedef struct __attribute__((packed)) {
    char name[256];                 /* Collection name */
    uint64_t doc_tree_root;         /* Document B-tree root */
    uint64_t index_tree_root;       /* Index B-tree root */
    uint64_t metadata_page;         /* Metadata page */
    uint64_t num_documents;         /* Document count */
    uint64_t total_size;            /* Total size */
    uint64_t created_at;            /* Creation timestamp */
    uint64_t updated_at;            /* Last update timestamp */
} jdbx_collection_meta_t;

/* WAL page header - for integrated WAL pages */
typedef struct __attribute__((packed)) {
    page_header_t header;           /* Standard page header */
    uint64_t wal_sequence_start;    /* First sequence in this page */
    uint64_t wal_sequence_end;      /* Last sequence in this page */
    uint32_t entry_count;           /* Number of entries */
    uint32_t used_bytes;            /* Bytes used for entries */
    /* Followed by WAL entries */
} wal_page_t;

/* WAL entry - stored within WAL pages */
typedef struct __attribute__((packed)) {
    uint64_t sequence;              /* WAL sequence number */
    uint64_t transaction_id;        /* Transaction ID */
    uint64_t page_id;               /* Page being modified */
    uint32_t size;                  /* Size of data */
    uint32_t checksum;              /* Checksum of data */
    /* Followed by page data */
} wal_entry_t;

/* Page cache entry */
typedef struct {
    page_header_t* page;            /* Cached page */
    uint64_t page_id;               /* Page ID */
    time_t last_access;             /* LRU tracking */
    int pin_count;                  /* Pin count */
    int dirty;                      /* Dirty flag */
} cache_entry_t;

/* Page manager structure */
typedef struct {
    /* File management */
    int fd;                         /* File descriptor */
    void* mmap_base;                /* Memory mapped base */
    size_t file_size;               /* Current file size */
    size_t mapped_size;             /* Mapped region size */
    
    /* Header and bitmap */
    jdbx_header_t* header;          /* Pointer to header */
    uint64_t* bitmap;               /* Page allocation bitmap */
    size_t bitmap_pages;            /* Number of bitmap pages */
    
    /* Page cache */
    cache_entry_t* cache;           /* Page cache entries */
    size_t cache_size;              /* Current cache size */
    size_t cache_capacity;          /* Max cache size */
    pthread_rwlock_t cache_lock;    /* Cache lock */
    
    /* Allocation management */
    pthread_mutex_t alloc_lock;     /* Allocation lock */
    uint64_t hint_page;             /* Next page hint */
    
    /* Integrated WAL state - CLEAN CUT! */
    pthread_mutex_t wal_lock;       /* WAL write lock */
    wal_page_t* current_wal_page;   /* Cached current WAL page */
    
    /* Statistics */
    struct {
        uint64_t page_reads;        /* Total page reads */
        uint64_t page_writes;       /* Total page writes */
        uint64_t cache_hits;        /* Cache hits */
        uint64_t cache_misses;      /* Cache misses */
    } stats;
    
} jdbx_page_manager_t;

/* B-tree structure */
typedef struct {
    jdbx_page_manager_t* pm;        /* Page manager */
    uint64_t root_page;             /* Root page ID */
    
    /* Key comparison function */
    int (*compare)(const void* a, size_t a_len, 
                   const void* b, size_t b_len);
    
    /* Statistics */
    struct {
        uint64_t height;            /* Tree height */
        uint64_t num_keys;          /* Total keys */
        uint64_t num_pages;         /* Total pages */
    } stats;
    
} jdbx_btree_t;

/* B-tree iterator structure */
typedef struct {
    jdbx_btree_t* tree;             /* B-tree reference */
    uint64_t current_page;          /* Current page */
    int current_index;              /* Current key index in page */
    int finished;                   /* Iterator finished */
} jdbx_btree_iterator_t;

/* Core page manager operations */
jdbx_page_manager_t* jdbx_create(const char* path, size_t initial_size);
jdbx_page_manager_t* jdbx_open(const char* path);
page_header_t* jdbx_get_page(jdbx_page_manager_t* pm, uint64_t page_id);
page_header_t* jdbx_get_page_for_write(jdbx_page_manager_t* pm, uint64_t page_id);
uint64_t jdbx_alloc_page(jdbx_page_manager_t* pm, page_type_t type);
void jdbx_free_page(jdbx_page_manager_t* pm, uint64_t page_id);
int jdbx_sync(jdbx_page_manager_t* pm);
int jdbx_checkpoint(jdbx_page_manager_t* pm);
void jdbx_close(jdbx_page_manager_t* pm);

/* B-tree operations */
jdbx_btree_t* jdbx_btree_create(jdbx_page_manager_t* pm, 
                                int (*compare)(const void*, size_t, const void*, size_t));
jdbx_btree_t* jdbx_btree_open(jdbx_page_manager_t* pm, uint64_t root_page,
                              int (*compare)(const void*, size_t, const void*, size_t));
int jdbx_btree_insert(jdbx_btree_t* tree, 
                      const void* key, size_t key_len,
                      const void* value, size_t value_len);
int jdbx_btree_get(jdbx_btree_t* tree,
                   const void* key, size_t key_len,
                   void** value, size_t* value_len);
int jdbx_btree_delete(jdbx_btree_t* tree,
                      const void* key, size_t key_len);
void jdbx_btree_close(jdbx_btree_t* tree);

/* B-tree iterator operations */
jdbx_btree_iterator_t* jdbx_btree_iterator_create(jdbx_btree_t* tree);
int jdbx_btree_iterator_next(jdbx_btree_iterator_t* iter, 
                             void** key, size_t* key_len,
                             void** value, size_t* value_len);
void jdbx_btree_iterator_destroy(jdbx_btree_iterator_t* iter);

/* Integrated WAL operations - CLEAN CUT! */
int jdbx_wal_write(jdbx_page_manager_t* pm, uint64_t page_id, 
                   const void* page_data, size_t size);
int jdbx_wal_recover(jdbx_page_manager_t* pm);

/* Utility functions */
uint32_t jdbx_crc32(const void* data, size_t size);
void jdbx_error(const char* fmt, ...);

#endif /* JDBX_H */