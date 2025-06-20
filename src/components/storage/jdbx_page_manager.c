/*
 * JDBX Page Manager V2 - CLEAN CUT with Integrated WAL
 * ONE SOURCE OF TRUTH - Single file database with built-in WAL
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
#include "utils/buffer_pool.h"

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
    /* Calculate checksum up to but not including checksum field */
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

/* Get pointer to a page */
static inline void* jdbx_get_page_ptr(jdbx_page_manager_t* pm, uint64_t page_id) {
    assert(page_id < pm->header->total_pages);
    return (uint8_t*)pm->mmap_base + (page_id * JDBX_PAGE_SIZE);
}

/* Initialize integrated WAL pages */
static int init_integrated_wal(jdbx_page_manager_t* pm) {
    LOG_INFO("Initializing integrated WAL with %d pages", JDBX_WAL_PAGES);
    
    /* WAL starts after bitmap pages */
    uint64_t bitmap_end = pm->header->bitmap_start_page + pm->bitmap_pages;
    pm->header->wal_start_page = bitmap_end;
    
    /* Initialize each WAL page */
    for (uint64_t i = 0; i < JDBX_WAL_PAGES; i++) {
        uint64_t page_num = pm->header->wal_start_page + i;
        wal_page_t* wal_page = (wal_page_t*)jdbx_get_page_ptr(pm, page_num);
        
        /* Clear the page */
        memset(wal_page, 0, JDBX_PAGE_SIZE);
        
        /* Set up WAL page header */
        wal_page->header.type = PAGE_TYPE_WAL;
        wal_page->header.page_id = page_num;
        wal_page->header.transaction_id = pm->header->transaction_id;
        wal_page->wal_sequence_start = 0;
        wal_page->wal_sequence_end = 0;
        wal_page->entry_count = 0;
        wal_page->used_bytes = sizeof(wal_page_t);
        
        /* Calculate and set checksum */
        wal_page->header.checksum = jdbx_crc32(wal_page, JDBX_PAGE_SIZE);
        
        /* Mark page as used in bitmap */
        size_t byte_idx = page_num / 64;
        size_t bit_idx = page_num % 64;
        pm->bitmap[byte_idx] |= (1ULL << bit_idx);
    }
    
    /* Set WAL pointers in header */
    pm->header->wal_current_page = pm->header->wal_start_page;
    pm->header->wal_current_offset = sizeof(wal_page_t);
    pm->header->wal_checkpoint_page = pm->header->wal_start_page;
    pm->header->wal_sequence = 1;
    
    /* Root directory comes after WAL */
    pm->header->root_directory_page = bitmap_end + JDBX_WAL_PAGES;
    
    LOG_INFO("WAL initialized: pages %llu-%llu", 
             pm->header->wal_start_page, 
             pm->header->wal_start_page + JDBX_WAL_PAGES - 1);
    
    return 0;
}

/* Initialize a new JDBX file - CLEAN CUT VERSION */
static int init_new_file(jdbx_page_manager_t* pm, size_t initial_size) {
    /* Calculate initial pages */
    uint64_t total_pages = initial_size / JDBX_PAGE_SIZE;
    if (total_pages < 1000) total_pages = 1000; /* Minimum 1000 pages */
    
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
    pm->header->version = JDBX_VERSION; /* Version 2 - Integrated WAL */
    pm->header->page_size = JDBX_PAGE_SIZE;
    pm->header->total_pages = total_pages;
    pm->header->bitmap_start_page = 1; /* Right after header */
    pm->header->transaction_id = 1;
    pm->header->last_checkpoint = 1;
    
    /* Calculate bitmap pages needed */
    size_t bitmap_pages = calculate_bitmap_pages(total_pages);
    pm->bitmap = (uint64_t*)((uint8_t*)pm->mmap_base + JDBX_PAGE_SIZE);
    pm->bitmap_pages = bitmap_pages;
    
    /* Mark header and bitmap pages as used */
    for (uint64_t i = 0; i < 1 + bitmap_pages; i++) {
        size_t byte_idx = i / 64;
        size_t bit_idx = i % 64;
        pm->bitmap[byte_idx] |= (1ULL << bit_idx);
    }
    
    /* Initialize integrated WAL */
    if (init_integrated_wal(pm) != 0) {
        jdbx_error("Failed to initialize integrated WAL");
        return -1;
    }
    
    /* Initialize root directory page */
    page_header_t* root_dir = (page_header_t*)jdbx_get_page_ptr(pm, 
                              pm->header->root_directory_page);
    memset(root_dir, 0, JDBX_PAGE_SIZE);
    root_dir->type = PAGE_TYPE_DIRECTORY;
    root_dir->page_id = pm->header->root_directory_page;
    root_dir->transaction_id = pm->header->transaction_id;
    root_dir->checksum = jdbx_crc32(root_dir, JDBX_PAGE_SIZE);
    
    /* Mark root directory as used */
    size_t byte_idx = pm->header->root_directory_page / 64;
    size_t bit_idx = pm->header->root_directory_page % 64;
    pm->bitmap[byte_idx] |= (1ULL << bit_idx);
    
    /* Calculate free pages */
    pm->header->free_pages = total_pages - 1 - bitmap_pages - JDBX_WAL_PAGES - 1;
    
    /* Update header checksum */
    update_header_checksum(pm);
    
    /* Sync to disk */
    if (msync(pm->mmap_base, pm->mapped_size, MS_SYNC) != 0) {
        jdbx_error("Failed to sync file: %s", strerror(errno));
        return -1;
    }
    
    LOG_INFO("Created new JDBX V2 file with %llu pages (%zu MB)", 
             (unsigned long long)total_pages,
             (total_pages * JDBX_PAGE_SIZE) / (1024 * 1024));
    LOG_INFO("Layout: Header(1) + Bitmap(%zu) + WAL(%d) + Root(1) + Data(%llu)",
             bitmap_pages, JDBX_WAL_PAGES, pm->header->free_pages);
    
    return 0;
}

