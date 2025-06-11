#ifndef STORAGE_BACKEND_H
#define STORAGE_BACKEND_H

#include <stddef.h>
#include <stdint.h>
#include "utils/json.h"

/* Storage backend types */
typedef enum {
    STORAGE_BACKEND_MMAP,    /* Memory-mapped file storage */
    STORAGE_BACKEND_JDBX     /* JDBX single-file storage */
} storage_backend_type_t;

/* Forward declarations */
typedef struct storage_backend storage_backend_t;
typedef struct storage_iterator storage_iterator_t;

/* Storage operations */
typedef struct {
    /* Initialize storage */
    int (*init)(storage_backend_t* backend, const char* path, size_t initial_size);
    
    /* Store a document */
    int (*store)(storage_backend_t* backend, const char* id, const char* data, size_t size);
    
    /* Retrieve a document */
    char* (*retrieve)(storage_backend_t* backend, const char* id, size_t* size);
    
    /* Delete a document */
    int (*delete)(storage_backend_t* backend, const char* id);
    
    /* Create iterator */
    storage_iterator_t* (*iterator_create)(storage_backend_t* backend);
    
    /* Iterator operations */
    int (*iterator_next)(storage_iterator_t* iter, char** id, char** data, size_t* size);
    void (*iterator_destroy)(storage_iterator_t* iter);
    
    /* Get statistics */
    void (*get_stats)(storage_backend_t* backend, uint64_t* doc_count, uint64_t* total_size);
    
    /* Cleanup */
    void (*destroy)(storage_backend_t* backend);
} storage_ops_t;

/* Generic storage backend */
struct storage_backend {
    storage_backend_type_t type;
    storage_ops_t* ops;
    void* impl;  /* Implementation-specific data */
};

/* Create a storage backend */
storage_backend_t* storage_backend_create(storage_backend_type_t type);

/* Destroy a storage backend */
void storage_backend_destroy(storage_backend_t* backend);

#endif /* STORAGE_BACKEND_H */