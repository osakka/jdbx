#ifndef QUERY_LANGUAGE_H
#define QUERY_LANGUAGE_H

#include "utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Query operator types */
typedef enum {
    /* Comparison operators */
    OP_EQ,         /* Equal to ($eq) */
    OP_NE,         /* Not equal to ($ne) */
    OP_GT,         /* Greater than ($gt) */
    OP_GTE,        /* Greater than or equal to ($gte) */
    OP_LT,         /* Less than ($lt) */
    OP_LTE,        /* Less than or equal to ($lte) */
    OP_IN,         /* In array ($in) */
    OP_NIN,        /* Not in array ($nin) */
    
    /* Logical operators */
    OP_AND,        /* Logical AND ($and) */
    OP_OR,         /* Logical OR ($or) */
    OP_NOT,        /* Logical NOT ($not) */
    OP_NOR,        /* Logical NOR ($nor) */
    
    /* Array operators */
    OP_ALL,        /* Array contains all values ($all) */
    OP_ELEM_MATCH, /* Array contains element matching condition ($elemMatch) */
    OP_SIZE,       /* Array has size ($size) */
    
    /* Special operators */
    OP_EXISTS,     /* Field exists ($exists) */
    OP_TYPE,       /* Field is of type ($type) */
    
    /* Internal operators (not directly exposed in query language) */
    OP_FIELD,      /* Direct field comparison (implicit) */
    OP_ROOT        /* Root of query expression tree */
} query_operator_t;

/* Query expression node */
typedef struct query_expr {
    query_operator_t op;      /* Operator type */
    char* field_path;         /* Field path (for field operators) */
    json_value_t* value;      /* Value to compare against (for comparison operators) */
    struct query_expr** children; /* Child expressions (for logical operators) */
    int num_children;         /* Number of child expressions */
} query_expr_t;

/* Pagination type */
typedef enum {
    PAGINATION_NONE,          /* No pagination */
    PAGINATION_OFFSET,        /* Offset-based pagination (skip/limit) */
    PAGINATION_CURSOR         /* Cursor-based pagination */
} pagination_type_t;

/* Query options */
typedef struct {
    int limit;                /* Maximum number of results (0 = no limit) */
    int skip;                 /* Number of results to skip */
    int page;                 /* Current page number (1-based, for pagination) */
    int page_size;            /* Number of documents per page (for pagination) */
    pagination_type_t pagination_type; /* Type of pagination to use */
    char* cursor;             /* Cursor for cursor-based pagination */
    json_value_t* sort;       /* Sort specification */
    json_value_t* projection; /* Field projection */
} query_options_t;

/* Query parse result */
typedef struct {
    query_expr_t* expr;       /* Root of query expression tree */
    query_options_t options;  /* Query options */
    char* error;              /* Error message (NULL if no error) */
} query_parse_result_t;

/* Pagination info */
typedef struct {
    int total_count;          /* Total number of matching documents */
    int page_count;           /* Number of documents in current page */
    int page;                 /* Current page number (1-based) */
    int page_size;            /* Number of documents per page */
    int total_pages;          /* Total number of pages */
    int has_next_page;        /* 1 if there is a next page, 0 otherwise */
    int has_prev_page;        /* 1 if there is a previous page, 0 otherwise */
    char* next_cursor;        /* Cursor for next page (for cursor-based pagination) */
    char* prev_cursor;        /* Cursor for previous page (for cursor-based pagination) */
} pagination_info_t;

/* Query result */
typedef struct {
    json_value_t* documents;  /* Array of matching documents */
    int count;                /* Number of documents in this result set */
    int total_count;          /* Total number of matching documents (before pagination) */
    pagination_info_t* pagination; /* Pagination information (NULL if pagination not used) */
} query_result_t;

/* Parse a query JSON into a query expression tree */
query_parse_result_t query_parse(json_value_t* query_json);

/* Execute a query against a collection of documents */
query_result_t query_execute(query_expr_t* expr, json_value_t* documents, query_options_t* options);

/* Free a query expression tree */
void query_free_expr(query_expr_t* expr);

/* Free a query parse result */
void query_free_parse_result(query_parse_result_t* result);

/* Check if a document matches a query expression */
int query_match_document(query_expr_t* expr, json_value_t* document);

/* Extract a value from a document by field path */
json_value_t* query_extract_field(json_value_t* document, const char* field_path);

/* Apply projection to a document */
json_value_t* query_apply_projection(json_value_t* document, json_value_t* projection);

/* Apply sort to result documents */
void query_apply_sort(json_value_t* documents, json_value_t* sort);

/* Parse query options (limit, skip, sort, projection) */
query_options_t query_parse_options(json_value_t* options_json);

/* Create a query expression node */
query_expr_t* query_create_expr(query_operator_t op);

/* Create a field comparison expression */
query_expr_t* query_create_field_expr(const char* field_path, query_operator_t op, json_value_t* value);

/* Create a logical operator expression with child expressions */
query_expr_t* query_create_logical_expr(query_operator_t op, query_expr_t** children, int num_children);

/* Get operator name string from type */
const char* query_operator_name(query_operator_t op);

/* Parse operator type from name */
query_operator_t query_parse_operator(const char* op_name);

/* Create pagination info from query options and results */
pagination_info_t* query_create_pagination_info(query_options_t* options, int total_count, json_value_t* documents);

/* Free pagination info */
void query_free_pagination_info(pagination_info_t* pagination);

/* Convert pagination info to JSON */
json_value_t* query_pagination_to_json(pagination_info_t* pagination);

/* Generate cursor for cursor-based pagination */
char* query_generate_cursor(json_value_t* document, query_options_t* options);

/* Parse cursor for cursor-based pagination */
int query_parse_cursor(const char* cursor, query_options_t* options);

#endif /* QUERY_LANGUAGE_H */