/*
 * JDBX Page Manager - Core file and page management
 */

#define _GNU_SOURCE  /* For mremap */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>
#include <stdarg.h>

#include "storage/jdbx.h"
#include "utils/logger.h"

/* CRC32 table for checksums */
static uint32_t crc32_table[256];
static int crc32_table_initialized = 0;

/* Initialize CRC32 table */
static void init_crc32_table(void) {
    if (crc32_table_initialized) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
        crc32_table[i] = crc;
    }
    crc32_table_initialized = 1;
}

/* Calculate CRC32 checksum */
uint32_t jdbx_crc32(const void* data, size_t size) {
    init_crc32_table();
    
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < size; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ bytes[i]) & 0xFF];
    }
    
    return crc ^ 0xFFFFFFFF;
}

/* Update header checksum */
static void update_header_checksum(jdbx_page_manager_t* pm) {
    pm->header->checksum = 0;
    size_t checksum_size = offsetof(jdbx_header_t, checksum);
    pm->header->checksum = jdbx_crc32(pm->header, checksum_size);
}

/* Error reporting */
void jdbx_error(const char* fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    LOG_ERROR("JDBX: %s", buffer);
    fprintf(stderr, "JDBX ERROR: %s\n", buffer);
}

/* Calculate number of pages needed for bitmap */
static size_t calculate_bitmap_pages(uint64_t total_pages) {
    /* 8 pages per byte, JDBX_PAGE_SIZE bytes per page */
    size_t bits_per_page = JDBX_PAGE_SIZE * 8;
    return (total_pages + bits_per_page - 1) / bits_per_page;
}

/* Initialize a new JDBX file */
static int init_new_file(jdbx_page_manager_t* pm, size_t initial_size) {
    /* Calculate initial pages */
    uint64_t total_pages = initial_size / JDBX_PAGE_SIZE;
    if (total_pages < 100) total_pages = 100; /* Minimum 100 pages */
    
    /* Expand file to initial size */
    if (ftruncate(pm->fd, total_pages * JDBX_PAGE_SIZE) != 0) {
        jdbx_error("Failed to expand file: %s", strerror(errno));
        return -1;
    }
    
    /* Memory map the file */
    pm->file_size = total_pages * JDBX_PAGE_SIZE;
    pm->mapped_size = pm->file_size;
    pm->mmap_base = mmap(NULL, pm->mapped_size, 
                         PROT_READ | PROT_WRITE, MAP_SHARED, 
                         pm->fd, 0);
    
    if (pm->mmap_base == MAP_FAILED) {
        jdbx_error("Failed to mmap file: %s", strerror(errno));
        return -1;
    }
    
    /* Initialize header */
    pm->header = (jdbx_header_t*)pm->mmap_base;
    memset(pm->header, 0, JDBX_HEADER_SIZE);
    memcpy(pm->header->magic, JDBX_MAGIC, 4);
    pm->header->version = JDBX_VERSION;
    pm->header->page_size = JDBX_PAGE_SIZE;
    pm->header->total_pages = total_pages;
    pm->header->bitmap_start_page = 1; /* Right after header */
    pm->header->transaction_id = 1;
    pm->header->last_checkpoint = 1;
    
    /* Calculate bitmap pages needed */
    size_t bitmap_pages = calculate_bitmap_pages(total_pages);
    
    /* Root directory comes after bitmap */
    pm->header->root_directory_page = 1 + bitmap_pages;
    
    /* Free pages = total - header - bitmap - root directory */
    pm->header->free_pages = total_pages - 1 - bitmap_pages - 1;
    
    /* Update header checksum */
    update_header_checksum(pm);
    
    LOG_DEBUG("Created header checksum: 0x%08x", pm->header->checksum);
    
    /* Initialize bitmap */
    pm->bitmap = (uint64_t*)((uint8_t*)pm->mmap_base + JDBX_PAGE_SIZE);
    pm->bitmap_pages = bitmap_pages;
    
    /* Mark used pages in bitmap: header, bitmap pages, root directory */
    for (uint64_t i = 0; i < 1 + bitmap_pages + 1; i++) {
        size_t byte_idx = i / 64;
        size_t bit_idx = i % 64;
        pm->bitmap[byte_idx] |= (1ULL << bit_idx);
    }
    
    /* Initialize root directory page */
    page_header_t* root_dir = (page_header_t*)((uint8_t*)pm->mmap_base + 
                              pm->header->root_directory_page * JDBX_PAGE_SIZE);
    memset(root_dir, 0, JDBX_PAGE_SIZE);
    root_dir->type = PAGE_TYPE_DIRECTORY;
    root_dir->page_id = pm->header->root_directory_page;
    root_dir->transaction_id = pm->header->transaction_id;
    root_dir->checksum = jdbx_crc32(root_dir, JDBX_PAGE_SIZE);
    
    /* Sync to disk */
    if (msync(pm->mmap_base, pm->mapped_size, MS_SYNC) != 0) {
        jdbx_error("Failed to sync file: %s", strerror(errno));
        return -1;
    }
    
    LOG_INFO("Created new JDBX file with %llu pages (%zu MB)", 
             (unsigned long long)total_pages,
             (total_pages * JDBX_PAGE_SIZE) / (1024 * 1024));
    
    return 0;
}