/* Create a new JDBX file - CLEAN CUT VERSION */
jdbx_page_manager_t* jdbx_create(const char* path, size_t initial_size) {
    jdbx_page_manager_t* pm = BUFFER_CALLOC(1, sizeof(jdbx_page_manager_t));
    if (!pm) {
        jdbx_error("Failed to allocate page manager");
        return NULL;
    }
    
    /* Open file with O_CREAT | O_EXCL to ensure new file */
    pm->fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0644);
    if (pm->fd < 0) {
        jdbx_error("Failed to create file %s: %s", path, strerror(errno));
        BUFFER_FREE(pm);
        return NULL;
    }
    
    /* Initialize new file with integrated WAL */
    if (init_new_file(pm, initial_size) != 0) {
        close(pm->fd);
        unlink(path);
        BUFFER_FREE(pm);
        return NULL;
    }
    
    /* Initialize locks */
    pthread_rwlock_init(&pm->cache_lock, NULL);
    pthread_mutex_init(&pm->alloc_lock, NULL);
    pthread_mutex_init(&pm->wal_lock, NULL);
    
    /* Initialize cache */
    pm->cache_capacity = JDBX_CACHE_SIZE;
    pm->cache = BUFFER_CALLOC(pm->cache_capacity, sizeof(cache_entry_t));
    if (!pm->cache) {
        jdbx_error("Failed to allocate cache");
        munmap(pm->mmap_base, pm->mapped_size);
        close(pm->fd);
        unlink(path);
        BUFFER_FREE(pm);
        return NULL;
    }
    
    LOG_INFO("Created JDBX V2 database: %s (single file with integrated WAL)", path);
    return pm;
}

/* Open existing JDBX file - CLEAN CUT VERSION */
jdbx_page_manager_t* jdbx_open(const char* path) {
    jdbx_page_manager_t* pm = BUFFER_CALLOC(1, sizeof(jdbx_page_manager_t));
    if (!pm) {
        jdbx_error("Failed to allocate page manager");
        return NULL;
    }
    
    /* Open existing file */
    pm->fd = open(path, O_RDWR);
    if (pm->fd < 0) {
        jdbx_error("Failed to open file %s: %s", path, strerror(errno));
        BUFFER_FREE(pm);
        return NULL;
    }
    
    /* Get file size */
    struct stat st;
    if (fstat(pm->fd, &st) != 0) {
        jdbx_error("Failed to stat file: %s", strerror(errno));
        close(pm->fd);
        BUFFER_FREE(pm);
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
        BUFFER_FREE(pm);
        return NULL;
    }
    
    /* Verify header */
    pm->header = (jdbx_header_t*)pm->mmap_base;
    if (memcmp(pm->header->magic, JDBX_MAGIC, 4) != 0) {
        jdbx_error("Invalid JDBX file magic");
        munmap(pm->mmap_base, pm->mapped_size);
        close(pm->fd);
        BUFFER_FREE(pm);
        return NULL;
    }
    
    /* CLEAN CUT - Only support version 2 */
    if (pm->header->version != JDBX_VERSION) {
        jdbx_error("Unsupported JDBX version: %u (only version 2 supported)", 
                   pm->header->version);
        munmap(pm->mmap_base, pm->mapped_size);
        close(pm->fd);
        BUFFER_FREE(pm);
        return NULL;
    }
    
    /* Verify checksum */
    uint32_t expected_checksum = pm->header->checksum;
    pm->header->checksum = 0;
    size_t checksum_size = offsetof(jdbx_header_t, checksum);
    uint32_t actual_checksum = jdbx_crc32(pm->header, checksum_size);
    pm->header->checksum = expected_checksum;
    
    if (actual_checksum != expected_checksum) {
        jdbx_error("Header checksum mismatch");
        munmap(pm->mmap_base, pm->mapped_size);
        close(pm->fd);
        BUFFER_FREE(pm);
        return NULL;
    }
    
    /* Set up bitmap pointer */
    pm->bitmap = (uint64_t*)((uint8_t*)pm->mmap_base + 
                            pm->header->bitmap_start_page * JDBX_PAGE_SIZE);
    pm->bitmap_pages = calculate_bitmap_pages(pm->header->total_pages);
    
    /* Initialize locks */
    pthread_rwlock_init(&pm->cache_lock, NULL);
    pthread_mutex_init(&pm->alloc_lock, NULL);
    pthread_mutex_init(&pm->wal_lock, NULL);
    
    /* Initialize cache */
    pm->cache_capacity = JDBX_CACHE_SIZE;
    pm->cache = BUFFER_CALLOC(pm->cache_capacity, sizeof(cache_entry_t));
    if (!pm->cache) {
        jdbx_error("Failed to allocate cache");
        munmap(pm->mmap_base, pm->mapped_size);
        close(pm->fd);
        BUFFER_FREE(pm);
        return NULL;
    }
    
    /* Perform WAL recovery if needed */
    if (pm->header->wal_current_page != pm->header->wal_checkpoint_page ||
        pm->header->last_checkpoint < pm->header->transaction_id) {
        LOG_INFO("WAL recovery needed: checkpoint=%llu, current=%llu",
                 pm->header->last_checkpoint, pm->header->transaction_id);
        if (jdbx_wal_recover(pm) != 0) {
            LOG_ERROR("WAL recovery failed");
            /* Continue anyway - database might still be usable */
        }
    }
    
    LOG_INFO("Opened JDBX V2 database: %s", path);
    return pm;
}

