/*
 * Storage Backend Abstraction Layer
 * Supports both mmap and JDBX storage backends
 */

#include <stdlib.h>
#include <string.h>
#include "storage/storage_backend.h"
#include "storage/mmap_storage.h"
#include "storage/jdbx.h"
#include "utils/logger.h"

/* MMAP storage implementation wrapper */
typedef struct {
    mmap_storage_t* storage;
} mmap_backend_t;

/* JDBX storage implementation wrapper */
typedef struct {
    jdbx_page_manager_t* pm;
    jdbx_btree_t* btree;
} jdbx_backend_t;

/* MMAP iterator */
typedef struct {
    storage_backend_t* backend;
    mmap_storage_t* storage;
    size_t current_index;
} mmap_iterator_t;

/* JDBX iterator */
typedef struct {
    storage_backend_t* backend;
    jdbx_btree_t* btree;
    jdbx_btree_iterator_t* iter;
} jdbx_iterator_t;

/*==============================================================================
 * MMAP Storage Operations
 *============================================================================*/

static int mmap_init(storage_backend_t* backend, const char* path, size_t initial_size) {
    mmap_backend_t* mmap = (mmap_backend_t*)backend->impl;
    mmap->storage = mmap_storage_create(path, initial_size);
    return mmap->storage ? 0 : -1;
}

static int mmap_store(storage_backend_t* backend, const char* id, const char* data, size_t size) {
    mmap_backend_t* mmap = (mmap_backend_t*)backend->impl;
    return mmap_storage_put(mmap->storage, id, strlen(id), data, size);
}

static char* mmap_retrieve(storage_backend_t* backend, const char* id, size_t* size) {
    mmap_backend_t* mmap = (mmap_backend_t*)backend->impl;
    void* value = NULL;
    if (mmap_storage_get(mmap->storage, id, strlen(id), &value, size) == 0) {
        return (char*)value;
    }
    return NULL;
}

static int mmap_delete(storage_backend_t* backend, const char* id) {
    mmap_backend_t* mmap = (mmap_backend_t*)backend->impl;
    return mmap_storage_delete(mmap->storage, id, strlen(id));
}

static storage_iterator_t* mmap_iterator_create(storage_backend_t* backend) {
    mmap_iterator_t* iter = calloc(1, sizeof(mmap_iterator_t));
    if (!iter) return NULL;
    
    iter->backend = backend;
    iter->storage = ((mmap_backend_t*)backend->impl)->storage;
    iter->current_index = 0;
    
    return (storage_iterator_t*)iter;
}

static int mmap_iterator_next(storage_iterator_t* iter, char** id, char** data, size_t* size) {
    mmap_iterator_t* mmap_iter = (mmap_iterator_t*)iter;
    /* TODO: Implement proper mmap iteration */
    /* For now, return -1 to indicate end of iteration */
    (void)mmap_iter;
    (void)id;
    (void)data;
    (void)size;
    return -1;
}

static void mmap_iterator_destroy(storage_iterator_t* iter) {
    free(iter);
}

static void mmap_get_stats(storage_backend_t* backend, uint64_t* doc_count, uint64_t* total_size) {
    mmap_backend_t* mmap = (mmap_backend_t*)backend->impl;
    /* TODO: Implement proper mmap stats */
    (void)mmap;
    *doc_count = 0;
    *total_size = 0;
}

static void mmap_destroy(storage_backend_t* backend) {
    mmap_backend_t* mmap = (mmap_backend_t*)backend->impl;
    if (mmap->storage) {
        mmap_storage_destroy(mmap->storage);
    }
}

static storage_ops_t mmap_ops = {
    .init = mmap_init,
    .store = mmap_store,
    .retrieve = mmap_retrieve,
    .delete = mmap_delete,
    .iterator_create = mmap_iterator_create,
    .iterator_next = mmap_iterator_next,
    .iterator_destroy = mmap_iterator_destroy,
    .get_stats = mmap_get_stats,
    .destroy = mmap_destroy
};

/*==============================================================================
 * JDBX Storage Operations
 *============================================================================*/

static int jdbx_init(storage_backend_t* backend, const char* path, size_t initial_size) {
    jdbx_backend_t* jdbx = (jdbx_backend_t*)backend->impl;
    
    /* Try to open existing database first */
    jdbx->pm = jdbx_open(path);
    if (!jdbx->pm) {
        /* Create new database */
        jdbx->pm = jdbx_create(path, initial_size);
        if (!jdbx->pm) {
            LOG_ERROR("Failed to create JDBX database at %s", path);
            return -1;
        }
        
        /* Create B-tree for document storage */
        jdbx->btree = jdbx_btree_create(jdbx->pm, NULL);
        if (!jdbx->btree) {
            LOG_ERROR("Failed to create JDBX B-tree");
            jdbx_close(jdbx->pm);
            jdbx->pm = NULL;
            return -1;
        }
        
        /* Store B-tree root page in header reserved area */
        /* TODO: Add proper metadata storage in JDBX header */
        
    } else {
        /* Open existing B-tree */
        /* TODO: Read B-tree root page from header */
        uint64_t root_page = 3; /* Hardcoded for now */
        jdbx->btree = jdbx_btree_open(jdbx->pm, root_page, NULL);
        if (!jdbx->btree) {
            LOG_ERROR("Failed to open JDBX B-tree");
            jdbx_close(jdbx->pm);
            jdbx->pm = NULL;
            return -1;
        }
    }
    
    return 0;
}