/* Create a new JDBX file */
jdbx_page_manager_t* jdbx_create(const char* path, size_t initial_size) {
    jdbx_page_manager_t* pm = calloc(1, sizeof(jdbx_page_manager_t));
    if (!pm) {
        jdbx_error("Failed to allocate page manager");
        return NULL;
    }
    
    /* Open file with O_CREAT | O_EXCL to ensure new file */
    pm->fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0644);
    if (pm->fd < 0) {
        jdbx_error("Failed to create file %s: %s", path, strerror(errno));
        free(pm);
        return NULL;
    }
    
    /* Initialize new file */
    if (init_new_file(pm, initial_size) != 0) {
        close(pm->fd);
        unlink(path);
        free(pm);
        return NULL;
    }
    
    /* Initialize locks */
    pthread_rwlock_init(&pm->cache_lock, NULL);
    pthread_mutex_init(&pm->alloc_lock, NULL);
    pthread_mutex_init(&pm->wal.lock, NULL);
    
    /* Initialize cache */
    pm->cache_capacity = JDBX_CACHE_SIZE;
    pm->cache = calloc(pm->cache_capacity, sizeof(cache_entry_t));
    if (!pm->cache) {
        jdbx_error("Failed to allocate cache");
        munmap(pm->mmap_base, pm->mapped_size);
        close(pm->fd);
        unlink(path);
        free(pm);
        return NULL;
    }
    
    /* Create WAL file */
    char wal_path[1024];
    size_t path_len = strlen(path);
    if (path_len >= 5 && strcmp(path + path_len - 5, ".jdbx") == 0) {
        /* Replace .jdbx with .wal */
        snprintf(wal_path, sizeof(wal_path), "%.*s.wal", (int)(path_len - 5), path);
    } else {
        /* Add .wal to path */
        snprintf(wal_path, sizeof(wal_path), "%s.wal", path);
    }
    pm->wal.fd = open(wal_path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (pm->wal.fd < 0) {
        jdbx_error("Failed to create WAL file: %s", strerror(errno));
        free(pm->cache);
        munmap(pm->mmap_base, pm->mapped_size);
        close(pm->fd);
        unlink(path);
        free(pm);
        return NULL;
    }
    
    /* Initialize WAL with minimal size */
    pm->wal.size = 1024 * 1024; /* 1MB initial WAL */
    if (ftruncate(pm->wal.fd, pm->wal.size) != 0) {
        jdbx_error("Failed to initialize WAL: %s", strerror(errno));
        close(pm->wal.fd);
        unlink(wal_path);
        free(pm->cache);
        munmap(pm->mmap_base, pm->mapped_size);
        close(pm->fd);
        unlink(path);
        free(pm);
        return NULL;
    }
    
    /* Memory map WAL */
    pm->wal.mmap_base = mmap(NULL, pm->wal.size,
                            PROT_READ | PROT_WRITE, MAP_SHARED,
                            pm->wal.fd, 0);
    if (pm->wal.mmap_base == MAP_FAILED) {
        jdbx_error("Failed to mmap WAL: %s", strerror(errno));
        close(pm->wal.fd);
        unlink(wal_path);
        free(pm->cache);
        munmap(pm->mmap_base, pm->mapped_size);
        close(pm->fd);
        unlink(path);
        free(pm);
        return NULL;
    }
    
    LOG_INFO("Created JDBX database: %s", path);
    return pm;
}