/* Write to integrated WAL */
int jdbx_wal_write(jdbx_page_manager_t* pm, uint64_t page_id, 
                   const void* page_data, size_t size) {
    if (size > JDBX_PAGE_SIZE) {
        jdbx_error("WAL entry too large: %zu", size);
        return -1;
    }
    
    pthread_mutex_lock(&pm->wal_lock);
    
    /* Get current WAL page */
    wal_page_t* wal_page = (wal_page_t*)jdbx_get_page_ptr(pm, 
                                        pm->header->wal_current_page);
    
    /* Check if entry fits in current page */
    size_t entry_size = sizeof(wal_entry_t) + size;
    size_t available = JDBX_PAGE_SIZE - wal_page->used_bytes;
    
    if (entry_size > available) {
        /* Move to next WAL page */
        uint64_t next_page = pm->header->wal_current_page + 1;
        if (next_page >= pm->header->wal_start_page + JDBX_WAL_PAGES) {
            /* Wrap around to beginning of WAL */
            next_page = pm->header->wal_start_page;
        }
        
        /* Check if we're about to overwrite uncheckpointed data */
        if (next_page == pm->header->wal_checkpoint_page) {
            pthread_mutex_unlock(&pm->wal_lock);
            LOG_ERROR("WAL is full - checkpoint needed");
            return -1;
        }
        
        /* Initialize next WAL page */
        pm->header->wal_current_page = next_page;
        wal_page = (wal_page_t*)jdbx_get_page_ptr(pm, next_page);
        wal_page->wal_sequence_start = pm->header->wal_sequence;
        wal_page->wal_sequence_end = pm->header->wal_sequence;
        wal_page->entry_count = 0;
        wal_page->used_bytes = sizeof(wal_page_t);
    }
    
    /* Write WAL entry */
    uint8_t* entry_ptr = (uint8_t*)wal_page + wal_page->used_bytes;
    wal_entry_t* entry = (wal_entry_t*)entry_ptr;
    
    entry->sequence = pm->header->wal_sequence++;
    entry->transaction_id = pm->header->transaction_id;
    entry->page_id = page_id;
    entry->size = size;
    entry->checksum = jdbx_crc32(page_data, size);
    
    /* Copy page data */
    memcpy(entry_ptr + sizeof(wal_entry_t), page_data, size);
    
    /* Update WAL page metadata */
    wal_page->wal_sequence_end = entry->sequence;
    wal_page->entry_count++;
    wal_page->used_bytes += entry_size;
    
    /* Update WAL page checksum */
    wal_page->header.checksum = jdbx_crc32(wal_page, JDBX_PAGE_SIZE);
    
    /* Sync WAL page */
    if (msync(wal_page, JDBX_PAGE_SIZE, MS_SYNC) != 0) {
        pthread_mutex_unlock(&pm->wal_lock);
        jdbx_error("Failed to sync WAL page: %s", strerror(errno));
        return -1;
    }
    
    pthread_mutex_unlock(&pm->wal_lock);
    return 0;
}

