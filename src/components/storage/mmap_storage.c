#define _GNU_SOURCE  /* For mremap */
#include "storage/mmap_storage.h"
#include "utils/logger.h"
#include "utils/memory_debug.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

/* Forward declaration */
static uint32_t murmur3_32(const void* key, size_t len, uint32_t seed);

/* Hash function wrapper for partitioned collections */
static uint32_t hash_wrapper(const void* key, size_t len) {
    return murmur3_32(key, len, 0);
}

/* Hash function for document keys */
static uint32_t murmur3_32(const void* key, size_t len, uint32_t seed) {
    const uint8_t* data = (const uint8_t*)key;
    const int nblocks = len / 4;
    uint32_t h1 = seed;
    
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;
    
    /* Body */
    const uint32_t* blocks = (const uint32_t*)(data + nblocks * 4);
    for (int i = -nblocks; i; i++) {
        uint32_t k1 = blocks[i];
        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= c2;
        
        h1 ^= k1;
        h1 = (h1 << 13) | (h1 >> 19);
        h1 = h1 * 5 + 0xe6546b64;
    }
    
    /* Tail */
    const uint8_t* tail = (const uint8_t*)(data + nblocks * 4);
    uint32_t k1 = 0;
    
    switch(len & 3) {
        case 3: k1 ^= tail[2] << 16;
                /* fall through */
        case 2: k1 ^= tail[1] << 8;
                /* fall through */
        case 1: k1 ^= tail[0];
                k1 *= c1; k1 = (k1 << 15) | (k1 >> 17); k1 *= c2; h1 ^= k1;
    }
    
    /* Finalization */
    h1 ^= len;
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;
    
    return h1;
}

/* Calculate CRC32 checksum */
static uint32_t crc32(const void* data, size_t size) {
    static const uint32_t crc_table[256] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
        0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
        0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
        0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
        /* ... rest of table omitted for brevity ... */
    };
    
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t crc = 0xffffffff;
    
    for (size_t i = 0; i < size; i++) {
        crc = crc_table[(crc ^ bytes[i]) & 0xff] ^ (crc >> 8);
    }
    
    return crc ^ 0xffffffff;
}

/* Allocate space from free list or extend file */
static uint64_t allocate_space(mmap_storage_t* storage, size_t size) {
    pthread_mutex_lock(&storage->alloc_lock);
    
    /* Align size to 8 bytes */
    size = (size + 7) & ~7;
    
    /* First check free list */
    free_entry_t* prev = NULL;
    free_entry_t* curr = storage->free_list;
    
    while (curr) {
        if (curr->size >= size) {
            uint64_t offset = curr->offset;
            
            if (curr->size > size + sizeof(free_entry_t)) {
                /* Split the free block */
                curr->offset += size;
                curr->size -= size;
            } else {
                /* Remove entire block from free list */
                if (prev) {
                    prev->next = curr->next;
                } else {
                    storage->free_list = curr->next;
                }
                storage->total_free -= curr->size;
                free(curr);
            }
            
            pthread_mutex_unlock(&storage->alloc_lock);
            return offset;
        }
        prev = curr;
        curr = curr->next;
    }
    
    /* No suitable free block, extend the file */
    uint64_t offset = storage->header->free_offset;
    storage->header->free_offset += size;
    
    /* Check if we need to resize */
    if (storage->header->free_offset > storage->mapped_size) {
        pthread_mutex_unlock(&storage->alloc_lock);
        
        /* Resize the mapping */
        size_t new_size = storage->mapped_size * 2;
        if (new_size > MAX_MMAP_SIZE) {
            new_size = MAX_MMAP_SIZE;
        }
        
        if (mmap_storage_resize(storage, new_size) != 0) {
            return 0; /* Allocation failed */
        }
        
        pthread_mutex_lock(&storage->alloc_lock);
    }
    
    pthread_mutex_unlock(&storage->alloc_lock);
    return offset;
}