/* Open existing JDBX file */
jdbx_page_manager_t* jdbx_open(const char* path) {
    jdbx_page_manager_t* pm = calloc(1, sizeof(jdbx_page_manager_t));
    if (!pm) {
        jdbx_error("Failed to allocate page manager");
        return NULL;
    }
    
    /* Open existing file */
    pm->fd = open(path, O_RDWR);
    if (pm->fd < 0) {
        jdbx_error("Failed to open file %s: %s (errno=%d)", path, strerror(errno), errno);
        LOG_ERROR("jdbx_open: Failed to open %s: %s", path, strerror(errno));
        free(pm);
        return NULL;
    }
    
    /* Get file size */
    struct stat st;
    if (fstat(pm->fd, &st) != 0) {
        jdbx_error("Failed to stat file: %s", strerror(errno));
        close(pm->fd);
        free(pm);
        return NULL;
    }
    
    pm->file_size = st.st_size;
    pm->mapped_size = pm->file_size;
    
    /* Memory map the file */
    pm->mmap_base = mmap(NULL, pm->mapped_size,
                         PROT_READ | PROT_WRITE, MAP_SHARED,
                         pm->fd, 0);
    
    if (pm->mmap_base == MAP_FAILED) {
        jdbx_error("Failed to mmap file: %s", strerror(errno));
        close(pm->fd);
        free(pm);
        return NULL;
    }
    
    /* Verify header */
    pm->header = (jdbx_header_t*)pm->mmap_base;
    if (memcmp(pm->header->magic, JDBX_MAGIC, 4) != 0) {
        jdbx_error("Invalid JDBX file magic");
        munmap(pm->mmap_base, pm->mapped_size);
        close(pm->fd);
        free(pm);
        return NULL;
    }
    
    if (pm->header->version != JDBX_VERSION) {
        jdbx_error("Unsupported JDBX version: %u", pm->header->version);
        munmap(pm->mmap_base, pm->mapped_size);
        close(pm->fd);
        free(pm);
        return NULL;
    }
    
    /* Verify checksum */
    uint32_t expected_checksum = pm->header->checksum;
    pm->header->checksum = 0;
    size_t checksum_size = offsetof(jdbx_header_t, checksum);
    
    /* Debug: print all header fields */
    LOG_DEBUG("Header during verify: magic=%c%c%c%c, version=%u, page_size=%u, total_pages=%llu, "
              "free_pages=%llu, root_dir=%llu, bitmap_start=%llu, txn_id=%llu, last_ckpt=%llu",
              pm->header->magic[0], pm->header->magic[1], pm->header->magic[2], pm->header->magic[3],
              pm->header->version, pm->header->page_size, 
              (unsigned long long)pm->header->total_pages,
              (unsigned long long)pm->header->free_pages,
              (unsigned long long)pm->header->root_directory_page,
              (unsigned long long)pm->header->bitmap_start_page,
              (unsigned long long)pm->header->transaction_id,
              (unsigned long long)pm->header->last_checkpoint);
    
    uint32_t actual_checksum = jdbx_crc32(pm->header, checksum_size);
    pm->header->checksum = expected_checksum;
    
    LOG_DEBUG("Verifying header checksum: expected=0x%08x, actual=0x%08x (size: %zu bytes)", 
              expected_checksum, actual_checksum, checksum_size);
    
    if (actual_checksum != expected_checksum) {
        jdbx_error("Header checksum mismatch: expected=0x%08x, actual=0x%08x", 
                   expected_checksum, actual_checksum);
        munmap(pm->mmap_base, pm->mapped_size);
        close(pm->fd);
        free(pm);
        return NULL;
    }
    
    /* Set up bitmap pointer */
    pm->bitmap = (uint64_t*)((uint8_t*)pm->mmap_base + 
                            pm->header->bitmap_start_page * JDBX_PAGE_SIZE);
    pm->bitmap_pages = calculate_bitmap_pages(pm->header->total_pages);
    
    /* Initialize locks */
    pthread_rwlock_init(&pm->cache_lock, NULL);
    pthread_mutex_init(&pm->alloc_lock, NULL);
    pthread_mutex_init(&pm->wal.lock, NULL);
    
    /* Initialize cache */
    pm->cache_capacity = JDBX_CACHE_SIZE;
    pm->cache = calloc(pm->cache_capacity, sizeof(cache_entry_t));
    if (!pm->cache) {
        jdbx_error("Failed to allocate cache");
        munmap(pm->mmap_base, pm->mapped_size);
        close(pm->fd);
        free(pm);
        return NULL;
    }
    
    /* Open WAL file */
    char wal_path[1024];
    size_t path_len = strlen(path);
    if (path_len >= 5 && strcmp(path + path_len - 5, ".jdbx") == 0) {
        /* Replace .jdbx with .wal */
        snprintf(wal_path, sizeof(wal_path), "%.*s.wal", (int)(path_len - 5), path);
    } else {
        /* Add .wal to path */
        snprintf(wal_path, sizeof(wal_path), "%s.wal", path);
    }
    pm->wal.fd = open(wal_path, O_RDWR | O_CREAT, 0644);
    if (pm->wal.fd < 0) {
        jdbx_error("Failed to open WAL file: %s", strerror(errno));
        free(pm->cache);
        munmap(pm->mmap_base, pm->mapped_size);
        close(pm->fd);
        free(pm);
        return NULL;
    }
    
    /* Get WAL size */
    if (fstat(pm->wal.fd, &st) != 0) {
        jdbx_error("Failed to stat WAL: %s", strerror(errno));
        close(pm->wal.fd);
        free(pm->cache);
        munmap(pm->mmap_base, pm->mapped_size);
        close(pm->fd);
        free(pm);
        return NULL;
    }
    
    pm->wal.size = st.st_size;
    if (pm->wal.size == 0) {
        pm->wal.size = 1024 * 1024; /* 1MB minimum */
        ftruncate(pm->wal.fd, pm->wal.size);
    }
    
    /* Memory map WAL */
    pm->wal.mmap_base = mmap(NULL, pm->wal.size,
                            PROT_READ | PROT_WRITE, MAP_SHARED,
                            pm->wal.fd, 0);
    if (pm->wal.mmap_base == MAP_FAILED) {
        jdbx_error("Failed to mmap WAL: %s", strerror(errno));
        close(pm->wal.fd);
        free(pm->cache);
        munmap(pm->mmap_base, pm->mapped_size);
        close(pm->fd);
        free(pm);
        return NULL;
    }
    
    LOG_INFO("Opened JDBX database: %s (%llu pages)",
             path, (unsigned long long)pm->header->total_pages);
    
    return pm;
}