static int jdbx_store(storage_backend_t* backend, const char* id, const char* data, size_t size) {
    jdbx_backend_t* jdbx = (jdbx_backend_t*)backend->impl;
    return jdbx_btree_insert(jdbx->btree, id, strlen(id), data, size);
}

static char* jdbx_retrieve(storage_backend_t* backend, const char* id, size_t* size) {
    jdbx_backend_t* jdbx = (jdbx_backend_t*)backend->impl;
    void* value = NULL;
    size_t value_len = 0;
    
    if (jdbx_btree_get(jdbx->btree, id, strlen(id), &value, &value_len) == 0) {
        *size = value_len;
        return (char*)value;  /* Caller must free */
    }
    
    return NULL;
}

static int jdbx_delete(storage_backend_t* backend, const char* id) {
    jdbx_backend_t* jdbx = (jdbx_backend_t*)backend->impl;
    return jdbx_btree_delete(jdbx->btree, id, strlen(id));
}

static storage_iterator_t* jdbx_iterator_create(storage_backend_t* backend) {
    jdbx_iterator_t* iter = calloc(1, sizeof(jdbx_iterator_t));
    if (!iter) return NULL;
    
    iter->backend = backend;
    iter->btree = ((jdbx_backend_t*)backend->impl)->btree;
    iter->iter = jdbx_btree_iterator_create(iter->btree);
    
    if (!iter->iter) {
        free(iter);
        return NULL;
    }
    
    return (storage_iterator_t*)iter;
}

static int jdbx_iterator_next(storage_iterator_t* iter, char** id, char** data, size_t* size) {
    jdbx_iterator_t* jdbx_iter = (jdbx_iterator_t*)iter;
    void* key;
    size_t key_len;
    void* value;
    size_t value_len;
    
    if (jdbx_btree_iterator_next(jdbx_iter->iter, &key, &key_len, &value, &value_len) != 0) {
        return -1;
    }
    
    /* Copy key as ID */
    *id = malloc(key_len + 1);
    if (!*id) {
        free(key);
        free(value);
        return -1;
    }
    memcpy(*id, key, key_len);
    (*id)[key_len] = '\0';
    
    /* Copy value as data */
    *data = malloc(value_len);
    if (!*data) {
        free(*id);
        free(key);
        free(value);
        return -1;
    }
    memcpy(*data, value, value_len);
    *size = value_len;
    
    free(key);
    free(value);
    return 0;
}

static void jdbx_iterator_destroy(storage_iterator_t* iter) {
    jdbx_iterator_t* jdbx_iter = (jdbx_iterator_t*)iter;
    if (jdbx_iter) {
        if (jdbx_iter->iter) {
            jdbx_btree_iterator_destroy(jdbx_iter->iter);
        }
        free(jdbx_iter);
    }
}

static void jdbx_get_stats(storage_backend_t* backend, uint64_t* doc_count, uint64_t* total_size) {
    jdbx_backend_t* jdbx = (jdbx_backend_t*)backend->impl;
    (void)jdbx; /* Unused for now */
    /* TODO: Get statistics from JDBX */
    *doc_count = 0;
    *total_size = 0;
}

static void jdbx_destroy(storage_backend_t* backend) {
    jdbx_backend_t* jdbx = (jdbx_backend_t*)backend->impl;
    if (jdbx->btree) {
        jdbx_btree_close(jdbx->btree);
    }
    if (jdbx->pm) {
        jdbx_close(jdbx->pm);
    }
}

static storage_ops_t jdbx_ops = {
    .init = jdbx_init,
    .store = jdbx_store,
    .retrieve = jdbx_retrieve,
    .delete = jdbx_delete,
    .iterator_create = jdbx_iterator_create,
    .iterator_next = jdbx_iterator_next,
    .iterator_destroy = jdbx_iterator_destroy,
    .get_stats = jdbx_get_stats,
    .destroy = jdbx_destroy
};

/*==============================================================================
 * Public API
 *============================================================================*/

storage_backend_t* storage_backend_create(storage_backend_type_t type) {
    storage_backend_t* backend = calloc(1, sizeof(storage_backend_t));
    if (!backend) return NULL;
    
    backend->type = type;
    
    switch (type) {
        case STORAGE_BACKEND_MMAP:
            backend->ops = &mmap_ops;
            backend->impl = calloc(1, sizeof(mmap_backend_t));
            break;
            
        case STORAGE_BACKEND_JDBX:
            backend->ops = &jdbx_ops;
            backend->impl = calloc(1, sizeof(jdbx_backend_t));
            break;
            
        default:
            free(backend);
            return NULL;
    }
    
    if (!backend->impl) {
        free(backend);
        return NULL;
    }
    
    return backend;
}

void storage_backend_destroy(storage_backend_t* backend) {
    if (!backend) return;
    
    if (backend->ops && backend->ops->destroy) {
        backend->ops->destroy(backend);
    }
    
    free(backend->impl);
    free(backend);
}