#ifndef INDEX_METADATA_H
#define INDEX_METADATA_H

#include <stdbool.h>
#include <stdint.h>

/* Forward declaration */
struct database;

/* Index type enumeration - using different name to avoid conflict */
typedef enum {
    INDEX_META_TYPE_HASH,      /* Hash index for equality queries */
    INDEX_META_TYPE_BTREE,     /* B-tree for range queries */
    INDEX_META_TYPE_FULLTEXT   /* Full-text search index */
} index_meta_type_t;

/* Index metadata structure */
typedef struct index_metadata {
    char* field_name;         /* Field being indexed */
    index_meta_type_t type;   /* Type of index */
    bool unique;              /* Enforce uniqueness */
    bool sparse;              /* Only index documents with this field */
    bool case_insensitive;    /* Case-insensitive comparisons */
    
    /* Function hooks */
    char* transform_function; /* Function to transform values before indexing */
    char* validate_function;  /* Function to validate uniqueness rules */
} index_metadata_t;

/* Collection index configuration */
typedef struct collection_indexes {
    char* collection_name;
    index_metadata_t* indexes;
    size_t index_count;
} collection_indexes_t;

/* Create index metadata */
index_metadata_t* index_metadata_create(const char* field_name, 
                                       index_meta_type_t type,
                                       bool unique);

/* Free index metadata */
void index_metadata_free(index_metadata_t* metadata);

/* Check unique constraint */
int index_check_unique_constraint(struct database* db,
                                 const char* collection,
                                 const char* field,
                                 const void* value,
                                 size_t value_len,
                                 const char* exclude_id);

#endif /* INDEX_METADATA_H */