/* Find free page in bitmap */
static uint64_t find_free_page(jdbx_page_manager_t* pm) {
    size_t bitmap_size = pm->bitmap_pages * JDBX_PAGE_SIZE / sizeof(uint64_t);
    
    /* Start from hint page */
    uint64_t start_idx = pm->hint_page / 64;
    
    for (size_t i = 0; i < bitmap_size; i++) {
        size_t idx = (start_idx + i) % bitmap_size;
        uint64_t word = pm->bitmap[idx];
        
        if (word != 0xFFFFFFFFFFFFFFFFULL) {
            /* Found word with free bit */
            for (int bit = 0; bit < 64; bit++) {
                if (!(word & (1ULL << bit))) {
                    uint64_t page_id = idx * 64 + bit;
                    if (page_id < pm->header->total_pages) {
                        pm->hint_page = page_id + 1;
                        return page_id;
                    }
                }
            }
        }
    }
    
    return 0; /* No free pages */
}

/* Allocate a new page */
uint64_t jdbx_alloc_page(jdbx_page_manager_t* pm, page_type_t type) {
    pthread_mutex_lock(&pm->alloc_lock);
    
    /* Find free page */
    uint64_t page_id = find_free_page(pm);
    if (page_id == 0) {
        /* Need to expand file */
        uint64_t new_pages = pm->header->total_pages / 10; /* Grow by 10% */
        if (new_pages < 100) new_pages = 100;
        
        uint64_t old_total = pm->header->total_pages;
        uint64_t new_total = old_total + new_pages;
        
        /* Expand file */
        if (ftruncate(pm->fd, new_total * JDBX_PAGE_SIZE) != 0) {
            jdbx_error("Failed to expand file: %s", strerror(errno));
            pthread_mutex_unlock(&pm->alloc_lock);
            return 0;
        }
        
        /* Remap if needed */
        if (new_total * JDBX_PAGE_SIZE > pm->mapped_size) {
            void* new_base = mremap(pm->mmap_base, pm->mapped_size,
                                   new_total * JDBX_PAGE_SIZE, MREMAP_MAYMOVE);
            if (new_base == MAP_FAILED) {
                jdbx_error("Failed to remap: %s", strerror(errno));
                pthread_mutex_unlock(&pm->alloc_lock);
                return 0;
            }
            
            pm->mmap_base = new_base;
            pm->mapped_size = new_total * JDBX_PAGE_SIZE;
            pm->file_size = pm->mapped_size;
            
            /* Update pointers */
            pm->header = (jdbx_header_t*)pm->mmap_base;
            pm->bitmap = (uint64_t*)((uint8_t*)pm->mmap_base + 
                                    pm->header->bitmap_start_page * JDBX_PAGE_SIZE);
        }
        
        /* Update header */
        pm->header->total_pages = new_total;
        pm->header->free_pages += new_pages;
        update_header_checksum(pm);
        
        /* Use first new page */
        page_id = old_total;
    }
    
    /* Mark page as used */
    size_t byte_idx = page_id / 64;
    size_t bit_idx = page_id % 64;
    pm->bitmap[byte_idx] |= (1ULL << bit_idx);
    pm->header->free_pages--;
    
    /* Update transaction ID and checksum */
    pm->header->transaction_id++;
    update_header_checksum(pm);
    
    /* Initialize page header */
    page_header_t* page = (page_header_t*)((uint8_t*)pm->mmap_base + 
                                          page_id * JDBX_PAGE_SIZE);
    memset(page, 0, JDBX_PAGE_SIZE);
    page->type = type;
    page->page_id = page_id;
    page->transaction_id = pm->header->transaction_id;
    
    pthread_mutex_unlock(&pm->alloc_lock);
    
    LOG_DEBUG("Allocated page %llu of type %d", 
              (unsigned long long)page_id, type);
    
    return page_id;
}