/* Recover from integrated WAL */
int jdbx_wal_recover(jdbx_page_manager_t* pm) {
    uint64_t entries_processed = 0;
    uint64_t entries_applied = 0;
    
    LOG_INFO("Starting WAL recovery");
    
    /* Start from checkpoint page and process to current page */
    uint64_t current_page = pm->header->wal_checkpoint_page;
    
    while (1) {
        wal_page_t* wal_page = (wal_page_t*)jdbx_get_page_ptr(pm, current_page);
        
        /* Verify WAL page */
        if (wal_page->header.type != PAGE_TYPE_WAL) {
            LOG_WARNING("Invalid WAL page type at %llu", current_page);
            break;
        }
        
        /* Process entries in this page */
        uint64_t offset = sizeof(wal_page_t);
        
        for (uint32_t i = 0; i < wal_page->entry_count; i++) {
            wal_entry_t* entry = (wal_entry_t*)((uint8_t*)wal_page + offset);
            
            /* Verify entry */
            if (entry->transaction_id > pm->header->last_checkpoint) {
                /* Apply the page update */
                void* data_ptr = (uint8_t*)entry + sizeof(wal_entry_t);
                uint32_t checksum = jdbx_crc32(data_ptr, entry->size);
                
                if (checksum != entry->checksum) {
                    LOG_ERROR("WAL entry checksum mismatch");
                    return -1;
                }
                
                /* Apply to actual page */
                void* page_ptr = jdbx_get_page_ptr(pm, entry->page_id);
                memcpy(page_ptr, data_ptr, entry->size);
                entries_applied++;
            }
            
            entries_processed++;
            offset += sizeof(wal_entry_t) + entry->size;
        }
        
        /* Move to next page */
        if (current_page == pm->header->wal_current_page) {
            break; /* Reached current page */
        }
        
        current_page++;
        if (current_page >= pm->header->wal_start_page + JDBX_WAL_PAGES) {
            current_page = pm->header->wal_start_page; /* Wrap around */
        }
    }
    
    LOG_INFO("WAL recovery complete: %llu entries processed, %llu applied",
             entries_processed, entries_applied);
    
    /* Update checkpoint */
    pm->header->last_checkpoint = pm->header->transaction_id;
    pm->header->wal_checkpoint_page = pm->header->wal_current_page;
    update_header_checksum(pm);
    
    return 0;
}

/* Sync database to disk */
int jdbx_sync(jdbx_page_manager_t* pm) {
    if (!pm || !pm->mmap_base) return -1;
    
    /* Sync entire database */
    if (msync(pm->mmap_base, pm->mapped_size, MS_SYNC) != 0) {
        jdbx_error("Failed to sync database: %s", strerror(errno));
        return -1;
    }
    
    return 0;
}

/* Checkpoint - flush WAL to data pages */
int jdbx_checkpoint(jdbx_page_manager_t* pm) {
    pthread_mutex_lock(&pm->wal_lock);
    
    /* Already checkpointed? */
    if (pm->header->wal_checkpoint_page == pm->header->wal_current_page &&
        pm->header->last_checkpoint == pm->header->transaction_id) {
        pthread_mutex_unlock(&pm->wal_lock);
        return 0;
    }
    
    LOG_INFO("Starting checkpoint");
    
    /* Update checkpoint in header */
    pm->header->last_checkpoint = pm->header->transaction_id;
    pm->header->wal_checkpoint_page = pm->header->wal_current_page;
    update_header_checksum(pm);
    
    /* Sync entire database */
    if (msync(pm->mmap_base, pm->mapped_size, MS_SYNC) != 0) {
        pthread_mutex_unlock(&pm->wal_lock);
        jdbx_error("Failed to sync database: %s", strerror(errno));
        return -1;
    }
    
    pthread_mutex_unlock(&pm->wal_lock);
    
    LOG_INFO("Checkpoint complete");
    return 0;
}

/* Close database */
void jdbx_close(jdbx_page_manager_t* pm) {
    if (!pm) return;
    
    /* Perform final checkpoint */
    jdbx_checkpoint(pm);
    
    /* Free cache */
    if (pm->cache) {
        BUFFER_FREE(pm->cache);
    }
    
    /* Unmap file */
    if (pm->mmap_base && pm->mmap_base != MAP_FAILED) {
        munmap(pm->mmap_base, pm->mapped_size);
    }
    
    /* Close file */
    if (pm->fd >= 0) {
        close(pm->fd);
    }
    
    /* Destroy locks */
    pthread_rwlock_destroy(&pm->cache_lock);
    pthread_mutex_destroy(&pm->alloc_lock);
    pthread_mutex_destroy(&pm->wal_lock);
    
    BUFFER_FREE(pm);
    
    LOG_INFO("JDBX database closed");
}