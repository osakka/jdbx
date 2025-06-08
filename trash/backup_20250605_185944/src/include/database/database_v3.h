#ifndef DATABASE_V3_H
#define DATABASE_V3_H

#include <stdatomic.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include "utils/json.h"

/* Maximum number of collections */
#define DB_MAX_COLLECTIONS 1024

/* Forward declarations */
typedef struct hp_collection hp_collection_t;

/* Collection structure for API compatibility */
typedef struct collection {
    char name[256];
    void* hp_impl;  /* Points to hp_collection_t */
} collection_t;

/* Database statistics */
typedef struct {
    size_t num_collections;
    size_t total_documents;
    size_t total_size;
    size_t total_inserts;
    size_t total_queries;
    size_t total_updates;
    size_t total_deletes;
    int cache_enabled;
    size_t cache_hits;
    size_t cache_misses;
} db_stats_t;

/* Database structure */
typedef struct database {
    char path[1024];
    collection_t** collections;
    size_t num_collections;
    pthread_rwlock_t lock;
    
    /* Statistics */
    atomic_uint_fast64_t total_collections;
    atomic_uint_fast64_t total_documents;
    atomic_uint_fast64_t total_size;
    
    /* Configuration */
    int persistence_enabled;
    int persistence_interval;
} database_t;

/* Database initialization and lifecycle */
int db_init(const char* path);
void db_close(void);
int db_save(void);
int db_load(void);

/* Collection operations */
collection_t* db_create_collection(const char* name);
collection_t* db_get_collection(const char* name);
int db_delete_collection(const char* name);
char** db_list_collections(size_t* count);

/* Document operations */
int db_insert_document(const char* collection_name, cjson_t* doc, char* id, size_t id_max_len);
cjson_t* db_get_document(const char* collection_name, const char* id);
int db_update_document(const char* collection_name, const char* id, cjson_t* doc);
int db_delete_document(const char* collection_name, const char* id);
cjson_t* db_query_documents(const char* collection_name, cjson_t* query, 
                           int offset, int limit, cjson_t* sort);

/* Cache operations */
int db_enable_cache(const char* collection_name);
int db_disable_cache(const char* collection_name);
void db_clear_cache(const char* collection_name);

/* Statistics */
void db_get_stats(db_stats_t* stats);
cjson_t* db_get_collection_stats(const char* collection_name);

/* Index operations */
int db_create_index(const char* collection_name, const char* field_name);

/* Schema operations */
int db_set_collection_schema(const char* collection_name, cjson_t* schema);

/* Bootstrap and system operations */
int db_needs_bootstrap(void);
int db_init_system_schemas(void);
int db_rebuild_indices(void);

/* Internal use */
database_t* db_get_instance(void);

#endif /* DATABASE_V3_H */