/* Free a page */
void jdbx_free_page(jdbx_page_manager_t* pm, uint64_t page_id) {
    if (page_id == 0 || page_id >= pm->header->total_pages) {
        jdbx_error("Invalid page ID: %llu", (unsigned long long)page_id);
        return;
    }
    
    pthread_mutex_lock(&pm->alloc_lock);
    
    /* Clear bit in bitmap */
    size_t byte_idx = page_id / 64;
    size_t bit_idx = page_id % 64;
    pm->bitmap[byte_idx] &= ~(1ULL << bit_idx);
    pm->header->free_pages++;
    update_header_checksum(pm);
    
    /* Update hint */
    if (page_id < pm->hint_page) {
        pm->hint_page = page_id;
    }
    
    pthread_mutex_unlock(&pm->alloc_lock);
    
    LOG_DEBUG("Freed page %llu", (unsigned long long)page_id);
}

/* Get page from cache or disk */
page_header_t* jdbx_get_page(jdbx_page_manager_t* pm, uint64_t page_id) {
    if (page_id >= pm->header->total_pages) {
        jdbx_error("Invalid page ID: %llu", (unsigned long long)page_id);
        return NULL;
    }
    
    /* Direct pointer to page */
    page_header_t* page = (page_header_t*)((uint8_t*)pm->mmap_base + 
                                          page_id * JDBX_PAGE_SIZE);
    
    /* Update statistics */
    __atomic_fetch_add(&pm->stats.page_reads, 1, __ATOMIC_RELAXED);
    
    return page;
}

