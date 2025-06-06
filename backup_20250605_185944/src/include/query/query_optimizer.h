#ifndef QUERY_OPTIMIZER_H
#define QUERY_OPTIMIZER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "query/query_language.h"
#include "index/btree_disk.h"

/* Cost-based query optimizer for JSONdb
 * Generates optimal execution plans for complex queries
 */

/* Plan node types */
typedef enum plan_node_type {
    PLAN_SCAN,              /* Full collection scan */
    PLAN_INDEX_SCAN,        /* Index-based scan */
    PLAN_INDEX_ONLY_SCAN,   /* Covering index scan */
    PLAN_BITMAP_SCAN,       /* Bitmap index scan */
    PLAN_HASH_JOIN,         /* Hash-based join */
    PLAN_MERGE_JOIN,        /* Sort-merge join */
    PLAN_NESTED_LOOP_JOIN,  /* Nested loop join */
    PLAN_AGGREGATE,         /* Aggregation */
    PLAN_SORT,              /* Sort operation */
    PLAN_LIMIT,             /* Limit results */
    PLAN_FILTER,            /* Apply predicates */
    PLAN_PROJECT            /* Select fields */
} plan_node_type_t;

/* Cost estimates */
typedef struct plan_cost {
    double startup_cost;    /* Cost before first row */
    double total_cost;      /* Total execution cost */
    size_t rows;           /* Estimated row count */
    size_t width;          /* Average row width */
} plan_cost_t;

/* Scan information */
typedef struct scan_info {
    char* collection_name;
    query_condition_t* filter;
    double selectivity;
} scan_info_t;

/* Index scan information */
typedef struct index_scan_info {
    char* index_name;
    char* collection_name;
    
    /* Index bounds */
    void* start_key;
    size_t start_len;
    void* end_key;
    size_t end_len;
    
    /* Additional filters after index */
    query_condition_t* filter;
    
    /* Covering index fields */
    char** included_fields;
    size_t included_count;
} index_scan_info_t;

/* Join information */
typedef struct join_info {
    char* join_type;        /* INNER, LEFT, RIGHT */
    query_condition_t* condition;
    
    /* For hash joins */
    size_t hash_table_size;
    
    /* For merge joins */
    bool needs_sort_left;
    bool needs_sort_right;
} join_info_t;

/* Aggregation information */
typedef struct aggregate_info {
    char** group_by_fields;
    size_t group_by_count;
    
    /* Aggregate functions */
    struct {
        char* function;     /* COUNT, SUM, AVG, MIN, MAX */
        char* field;
        char* alias;
    }* aggregates;
    size_t aggregate_count;
    
    /* Having clause */
    query_condition_t* having;
} aggregate_info_t;

/* Query plan node */
typedef struct plan_node {
    plan_node_type_t type;
    plan_cost_t cost;
    
    /* Node-specific data */
    union {
        scan_info_t scan;
        index_scan_info_t index_scan;
        join_info_t join;
        aggregate_info_t aggregate;
        struct {
            char** fields;
            size_t count;
        } project;
        struct {
            char** order_by;
            bool* ascending;
            size_t count;
        } sort;
        struct {
            size_t limit;
            size_t offset;
        } limit;
    } data;
    
    /* Child nodes */
    struct plan_node** children;
    size_t child_count;
    
    /* Execution hints */
    bool parallel_safe;
    int parallel_degree;
} plan_node_t;

/* Query plan */
typedef struct query_plan {
    plan_node_t* root;
    double estimated_time_ms;
    size_t estimated_memory_kb;
    
    /* Plan metadata */
    char* query_text;
    uint64_t plan_id;
    time_t created_at;
} query_plan_t;

/* Table statistics for cost estimation */
typedef struct table_stats {
    char* collection_name;
    uint64_t row_count;
    uint64_t total_pages;
    double avg_row_size;
    
    /* Column statistics */
    struct column_stats {
        char* field_name;
        uint64_t distinct_values;
        double null_fraction;
        void* min_value;
        void* max_value;
        
        /* Histogram for selectivity estimation */
        struct {
            void* bound;
            double frequency;
        }* histogram;
        size_t histogram_buckets;
    }* columns;
    size_t column_count;
    
    /* Index statistics */
    struct index_stats {
        char* index_name;
        char** fields;
        size_t field_count;
        uint64_t distinct_keys;
        uint64_t leaf_pages;
        uint32_t height;
    }* indexes;
    size_t index_count;
} table_stats_t;

/* Query optimizer */
typedef struct query_optimizer {
    /* Statistics cache */
    table_stats_t** stats_cache;
    size_t stats_count;
    pthread_rwlock_t stats_lock;
    
    /* Plan cache */
    struct {
        uint64_t query_hash;
        query_plan_t* plan;
    }* plan_cache;
    size_t plan_cache_size;
    size_t max_cache_size;
    pthread_mutex_t cache_lock;
    
    /* Cost model parameters */
    double seq_page_cost;       /* Sequential I/O cost */
    double random_page_cost;    /* Random I/O cost */
    double cpu_tuple_cost;      /* Per-tuple CPU cost */
    double cpu_operator_cost;   /* Per-operation CPU cost */
} query_optimizer_t;

/* Create and destroy optimizer */
query_optimizer_t* query_optimizer_create(void);
void query_optimizer_destroy(query_optimizer_t* optimizer);

/* Generate query plan */
query_plan_t* query_optimize(query_optimizer_t* optimizer,
                            query_t* query,
                            table_stats_t** available_stats,
                            size_t stats_count);

/* Plan operations */
void query_plan_destroy(query_plan_t* plan);
void query_plan_print(query_plan_t* plan, FILE* output);
char* query_plan_to_json(query_plan_t* plan);

/* Statistics management */
table_stats_t* gather_table_stats(const char* collection_name);
void table_stats_destroy(table_stats_t* stats);
void update_stats_cache(query_optimizer_t* optimizer, table_stats_t* stats);

/* Cost estimation helpers */
double estimate_scan_cost(table_stats_t* stats, query_condition_t* filter);
double estimate_index_scan_cost(table_stats_t* stats, 
                               struct index_stats* index,
                               double selectivity);
double estimate_join_cost(plan_cost_t* left, plan_cost_t* right,
                         join_info_t* join_info);

/* Selectivity estimation */
double estimate_selectivity(table_stats_t* stats,
                          query_condition_t* condition);
double estimate_index_selectivity(struct index_stats* index,
                                 void* start_key, void* end_key);

/* Plan transformation rules */
plan_node_t* push_down_predicates(plan_node_t* plan);
plan_node_t* merge_joins(plan_node_t* plan);
plan_node_t* eliminate_sorts(plan_node_t* plan);
plan_node_t* choose_join_order(plan_node_t* plan, query_optimizer_t* optimizer);

#endif /* QUERY_OPTIMIZER_H */