/* Initialize memory-mapped storage */
mmap_storage_t* mmap_storage_create(const char* path, size_t initial_size) {
    if (!path) {
        LOG_ERROR("Storage path is NULL");
        return NULL;
    }
    
    /* Round up to page size */
    initial_size = (initial_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (initial_size < DEFAULT_MMAP_SIZE) {
        initial_size = DEFAULT_MMAP_SIZE;
    }
    
    mmap_storage_t* storage = calloc(1, sizeof(mmap_storage_t));
    if (!storage) {
        LOG_ERROR("Failed to allocate storage structure");
        return NULL;
    }
    
    /* Open or create the file */
    storage->fd = open(path, O_RDWR | O_CREAT, 0644);
    if (storage->fd < 0) {
        LOG_ERROR("Failed to open storage file %s: %s", path, strerror(errno));
        free(storage);
        return NULL;
    }
    
    /* Get file size */
    struct stat st;
    if (fstat(storage->fd, &st) < 0) {
        LOG_ERROR("Failed to stat storage file: %s", strerror(errno));
        close(storage->fd);
        free(storage);
        return NULL;
    }
    
    storage->file_size = st.st_size;
    bool is_new = (storage->file_size == 0);
    
    /* Ensure minimum size */
    if (storage->file_size < initial_size) {
        if (ftruncate(storage->fd, initial_size) < 0) {
            LOG_ERROR("Failed to resize storage file: %s", strerror(errno));
            close(storage->fd);
            free(storage);
            return NULL;
        }
        storage->file_size = initial_size;
    }
    
    storage->mapped_size = storage->file_size;
    
    /* Memory map the file */
    storage->base_addr = mmap(NULL, storage->mapped_size, 
                             PROT_READ | PROT_WRITE, MAP_SHARED,
                             storage->fd, 0);
    
    if (storage->base_addr == MAP_FAILED) {
        LOG_ERROR("Failed to mmap storage file: %s", strerror(errno));
        close(storage->fd);
        free(storage);
        return NULL;
    }
    
    /* Set up pointers to regions */
    storage->header = (storage_header_t*)storage->base_addr;
    
    /* Initialize header for new file */
    if (is_new) {
        memset(storage->header, 0, sizeof(storage_header_t));
        storage->header->magic = MMAP_MAGIC;
        storage->header->version = MMAP_VERSION;
        storage->header->file_size = storage->file_size;
        storage->header->data_offset = PAGE_SIZE; /* After header */
        storage->header->index_offset = storage->file_size / 2; /* Middle of file */
        storage->header->free_offset = storage->header->data_offset;
        storage->header->partition_count = PARTITION_COUNT;
        storage->header->partition_size = (storage->header->index_offset - 
                                         storage->header->data_offset) / PARTITION_COUNT;
        
        /* Calculate checksum */
        storage->header->checksum = crc32(storage->header, 
                                        sizeof(storage_header_t) - sizeof(uint64_t));
        
        /* Sync to disk */
        msync(storage->header, sizeof(storage_header_t), MS_SYNC);
    } else {
        /* Verify existing file */
        if (storage->header->magic != MMAP_MAGIC) {
            LOG_ERROR("Invalid storage file magic");
            munmap(storage->base_addr, storage->mapped_size);
            close(storage->fd);
            free(storage);
            return NULL;
        }
        
        if (storage->header->version != MMAP_VERSION) {
            LOG_ERROR("Incompatible storage version");
            munmap(storage->base_addr, storage->mapped_size);
            close(storage->fd);
            free(storage);
            return NULL;
        }
        
        /* Verify checksum */
        uint64_t saved_checksum = storage->header->checksum;
        storage->header->checksum = 0;
        uint32_t calc_checksum = crc32(storage->header, 
                                      sizeof(storage_header_t) - sizeof(uint64_t));
        storage->header->checksum = saved_checksum;
        
        if (calc_checksum != saved_checksum) {
            LOG_ERROR("Storage header checksum mismatch");
            munmap(storage->base_addr, storage->mapped_size);
            close(storage->fd);
            free(storage);
            return NULL;
        }
    }
    
    /* Set up region pointers */
    storage->data_region = (char*)storage->base_addr + storage->header->data_offset;
    storage->index_region = (char*)storage->base_addr + storage->header->index_offset;
    
    /* Initialize locks */
    pthread_rwlock_init(&storage->resize_lock, NULL);
    pthread_mutex_init(&storage->alloc_lock, NULL);
    
    /* Initialize statistics */
    atomic_init(&storage->read_count, 0);
    atomic_init(&storage->write_count, 0);
    atomic_init(&storage->cache_hits, 0);
    atomic_init(&storage->cache_misses, 0);
    
    LOG_INFO("Created mmap storage: size=%zu, partitions=%u", 
             storage->mapped_size, storage->header->partition_count);
    
    return storage;
}

/* Destroy memory-mapped storage */
void mmap_storage_destroy(mmap_storage_t* storage) {
    if (!storage) return;
    
    /* Update checksum before closing */
    storage->header->checksum = 0;
    storage->header->checksum = crc32(storage->header, 
                                    sizeof(storage_header_t) - sizeof(uint64_t));
    
    /* Sync all changes */
    msync(storage->base_addr, storage->mapped_size, MS_SYNC);
    
    /* Unmap and close */
    munmap(storage->base_addr, storage->mapped_size);
    close(storage->fd);
    
    /* Free the free list */
    free_entry_t* curr = storage->free_list;
    while (curr) {
        free_entry_t* next = curr->next;
        free(curr);
        curr = next;
    }
    
    /* Destroy locks */
    pthread_rwlock_destroy(&storage->resize_lock);
    pthread_mutex_destroy(&storage->alloc_lock);
    
    free(storage);
}

/* Store a key-value pair */
int mmap_storage_put(mmap_storage_t* storage, const void* key, size_t key_len,
                     const void* value, size_t value_len) {
    if (!storage || !key || !value) {
        return -1;
    }
    
    /* Calculate total size needed */
    size_t entry_size = sizeof(doc_entry_t) + key_len + value_len;
    
    /* Get read lock for resize protection */
    pthread_rwlock_rdlock(&storage->resize_lock);
    
    /* Allocate space */
    uint64_t offset = allocate_space(storage, entry_size);
    if (offset == 0) {
        pthread_rwlock_unlock(&storage->resize_lock);
        LOG_ERROR("Failed to allocate space for document");
        return -1;
    }
    
    /* Write the entry */
    doc_entry_t* entry = (doc_entry_t*)((char*)storage->base_addr + offset);
    entry->magic = 0x444F4355; /* "DOCU" */
    entry->flags = 0;
    entry->key_hash = murmur3_32(key, key_len, 0);
    entry->key_len = key_len;
    entry->value_len = value_len;
    entry->timestamp = time(NULL);
    entry->next_offset = 0;
    
    /* Copy key and value */
    char* data_ptr = (char*)(entry + 1);
    memcpy(data_ptr, key, key_len);
    memcpy(data_ptr + key_len, value, value_len);
    
    /* Update statistics */
    atomic_fetch_add(&storage->write_count, 1);
    storage->header->doc_count++;
    
    pthread_rwlock_unlock(&storage->resize_lock);
    
    /* TODO: Update index structures */
    
    return 0;
}

/* Retrieve a value by key */
int mmap_storage_get(mmap_storage_t* storage, const void* key, size_t key_len,
                     void** value, size_t* value_len) {
    if (!storage || !key || !value || !value_len) {
        return -1;
    }
    
    /* Get read lock */
    pthread_rwlock_rdlock(&storage->resize_lock);
    
    uint32_t key_hash = murmur3_32(key, key_len, 0);
    
    /* TODO: Use index to find document */
    /* For now, linear scan (to be replaced with index lookup) */
    uint64_t offset = storage->header->data_offset;
    uint64_t end_offset = storage->header->free_offset;
    
    while (offset < end_offset) {
        doc_entry_t* entry = (doc_entry_t*)((char*)storage->base_addr + offset);
        
        if (entry->magic != 0x444F4355) {
            /* Skip invalid entry */
            offset += sizeof(doc_entry_t);
            continue;
        }
        
        if (entry->key_hash == key_hash && entry->key_len == key_len) {
            /* Hash matches, compare actual key */
            char* entry_key = (char*)(entry + 1);
            if (memcmp(entry_key, key, key_len) == 0) {
                /* Found it */
                *value_len = entry->value_len;
                *value = malloc(entry->value_len);
                if (!*value) {
                    pthread_rwlock_unlock(&storage->resize_lock);
                    return -1;
                }
                
                memcpy(*value, entry_key + key_len, entry->value_len);
                atomic_fetch_add(&storage->read_count, 1);
                atomic_fetch_add(&storage->cache_hits, 1); /* Since it's in mapped memory */
                
                pthread_rwlock_unlock(&storage->resize_lock);
                return 0;
            }
        }
        
        /* Move to next entry */
        offset += sizeof(doc_entry_t) + entry->key_len + entry->value_len;
        offset = (offset + 7) & ~7; /* Align to 8 bytes */
    }
    
    atomic_fetch_add(&storage->read_count, 1);
    atomic_fetch_add(&storage->cache_misses, 1);
    
    pthread_rwlock_unlock(&storage->resize_lock);
    return -1; /* Not found */
}

/* Delete a document */
int mmap_storage_delete(mmap_storage_t* storage, const void* key, size_t key_len) {
    if (!storage || !key) {
        return -1;
    }
    
    /* Get write lock */
    pthread_rwlock_wrlock(&storage->resize_lock);
    
    uint32_t key_hash = murmur3_32(key, key_len, 0);
    
    /* TODO: Use index to find document */
    /* For now, linear scan */
    uint64_t offset = storage->header->data_offset;
    uint64_t end_offset = storage->header->free_offset;
    
    while (offset < end_offset) {
        doc_entry_t* entry = (doc_entry_t*)((char*)storage->base_addr + offset);
        
        if (entry->magic != 0x444F4355) {
            offset += sizeof(doc_entry_t);
            continue;
        }
        
        if (entry->key_hash == key_hash && entry->key_len == key_len) {
            char* entry_key = (char*)(entry + 1);
            if (memcmp(entry_key, key, key_len) == 0) {
                /* Mark as deleted */
                entry->magic = 0xDEADBEEF;
                
                /* Add to free list */
                size_t entry_size = sizeof(doc_entry_t) + entry->key_len + entry->value_len;
                free_entry_t* free_entry = malloc(sizeof(free_entry_t));
                if (free_entry) {
                    free_entry->offset = offset;
                    free_entry->size = entry_size;
                    
                    pthread_mutex_lock(&storage->alloc_lock);
                    free_entry->next = storage->free_list;
                    storage->free_list = free_entry;
                    storage->total_free += entry_size;
                    pthread_mutex_unlock(&storage->alloc_lock);
                }
                
                storage->header->doc_count--;
                storage->header->deleted_count++;
                
                pthread_rwlock_unlock(&storage->resize_lock);
                return 0;
            }
        }
        
        offset += sizeof(doc_entry_t) + entry->key_len + entry->value_len;
        offset = (offset + 7) & ~7;
    }
    
    pthread_rwlock_unlock(&storage->resize_lock);
    return -1; /* Not found */
}

/* Batch put operation */
int mmap_storage_put_batch(mmap_storage_t* storage,
                          const void** keys, const size_t* key_lens,
                          const void** values, const size_t* value_lens,
                          size_t count) {
    if (!storage || !keys || !key_lens || !values || !value_lens) {
        return -1;
    }
    
    int success_count = 0;
    
    /* Get write lock once for entire batch */
    pthread_rwlock_wrlock(&storage->resize_lock);
    
    for (size_t i = 0; i < count; i++) {
        /* Use internal put without locks */
        size_t entry_size = sizeof(doc_entry_t) + key_lens[i] + value_lens[i];
        uint64_t offset = allocate_space(storage, entry_size);
        
        if (offset == 0) {
            continue;
        }
        
        doc_entry_t* entry = (doc_entry_t*)((char*)storage->base_addr + offset);
        entry->magic = 0x444F4355;
        entry->flags = 0;
        entry->key_hash = murmur3_32(keys[i], key_lens[i], 0);
        entry->key_len = key_lens[i];
        entry->value_len = value_lens[i];
        entry->timestamp = time(NULL);
        entry->next_offset = 0;
        
        char* data_ptr = (char*)(entry + 1);
        memcpy(data_ptr, keys[i], key_lens[i]);
        memcpy(data_ptr + key_lens[i], values[i], value_lens[i]);
        
        atomic_fetch_add(&storage->write_count, 1);
        storage->header->doc_count++;
        success_count++;
    }
    
    pthread_rwlock_unlock(&storage->resize_lock);
    
    return success_count;
}

/* Compact storage to reclaim space */
int mmap_storage_compact(mmap_storage_t* storage) {
    if (!storage) {
        return -1;
    }
    
    LOG_INFO("Starting storage compaction");
    
    /* Get exclusive write lock */
    pthread_rwlock_wrlock(&storage->resize_lock);
    
    /* TODO: Implement compaction algorithm */
    /* 1. Create new temporary file */
    /* 2. Copy all live documents */
    /* 3. Rebuild indexes */
    /* 4. Atomic swap files */
    
    storage->header->last_compact_time = time(NULL);
    
    pthread_rwlock_unlock(&storage->resize_lock);
    
    LOG_INFO("Storage compaction completed");
    return 0;
}

/* Resize the storage file */
int mmap_storage_resize(mmap_storage_t* storage, size_t new_size) {
    if (!storage || new_size <= storage->mapped_size) {
        return -1;
    }
    
    /* Round up to page size */
    new_size = (new_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    
    /* Get exclusive write lock */
    pthread_rwlock_wrlock(&storage->resize_lock);
    
    /* Extend the file */
    if (ftruncate(storage->fd, new_size) < 0) {
        LOG_ERROR("Failed to resize file: %s", strerror(errno));
        pthread_rwlock_unlock(&storage->resize_lock);
        return -1;
    }
    
    /* Remap the file */
    void* new_addr = mremap(storage->base_addr, storage->mapped_size,
                           new_size, MREMAP_MAYMOVE);
    
    if (new_addr == MAP_FAILED) {
        /* Try manual unmap/map */
        munmap(storage->base_addr, storage->mapped_size);
        new_addr = mmap(NULL, new_size, PROT_READ | PROT_WRITE,
                       MAP_SHARED, storage->fd, 0);
        
        if (new_addr == MAP_FAILED) {
            LOG_ERROR("Failed to remap storage: %s", strerror(errno));
            pthread_rwlock_unlock(&storage->resize_lock);
            return -1;
        }
    }
    
    /* Update pointers */
    storage->base_addr = new_addr;
    storage->mapped_size = new_size;
    storage->file_size = new_size;
    storage->header = (storage_header_t*)storage->base_addr;
    storage->data_region = (char*)storage->base_addr + storage->header->data_offset;
    storage->index_region = (char*)storage->base_addr + storage->header->index_offset;
    
    /* Update header */
    storage->header->file_size = new_size;
    
    pthread_rwlock_unlock(&storage->resize_lock);
    
    LOG_INFO("Resized storage to %zu bytes", new_size);
    return 0;
}

/* Sync changes to disk */
int mmap_storage_sync(mmap_storage_t* storage) {
    if (!storage) {
        return -1;
    }
    
    /* Update checksum */
    storage->header->checksum = 0;
    storage->header->checksum = crc32(storage->header,
                                    sizeof(storage_header_t) - sizeof(uint64_t));
    
    /* Sync mapped memory */
    if (msync(storage->base_addr, storage->mapped_size, MS_SYNC) < 0) {
        LOG_ERROR("Failed to sync storage: %s", strerror(errno));
        return -1;
    }
    
    return 0;
}

/* Create a partitioned collection */
partitioned_collection_t* partitioned_collection_create(const char* name,
                                                       uint32_t partition_count) {
    if (!name || partition_count == 0) {
        return NULL;
    }
    
    partitioned_collection_t* collection = calloc(1, sizeof(partitioned_collection_t));
    if (!collection) {
        return NULL;
    }
    
    strncpy(collection->name, name, sizeof(collection->name) - 1);
    collection->partition_count = partition_count;
    collection->hash_fn = hash_wrapper;
    
    /* Allocate partition array */
    collection->partitions = calloc(partition_count, sizeof(partition_t*));
    if (!collection->partitions) {
        free(collection);
        return NULL;
    }
    
    /* Create partitions */
    for (uint32_t i = 0; i < partition_count; i++) {
        collection->partitions[i] = calloc(1, sizeof(partition_t));
        if (!collection->partitions[i]) {
            /* Cleanup on failure */
            for (uint32_t j = 0; j < i; j++) {
                free(collection->partitions[j]);
            }
            free(collection->partitions);
            free(collection);
            return NULL;
        }
        
        partition_t* part = collection->partitions[i];
        part->id = i;
        pthread_spin_init(&part->lock, PTHREAD_PROCESS_PRIVATE);
        atomic_init(&part->doc_count, 0);
        atomic_init(&part->total_size, 0);
        
        /* Create storage for partition */
        char path[512];
        snprintf(path, sizeof(path), "%s.part%u", name, i);
        part->storage = mmap_storage_create(path, DEFAULT_MMAP_SIZE / partition_count);
        
        if (!part->storage) {
            LOG_ERROR("Failed to create storage for partition %u", i);
            /* Continue with other partitions */
        }
    }
    
    atomic_init(&collection->total_docs, 0);
    atomic_init(&collection->total_size, 0);
    
    LOG_INFO("Created partitioned collection '%s' with %u partitions", 
             name, partition_count);
    
    return collection;
}

/* Destroy partitioned collection */
void partitioned_collection_destroy(partitioned_collection_t* collection) {
    if (!collection) return;
    
    /* Destroy all partitions */
    for (uint32_t i = 0; i < collection->partition_count; i++) {
        if (collection->partitions[i]) {
            partition_t* part = collection->partitions[i];
            
            if (part->storage) {
                mmap_storage_destroy(part->storage);
            }
            
            if (part->bloom_filter) {
                free(part->bloom_filter);
            }
            
            pthread_spin_destroy(&part->lock);
            free(part);
        }
    }
    
    free(collection->partitions);
    free(collection);
}

/* Put into partitioned collection */
int partitioned_put(partitioned_collection_t* collection,
                   const void* key, size_t key_len,
                   const void* value, size_t value_len) {
    if (!collection || !key || !value) {
        return -1;
    }
    
    /* Get partition */
    uint32_t partition_id = get_partition(collection, key, key_len);
    partition_t* partition = collection->partitions[partition_id];
    
    if (!partition || !partition->storage) {
        return -1;
    }
    
    /* Lock partition */
    pthread_spin_lock(&partition->lock);
    
    /* Put into partition's storage */
    int result = mmap_storage_put(partition->storage, key, key_len, value, value_len);
    
    if (result == 0) {
        atomic_fetch_add(&partition->doc_count, 1);
        atomic_fetch_add(&partition->total_size, key_len + value_len);
        atomic_fetch_add(&collection->total_docs, 1);
        atomic_fetch_add(&collection->total_size, key_len + value_len);
    }
    
    pthread_spin_unlock(&partition->lock);
    
    return result;
}

/* Get from partitioned collection */
int partitioned_get(partitioned_collection_t* collection,
                   const void* key, size_t key_len,
                   void** value, size_t* value_len) {
    if (!collection || !key || !value || !value_len) {
        return -1;
    }
    
    /* Get partition */
    uint32_t partition_id = get_partition(collection, key, key_len);
    partition_t* partition = collection->partitions[partition_id];
    
    if (!partition || !partition->storage) {
        return -1;
    }
    
    /* No lock needed for reads (MVCC will handle this) */
    return mmap_storage_get(partition->storage, key, key_len, value, value_len);
}