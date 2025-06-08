#ifndef BATCH_OPERATIONS_H
#define BATCH_OPERATIONS_H

#include <stdbool.h>
#include <stddef.h>
#include "database/database.h"
#include "utils/json.h"

/* Batch operation result */
typedef struct {
    size_t total_documents;      /* Total documents in batch */
    size_t successful_inserts;   /* Successfully inserted */
    size_t failed_inserts;       /* Failed to insert */
    double total_time_ms;        /* Total operation time */
    json_value_t* errors;        /* Array of error details */
} batch_insert_result_t;

/* Batch insert options */
typedef struct {
    bool validate_schema;        /* Validate against schema */
    bool ignore_duplicates;      /* Skip duplicate documents */
    bool atomic_operation;       /* All or nothing */
    size_t batch_size;          /* Internal processing batch size */
    bool defer_indexing;        /* Defer index updates until end */
} batch_insert_options_t;

/* Default batch options */
#define BATCH_INSERT_OPTIONS_DEFAULT ((batch_insert_options_t){  \
    .validate_schema = false,                                    \
    .ignore_duplicates = false,                                  \
    .atomic_operation = false,                                   \
    .batch_size = 1000,                                         \
    .defer_indexing = true                                      \
})

/* Batch insert API */
batch_insert_result_t* db_batch_insert_documents(
    database_t* db,
    const char* collection_name,
    json_value_t* documents,        /* Array of documents */
    const batch_insert_options_t* options
);

/* Free batch result */
void batch_insert_result_free(batch_insert_result_t* result);

/* Batch update API (future) */
typedef struct {
    json_value_t* filter;          /* Query filter */
    json_value_t* update;          /* Update operations */
    bool multi;                    /* Update multiple documents */
} batch_update_operation_t;

/* Batch delete API (future) */
typedef struct {
    json_value_t* filter;          /* Query filter */
    bool multi;                    /* Delete multiple documents */
} batch_delete_operation_t;

#endif /* BATCH_OPERATIONS_H */