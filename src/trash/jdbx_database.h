#ifndef JDBX_DATABASE_H
#define JDBX_DATABASE_H

#include <stdint.h>
#include <pthread.h>
#include "storage/jdbx.h"
#include "utils/json.h"
#include "utils/generic_cache.h"

/* JDBX Database - Single file, everything inside */

/* Collection metadata stored in JDBX */
typedef struct {
    char name[256];
    uint64_t doc_tree_root;      /* B-tree for documents */
    uint64_t primary_idx_root;   /* B-tree for primary index */
    uint64_t secondary_idx_roots[16]; /* B-trees for secondary indexes */
    char secondary_idx_fields[16][128]; /* Field names for secondary indexes */
    int num_secondary_indexes;
    uint64_t doc_count;
    uint64_t total_size;
    uint64_t created_at;
    uint64_t updated_at;
} jdbx_collection_t;

/* JDBX Database instance */
typedef struct {
    /* Single JDBX file for entire database */
    jdbx_page_manager_t* pm;
    
    /* Collections directory B-tree */
    jdbx_btree_t* collections_dir;
    
    /* Runtime collection cache */
    struct {
        char name[256];
        jdbx_collection_t meta;
        jdbx_btree_t* doc_tree;
        jdbx_btree_t* primary_idx;
        jdbx_btree_t* secondary_idx[16];
        generic_cache_t* doc_cache;
        pthread_rwlock_t lock;
    } collections[1024];
    int num_collections;
    
    /* Global lock for collection operations */
    pthread_rwlock_t global_lock;
    
    /* Statistics */
    struct {
        uint64_t total_collections;
        uint64_t total_documents;
        uint64_t total_size;
    } stats;
} jdbx_database_t;

/* Database operations */
jdbx_database_t* jdbx_database_create(const char* path);
jdbx_database_t* jdbx_database_open(const char* path);
void jdbx_database_close(jdbx_database_t* db);

/* Collection operations */
int jdbx_create_collection(jdbx_database_t* db, const char* name);
int jdbx_drop_collection(jdbx_database_t* db, const char* name);
int jdbx_collection_exists(jdbx_database_t* db, const char* name);
json_value_t* jdbx_list_collections(jdbx_database_t* db);

/* Document operations */
int jdbx_insert_document(jdbx_database_t* db, const char* collection, 
                        const char* id, json_value_t* doc);
json_value_t* jdbx_get_document(jdbx_database_t* db, const char* collection,
                               const char* id);
int jdbx_update_document(jdbx_database_t* db, const char* collection,
                        const char* id, json_value_t* doc);
int jdbx_delete_document(jdbx_database_t* db, const char* collection,
                        const char* id);

/* Query operations */
json_value_t* jdbx_find_documents(jdbx_database_t* db, const char* collection,
                                 json_value_t* query, int limit, int skip);

/* Index operations */
int jdbx_create_index(jdbx_database_t* db, const char* collection,
                     const char* field, int unique);
int jdbx_drop_index(jdbx_database_t* db, const char* collection,
                   const char* field);

/* Statistics */
json_value_t* jdbx_get_stats(jdbx_database_t* db, const char* collection);

#endif /* JDBX_DATABASE_H */