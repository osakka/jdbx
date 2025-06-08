#ifndef PRODUCTION_CONFIG_H
#define PRODUCTION_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

/* Production configuration for billion-document scale */

/* Memory configuration */
typedef struct {
    uint64_t mmap_size_per_collection;  /* Size in bytes for each collection */
    uint64_t cache_size_total;          /* Total cache size in bytes */
    uint64_t buffer_pool_size;          /* Buffer pool size in bytes */
    uint32_t index_batch_size;          /* Batch size for index updates */
    bool lazy_indexing;                 /* Enable lazy index updates */
    bool preallocate_storage;           /* Pre-allocate full MMAP size */
} production_config_t;

/* Default configurations */
#define PROD_CONFIG_DEVELOPMENT ((production_config_t){     \
    .mmap_size_per_collection = 256ULL * 1024 * 1024,      /* 256MB */  \
    .cache_size_total = 50ULL * 1024 * 1024,               /* 50MB */   \
    .buffer_pool_size = 10ULL * 1024 * 1024,               /* 10MB */   \
    .index_batch_size = 100,                                            \
    .lazy_indexing = false,                                             \
    .preallocate_storage = false                                        \
})

#define PROD_CONFIG_SMALL ((production_config_t){           \
    .mmap_size_per_collection = 1ULL * 1024 * 1024 * 1024,  /* 1GB */   \
    .cache_size_total = 256ULL * 1024 * 1024,              /* 256MB */  \
    .buffer_pool_size = 64ULL * 1024 * 1024,               /* 64MB */   \
    .index_batch_size = 1000,                                           \
    .lazy_indexing = true,                                              \
    .preallocate_storage = true                                         \
})

#define PROD_CONFIG_MEDIUM ((production_config_t){          \
    .mmap_size_per_collection = 10ULL * 1024 * 1024 * 1024, /* 10GB */  \
    .cache_size_total = 1ULL * 1024 * 1024 * 1024,         /* 1GB */    \
    .buffer_pool_size = 256ULL * 1024 * 1024,              /* 256MB */  \
    .index_batch_size = 10000,                                          \
    .lazy_indexing = true,                                              \
    .preallocate_storage = true                                         \
})

#define PROD_CONFIG_LARGE ((production_config_t){           \
    .mmap_size_per_collection = 100ULL * 1024 * 1024 * 1024,/* 100GB */ \
    .cache_size_total = 4ULL * 1024 * 1024 * 1024,         /* 4GB */    \
    .buffer_pool_size = 1ULL * 1024 * 1024 * 1024,         /* 1GB */    \
    .index_batch_size = 100000,                                         \
    .lazy_indexing = true,                                              \
    .preallocate_storage = true                                         \
})

/* Global configuration instance */
extern production_config_t g_production_config;

/* Configuration functions */
void production_config_init(const char* config_level);
void production_config_set(const production_config_t* config);
const production_config_t* production_config_get(void);
void production_config_print(void);

/* Helper to get human-readable size */
const char* format_bytes(uint64_t bytes);

#endif /* PRODUCTION_CONFIG_H */