/* Get page for writing (WAL integration) */
page_header_t* jdbx_get_page_for_write(jdbx_page_manager_t* pm, uint64_t page_id) {
    page_header_t* page = jdbx_get_page(pm, page_id);
    if (!page) return NULL;
    
    pthread_mutex_lock(&pm->wal.lock);
    
    /* Write to WAL first */
    if (pm->wal.offset + sizeof(wal_entry_t) + JDBX_PAGE_SIZE > pm->wal.size) {
        /* Extend WAL */
        size_t new_size = pm->wal.size * 2;
        if (ftruncate(pm->wal.fd, new_size) != 0) {
            jdbx_error("Failed to extend WAL: %s", strerror(errno));
            pthread_mutex_unlock(&pm->wal.lock);
            return NULL;
        }
        
        void* new_base = mremap(pm->wal.mmap_base, pm->wal.size,
                               new_size, MREMAP_MAYMOVE);
        if (new_base == MAP_FAILED) {
            jdbx_error("Failed to remap WAL: %s", strerror(errno));
            pthread_mutex_unlock(&pm->wal.lock);
            return NULL;
        }
        
        pm->wal.mmap_base = new_base;
        pm->wal.size = new_size;
    }
    
    /* Write WAL entry */
    wal_entry_t* wal_entry = (wal_entry_t*)((uint8_t*)pm->wal.mmap_base + 
                                            pm->wal.offset);
    memcpy(wal_entry->magic, JDBX_WAL_MAGIC, 4);
    wal_entry->transaction_id = pm->header->transaction_id;
    wal_entry->page_id = page_id;
    wal_entry->size = JDBX_PAGE_SIZE;
    
    /* Copy page data to WAL */
    memcpy(wal_entry + 1, page, JDBX_PAGE_SIZE);
    wal_entry->checksum = jdbx_crc32(wal_entry + 1, JDBX_PAGE_SIZE);
    
    pm->wal.offset += sizeof(wal_entry_t) + JDBX_PAGE_SIZE;
    
    pthread_mutex_unlock(&pm->wal.lock);
    
    /* Update statistics */
    __atomic_fetch_add(&pm->stats.page_writes, 1, __ATOMIC_RELAXED);
    
    return page;
}

/* Sync changes to disk */
int jdbx_sync(jdbx_page_manager_t* pm) {
    /* Sync main file */
    if (msync(pm->mmap_base, pm->mapped_size, MS_SYNC) != 0) {
        jdbx_error("Failed to sync main file: %s", strerror(errno));
        return -1;
    }
    
    /* Sync WAL */
    if (msync(pm->wal.mmap_base, pm->wal.size, MS_SYNC) != 0) {
        jdbx_error("Failed to sync WAL: %s", strerror(errno));
        return -1;
    }
    
    return 0;
}

/* Checkpoint - apply WAL and clear it */
int jdbx_checkpoint(jdbx_page_manager_t* pm) {
    pthread_mutex_lock(&pm->wal.lock);
    
    /* Already applied since we use mmap */
    pm->header->last_checkpoint = pm->header->transaction_id;
    update_header_checksum(pm);
    pm->wal.offset = 0;
    
    /* Clear WAL */
    memset(pm->wal.mmap_base, 0, pm->wal.size);
    
    pthread_mutex_unlock(&pm->wal.lock);
    
    LOG_INFO("Checkpoint complete at transaction %llu",
             (unsigned long long)pm->header->transaction_id);
    
    return jdbx_sync(pm);
}

/* Close database */
void jdbx_close(jdbx_page_manager_t* pm) {
    if (!pm) return;
    
    /* Final checkpoint */
    jdbx_checkpoint(pm);
    
    /* Clean up WAL */
    if (pm->wal.mmap_base && pm->wal.mmap_base != MAP_FAILED) {
        munmap(pm->wal.mmap_base, pm->wal.size);
    }
    if (pm->wal.fd >= 0) {
        close(pm->wal.fd);
    }
    
    /* Clean up main file */
    if (pm->mmap_base && pm->mmap_base != MAP_FAILED) {
        munmap(pm->mmap_base, pm->mapped_size);
    }
    if (pm->fd >= 0) {
        close(pm->fd);
    }
    
    /* Clean up cache */
    free(pm->cache);
    
    /* Destroy locks */
    pthread_rwlock_destroy(&pm->cache_lock);
    pthread_mutex_destroy(&pm->alloc_lock);
    pthread_mutex_destroy(&pm->wal.lock);
    
    LOG_INFO("Closed JDBX database");
    
    free(pm);
}