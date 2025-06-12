#ifndef JSONDB_JDBX_V2_H
#define JSONDB_JDBX_V2_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "utils/json.h"
#include "storage/jdbx.h"

/* JDBX v2 - Hierarchical Storage with Libraries → Collections → Documents */

#define JDBX_V2_MAGIC "JDB2"
#define JDBX_V2_VERSION 2
#define JDBX_MAX_LIBRARIES 256
#define JDBX_MAX_COLLECTIONS_PER_LIBRARY 1024
#define JDBX_MAX_INDEXES_PER_COLLECTION 16

/* Forward declarations */
typedef struct jdbx_database_v2 jdbx_database_v2_t;
typedef struct jdbx_library jdbx_library_t;
typedef struct jdbx_collection_v2 jdbx_collection_v2_t;

/* Library structure */
struct jdbx_library {
    char name[64];
    uint32_t collections_dir_page;      /* Root page of collections B-tree */
    jdbx_btree_t* collections_btree;    /* Cached B-tree handle */
    json_value_t* metadata;             /* Library metadata */
    pthread_rwlock_t lock;
    bool loaded;
};

/* Collection structure */
struct jdbx_collection_v2 {
    char name[64];
    char library[64];
    uint32_t documents_page;            /* Root page of documents B-tree */
    uint32_t indexes_pages[JDBX_MAX_INDEXES_PER_COLLECTION];
    jdbx_btree_t* documents_btree;      /* Cached B-tree handle */
    jdbx_btree_t* index_btrees[JDBX_MAX_INDEXES_PER_COLLECTION];
    json_value_t* schema;
    json_value_t* metadata;
    pthread_rwlock_t lock;
    bool loaded;
};

/* Main database structure */
struct jdbx_database_v2 {
    /* Core components */
    jdbx_page_manager_t* pm;            /* Page manager */
    jdbx_btree_t* root_dir;            /* Root directory (libraries) */
    
    /* Cached libraries */
    jdbx_library_t libraries[JDBX_MAX_LIBRARIES];
    int num_libraries;
    
    /* Global lock for structural changes */
    pthread_rwlock_t global_lock;
    
    /* Database path */
    char path[256];
};

/* Database lifecycle */
jdbx_database_v2_t* jdbx_v2_open(const char* path, size_t initial_size);
void jdbx_v2_close(jdbx_database_v2_t* db);
int jdbx_v2_sync(jdbx_database_v2_t* db);

/* Library operations */
int jdbx_v2_create_library(jdbx_database_v2_t* db, const char* library_name);
int jdbx_v2_drop_library(jdbx_database_v2_t* db, const char* library_name);
int jdbx_v2_library_exists(jdbx_database_v2_t* db, const char* library_name);
json_value_t* jdbx_v2_list_libraries(jdbx_database_v2_t* db);
json_value_t* jdbx_v2_get_library_metadata(jdbx_database_v2_t* db, const char* library_name);
int jdbx_v2_set_library_metadata(jdbx_database_v2_t* db, const char* library_name, json_value_t* metadata);

/* Collection operations */
int jdbx_v2_create_collection(jdbx_database_v2_t* db, const char* library_name, const char* collection_name);
int jdbx_v2_drop_collection(jdbx_database_v2_t* db, const char* library_name, const char* collection_name);
int jdbx_v2_collection_exists(jdbx_database_v2_t* db, const char* library_name, const char* collection_name);
json_value_t* jdbx_v2_list_collections(jdbx_database_v2_t* db, const char* library_name);

/* Path-based collection operations (library/collection format) */
int jdbx_v2_create_collection_path(jdbx_database_v2_t* db, const char* collection_path);
int jdbx_v2_drop_collection_path(jdbx_database_v2_t* db, const char* collection_path);
int jdbx_v2_collection_exists_path(jdbx_database_v2_t* db, const char* collection_path);

/* Document operations with full paths */
json_value_t* jdbx_v2_insert(jdbx_database_v2_t* db, const char* collection_path, json_value_t* document);
json_value_t* jdbx_v2_find_by_id(jdbx_database_v2_t* db, const char* collection_path, const char* doc_id);
json_value_t* jdbx_v2_find(jdbx_database_v2_t* db, const char* collection_path, json_value_t* query, int limit, int skip);
int jdbx_v2_update(jdbx_database_v2_t* db, const char* collection_path, const char* doc_id, json_value_t* update);
int jdbx_v2_delete(jdbx_database_v2_t* db, const char* collection_path, const char* doc_id);
int jdbx_v2_count(jdbx_database_v2_t* db, const char* collection_path, json_value_t* query);

/* Index operations */
int jdbx_v2_create_index(jdbx_database_v2_t* db, const char* collection_path, 
                        const char* index_name, const char* field_path, bool unique);
int jdbx_v2_drop_index(jdbx_database_v2_t* db, const char* collection_path, const char* index_name);
json_value_t* jdbx_v2_list_indexes(jdbx_database_v2_t* db, const char* collection_path);

/* Schema operations */
int jdbx_v2_set_schema(jdbx_database_v2_t* db, const char* collection_path, json_value_t* schema);
json_value_t* jdbx_v2_get_schema(jdbx_database_v2_t* db, const char* collection_path);

/* Utility functions */
int jdbx_v2_parse_path(const char* path, char* library, size_t lib_size, char* collection, size_t coll_size);

#endif /* JSONDB_JDBX_V2_H */