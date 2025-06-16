#include "query/query_language.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/* Hash table for O(1) operator lookup */
#define OPERATOR_HASH_SIZE 32
static query_operator_t operator_hash_table[OPERATOR_HASH_SIZE];
static const char* operator_names[OPERATOR_HASH_SIZE];
static int hash_table_initialized = 0;

/* Simple hash function for operator names */
static unsigned int hash_operator_name(const char* name) {
  unsigned int hash = 5381;
  int c;
  while ((c = *name++)) {
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }
  return hash % OPERATOR_HASH_SIZE;
}

/* Initialize the operator hash table */
static void init_operator_hash_table(void) {
  if (hash_table_initialized) return;
  
  /* Initialize all entries to OP_FIELD (default) */
  for (int i = 0; i < OPERATOR_HASH_SIZE; i++) {
    operator_hash_table[i] = OP_FIELD;
    operator_names[i] = NULL;
  }
  
  /* Populate hash table with known operators */
  static struct {
    const char* name;
    query_operator_t op;
  } operators[] = {
    {"$eq", OP_EQ},
    {"$ne", OP_NE},
    {"$gt", OP_GT},
    {"$gte", OP_GTE},
    {"$lt", OP_LT},
    {"$lte", OP_LTE},
    {"$in", OP_IN},
    {"$nin", OP_NIN},
    {"$and", OP_AND},
    {"$or", OP_OR},
    {"$not", OP_NOT},
    {"$nor", OP_NOR},
    {"$all", OP_ALL},
    {"$elemMatch", OP_ELEM_MATCH},
    {"$size", OP_SIZE},
    {"$exists", OP_EXISTS},
    {"$type", OP_TYPE}
  };
  
  for (size_t i = 0; i < sizeof(operators) / sizeof(operators[0]); i++) {
    unsigned int hash = hash_operator_name(operators[i].name);
    /* Handle collisions with linear probing */
    while (operator_names[hash] != NULL) {
      hash = (hash + 1) % OPERATOR_HASH_SIZE;
    }
    operator_hash_table[hash] = operators[i].op;
    operator_names[hash] = operators[i].name;
  }
  
  hash_table_initialized = 1;
}

/* Old operator_map array replaced with hash table above for O(1) lookup */

/* Get operator name string from type using optimized lookup */
const char* query_operator_name(query_operator_t op) {
  /* Initialize hash table on first use */
  if (!hash_table_initialized) {
    init_operator_hash_table();
  }
  
  /* Linear search through hash table entries (reverse lookup) */
  for (int i = 0; i < OPERATOR_HASH_SIZE; i++) {
    if (operator_names[i] != NULL && operator_hash_table[i] == op) {
      return operator_names[i];
    }
  }
  return "unknown";
}

/* Parse operator type from name using O(1) hash table lookup */
query_operator_t query_parse_operator(const char* op_name) {
  if (!op_name) {
    return OP_FIELD; /* Default to field comparison */
  }
  
  /* Initialize hash table on first use */
  if (!hash_table_initialized) {
    init_operator_hash_table();
  }
  
  /* Hash table lookup with linear probing for collision resolution */
  unsigned int hash = hash_operator_name(op_name);
  for (int probe = 0; probe < OPERATOR_HASH_SIZE; probe++) {
    unsigned int index = (hash + probe) % OPERATOR_HASH_SIZE;
    if (operator_names[index] == NULL) {
      /* Not found */
      break;
    }
    if (strcmp(operator_names[index], op_name) == 0) {
      /* Found */
      return operator_hash_table[index];
    }
  }
  
  return OP_FIELD; /* Default to field comparison */
}

/* Create a query expression node */
query_expr_t* query_create_expr(query_operator_t op) {
  query_expr_t* expr = (query_expr_t*)BUFFER_ALLOC(sizeof(query_expr_t));
  if (!expr) {
    return NULL;
  }
  
  expr->op = op;
  expr->field_path = NULL;
  expr->value = NULL;
  expr->children = NULL;
  expr->num_children = 0;
  
  return expr;
}

/* Create a field comparison expression */
query_expr_t* query_create_field_expr(const char* field_path, query_operator_t op, json_value_t* value) {
  query_expr_t* expr = query_create_expr(op);
  if (!expr) {
    return NULL;
  }
  
  if (field_path) {
    expr->field_path = BUFFER_STRDUP(field_path);
  }
  
  expr->value = value ? json_clone(value) : NULL;
  
  return expr;
}

/* Create a logical operator expression with child expressions */
query_expr_t* query_create_logical_expr(query_operator_t op, query_expr_t** children, int num_children) {
  query_expr_t* expr = query_create_expr(op);
  if (!expr) {
    return NULL;
  }
  
  if (num_children > 0 && children) {
    expr->children = (query_expr_t**)BUFFER_ALLOC(num_children * sizeof(query_expr_t*));
    if (!expr->children) {
      BUFFER_FREE(expr);
      return NULL;
    }
    
    expr->num_children = num_children;
    for (int i = 0; i < num_children; i++) {
      expr->children[i] = children[i];
    }
  }
  
  return expr;
}

/* Free a query expression tree */
void query_free_expr(query_expr_t* expr) {
  if (!expr) {
    return;
  }
  
  if (expr->field_path) {
    BUFFER_FREE(expr->field_path);
  }
  
  if (expr->value) {
    json_free(expr->value);
  }
  
  if (expr->children) {
    for (int i = 0; i < expr->num_children; i++) {
      query_free_expr(expr->children[i]);
    }
    BUFFER_FREE(expr->children);
  }
  
  BUFFER_FREE(expr);
}

/* Forward declarations for recursive parsing */
static query_expr_t* parse_comparison_expr(const char* field_path, json_value_t* value);
static query_expr_t* parse_logical_expr(const char* op_name, json_value_t* array_value);
static query_expr_t* parse_field_expr(const char* field_path, json_value_t* value);

/* Parse a query JSON into a query expression tree */
query_parse_result_t query_parse(json_value_t* query_json) {
  TRACE_DB("Parsing query");

  query_parse_result_t result;
  result.expr = NULL;
  result.options.limit = 0;
  result.options.skip = 0;
  result.options.sort = NULL;
  result.options.projection = NULL;
  result.error = NULL;

  if (!query_json) {
    LOG_ERROR("Query JSON is NULL");
    result.error = BUFFER_STRDUP("Query JSON is NULL");
    return result;
  }

  TRACE_DB("Query JSON type: %d", query_json->type);

  /* Handle complex query format with query, limit, skip, sort, projection */
  if (query_json->type == JSON_OBJECT && json_object_get(query_json, "query")) {
    TRACE_DB("Processing complex query format with options");
    json_value_t* query = json_object_get(query_json, "query");

    /* Parse options */
    json_value_t* limit_val = json_object_get(query_json, "limit");
    if (limit_val && limit_val->type == JSON_INTEGER) {
      result.options.limit = (int)limit_val->value.integer;
      TRACE_DB("Query option: limit = %d", result.options.limit);
    }

    json_value_t* skip_val = json_object_get(query_json, "skip");
    if (skip_val && skip_val->type == JSON_INTEGER) {
      result.options.skip = (int)skip_val->value.integer;
      TRACE_DB("Query option: skip = %d", result.options.skip);
    }

    json_value_t* sort_val = json_object_get(query_json, "sort");
    if (sort_val && sort_val->type == JSON_OBJECT) {
      result.options.sort = json_clone(sort_val);
      TRACE_DB("Query option: sort is present");
    }

    json_value_t* projection_val = json_object_get(query_json, "projection");
    if (projection_val && projection_val->type == JSON_OBJECT) {
      result.options.projection = json_clone(projection_val);
      TRACE_DB("Query option: projection is present");
    }

    /* Parse actual query */
    TRACE_DB("Proceeding to parse inner query object");
    query_json = query;
  } else {
    TRACE_DB("Processing simple query format without options");
  }
  
  /* Parse query expression */
  if (query_json->type == JSON_OBJECT) {
    TRACE_DB("Parsing object query expression");

    /* Check for logical operators at root level */
    int has_logical_op = 0;
    int field_count = 0;
    const char* qkey;
    json_value_t* qvalue;

    TRACE_DB("Checking for logical operators at root level.");
    json_object_foreach(query_json, qkey, qvalue) {
      field_count++;
      TRACE_DB("Examining query field: %s", qkey);

      if (qkey[0] == '$') {
        query_operator_t op = query_parse_operator(qkey);
        if (op == OP_AND || op == OP_OR || op == OP_NOT || op == OP_NOR) {
          TRACE_DB("Found logical operator %s at root level", qkey);
          has_logical_op = 1;
          /* Create logical expression */
          result.expr = parse_logical_expr(qkey, qvalue);
          break;
        }
      }
    }
    
    if (!has_logical_op) {
      TRACE_DB("No logical operators found, creating implicit AND expression with %d fields", field_count);

      /* Create a default AND expression at root with all field expressions as children */
      query_expr_t** children = (query_expr_t**)BUFFER_ALLOC(field_count * sizeof(query_expr_t*));
      if (!children) {
        LOG_ERROR("Memory allocation failed for query expression children.");
        result.error = BUFFER_STRDUP("Out of memory");
        return result;
      }

      int index = 0;
      const char* fkey;
      json_value_t* fvalue;
      json_object_foreach(query_json, fkey, fvalue) {
        TRACE_DB("Creating field expression for field: %s", fkey);
        children[index++] = parse_field_expr(fkey, fvalue);
      }

      result.expr = query_create_logical_expr(OP_AND, children, field_count);
      BUFFER_FREE(children); /* Note: only free the array, not the child expressions */
      TRACE_DB("Created implicit AND expression with %d child expressions", field_count);
    }
  } else {
    /* Invalid query format */
    LOG_ERROR("Invalid query format: query must be a JSON object, but got type %d", query_json->type);
    result.error = BUFFER_STRDUP("Query must be a JSON object");
  }

  if (result.error) {
    LOG_ERROR("Query parsing failed: %s", result.error);
  } else {
    TRACE_DB("Query parsed successfully");
  }

  return result;
}

/* Parse a field expression (field comparisons or nested operators) */
static query_expr_t* parse_field_expr(const char* field_path, json_value_t* value) {
  if (!field_path || !value) {
    return NULL;
  }
  
  /* Check if field path has a query operator name (starts with $) */
  if (field_path[0] == '$') {
    query_operator_t op = query_parse_operator(field_path);
    if (op == OP_AND || op == OP_OR || op == OP_NOT || op == OP_NOR) {
      return parse_logical_expr(field_path, value);
    }
  }
  
  /* Handle direct value comparison */
  if (value->type != JSON_OBJECT || 
    (value->type == JSON_OBJECT && json_object_size(value) == 0)) {
    /* Direct equality comparison */
    return query_create_field_expr(field_path, OP_EQ, value);
  }
  
  /* Check for operator object */
  int has_operator = 0;
  
  /* Manual iteration instead of the macro to avoid unused variable warning */
  for (size_t i = 0; i < value->value.object.size; i++) {
    const char* key = value->value.object.entries[i].key;
    if (key[0] == '$') {
      has_operator = 1;
      break;
    }
  }
  
  if (has_operator) {
    /* This is a comparison expression with operators */
    return parse_comparison_expr(field_path, value);
  } else {
    /* This is a nested document match */
    query_expr_t** children = (query_expr_t**)BUFFER_ALLOC(json_object_size(value) * sizeof(query_expr_t*));
    if (!children) {
      return NULL;
    }
    
    int i = 0;
    const char* nest_key;
    json_value_t* nest_val;
    json_object_foreach(value, nest_key, nest_val) {
      char* nested_path = (char*)BUFFER_ALLOC(strlen(field_path) + strlen(nest_key) + 2);
      sprintf(nested_path, "%s.%s", field_path, nest_key);
      children[i++] = parse_field_expr(nested_path, nest_val);
      BUFFER_FREE(nested_path);
    }
    
    query_expr_t* expr = query_create_logical_expr(OP_AND, children, i);
    BUFFER_FREE(children);
    return expr;
  }
}

/* Parse a comparison expression (with operators like $eq, $gt, etc.) */
static query_expr_t* parse_comparison_expr(const char* field_path, json_value_t* value) {
  if (value->type != JSON_OBJECT) {
    return NULL;
  }
  
  /* Count operators to decide whether to combine with AND */
  int op_count = json_object_size(value);
  if (op_count == 0) {
    return NULL;
  }
  
  if (op_count == 1) {
    /* Single operator */
    const char* op_name = NULL;
    json_value_t* op_value = NULL;
    const char* comp_key;
    json_value_t* comp_val;
    json_object_foreach(value, comp_key, comp_val) {
      op_name = comp_key;
      op_value = comp_val;
      break;
    }
    
    query_operator_t op = query_parse_operator(op_name);
    return query_create_field_expr(field_path, op, op_value);
  } else {
    /* Multiple operators - combine with AND */
    query_expr_t** children = (query_expr_t**)BUFFER_ALLOC(op_count * sizeof(query_expr_t*));
    if (!children) {
      return NULL;
    }
    
    int i = 0;
    const char* multi_key;
    json_value_t* multi_val;
    json_object_foreach(value, multi_key, multi_val) {
      query_operator_t op = query_parse_operator(multi_key);
      children[i++] = query_create_field_expr(field_path, op, multi_val);
    }
    
    query_expr_t* expr = query_create_logical_expr(OP_AND, children, i);
    BUFFER_FREE(children);
    return expr;
  }
}

/* Parse a logical expression (with $and, $or, $not, $nor) */
static query_expr_t* parse_logical_expr(const char* op_name, json_value_t* array_value) {
  query_operator_t op = query_parse_operator(op_name);
  
  /* Handle $not (which takes a filter, not an array) */
  if (op == OP_NOT) {
    if (array_value->type != JSON_OBJECT) {
      return NULL;
    }
    
    /* Create a child expression for the filter */
    query_expr_t* child = parse_comparison_expr(NULL, array_value);
    if (!child) {
      return NULL;
    }
    
    return query_create_logical_expr(OP_NOT, &child, 1);
  }
  
  /* Handle $and, $or, $nor (which take arrays of filters) */
  if (array_value->type != JSON_ARRAY) {
    return NULL;
  }
  
  int count = json_array_size(array_value);
  if (count == 0) {
    return NULL;
  }
  
  query_expr_t** children = (query_expr_t**)BUFFER_ALLOC(count * sizeof(query_expr_t*));
  if (!children) {
    return NULL;
  }
  
  for (int i = 0; i < count; i++) {
    json_value_t* filter = json_array_get(array_value, i);
    if (filter->type != JSON_OBJECT) {
      for (int j = 0; j < i; j++) {
        query_free_expr(children[j]);
      }
      BUFFER_FREE(children);
      return NULL;
    }
    
    /* Parse each filter */
    if (json_object_size(filter) == 1) {
      /* Check if it's a field expression or a logical expression */
      const char* key = NULL;
      json_value_t* val = NULL;
      
      const char* filter_key;
      json_value_t* filter_val;
      json_object_foreach(filter, filter_key, filter_val) {
        key = filter_key;
        val = filter_val;
        break;
      }
      
      if (key[0] == '$' && (
        strcmp(key, "$and") == 0 || 
        strcmp(key, "$or") == 0 || 
        strcmp(key, "$not") == 0 || 
        strcmp(key, "$nor") == 0)) {
        children[i] = parse_logical_expr(key, val);
      } else {
        children[i] = parse_field_expr(key, val);
      }
    } else {
      /* Multiple conditions - create an AND expression */
      query_expr_t** sub_children = (query_expr_t**)BUFFER_ALLOC(json_object_size(filter) * sizeof(query_expr_t*));
      int sub_index = 0;
      
      const char* sub_key;
      json_value_t* sub_val;
      json_object_foreach(filter, sub_key, sub_val) {
        sub_children[sub_index++] = parse_field_expr(sub_key, sub_val);
      }
      
      children[i] = query_create_logical_expr(OP_AND, sub_children, sub_index);
      BUFFER_FREE(sub_children);
    }
    
    if (!children[i]) {
      for (int j = 0; j < i; j++) {
        query_free_expr(children[j]);
      }
      BUFFER_FREE(children);
      return NULL;
    }
  }
  
  query_expr_t* expr = query_create_logical_expr(op, children, count);
  BUFFER_FREE(children);
  return expr;
}

/* Free a query parse result */
void query_free_parse_result(query_parse_result_t* result) {
  if (!result) {
    return;
  }
  
  if (result->expr) {
    query_free_expr(result->expr);
  }
  
  if (result->options.sort) {
    json_free(result->options.sort);
  }
  
  if (result->options.projection) {
    json_free(result->options.projection);
  }
  
  if (result->error) {
    BUFFER_FREE(result->error);
  }
}

/* Extract a value from a document by field path */
json_value_t* query_extract_field(json_value_t* document, const char* field_path) {
  if (!document || !field_path) {
    return NULL;
  }
  
  /* Handle direct field access */
  if (strchr(field_path, '.') == NULL) {
    if (document->type == JSON_OBJECT) {
      return json_object_get(document, field_path);
    } else {
      return NULL;
    }
  }
  
  /* Handle nested field access */
  char* path_copy = BUFFER_STRDUP(field_path);
  char* token = strtok(path_copy, ".");
  json_value_t* current = document;
  
  while (token) {
    /* Check if we're accessing an array element by index */
    char* bracket = strchr(token, '[');
    if (bracket) {
      *bracket = '\0';
      char* index_str = bracket + 1;
      char* end_bracket = strchr(index_str, ']');
      if (end_bracket) {
        *end_bracket = '\0';
        int index = atoi(index_str);
        
        /* Get the array first */
        if (token[0] != '\0') {
          if (current->type != JSON_OBJECT) {
            BUFFER_FREE(path_copy);
            return NULL;
          }
          current = json_object_get(current, token);
        }
        
        /* Then access the array element */
        if (current && current->type == JSON_ARRAY && index >= 0 && (size_t)index < json_array_size(current)) {
          current = json_array_get(current, index);
        } else {
          BUFFER_FREE(path_copy);
          return NULL;
        }
      } else {
        BUFFER_FREE(path_copy);
        return NULL;
      }
    } else {
      /* Regular object field access */
      if (current->type != JSON_OBJECT) {
        BUFFER_FREE(path_copy);
        return NULL;
      }
      current = json_object_get(current, token);
    }
    
    if (!current) {
      BUFFER_FREE(path_copy);
      return NULL;
    }
    
    token = strtok(NULL, ".");
  }
  
  BUFFER_FREE(path_copy);
  return current;
}

/* Check if a document matches a comparison operator */
static int match_comparison_operator(query_operator_t op, json_value_t* field_value, json_value_t* query_value) {
  if (!field_value) {
    /* Field does not exist */
    switch (op) {
      case OP_EQ:
        return query_value->type == JSON_NULL;
      case OP_NE:
        return query_value->type != JSON_NULL;
      case OP_EXISTS:
        return query_value->type == JSON_BOOLEAN && !query_value->value.boolean;
      default:
        return 0;
    }
  }
  
  switch (op) {
    case OP_EQ:
      return json_equals(field_value, query_value);
      
    case OP_NE:
      return !json_equals(field_value, query_value);
      
    case OP_GT: {
      if (field_value->type == JSON_INTEGER && query_value->type == JSON_INTEGER) {
        return field_value->value.integer > query_value->value.integer;
      } else if (field_value->type == JSON_NUMBER && query_value->type == JSON_NUMBER) {
        return field_value->value.number > query_value->value.number;
      } else if (field_value->type == JSON_INTEGER && query_value->type == JSON_NUMBER) {
        return (double)field_value->value.integer > query_value->value.number;
      } else if (field_value->type == JSON_NUMBER && query_value->type == JSON_INTEGER) {
        return field_value->value.number > (double)query_value->value.integer;
      } else if (field_value->type == JSON_STRING && query_value->type == JSON_STRING) {
        return strcmp(field_value->value.string, query_value->value.string) > 0;
      }
      return 0;
    }
      
    case OP_GTE: {
      if (field_value->type == JSON_INTEGER && query_value->type == JSON_INTEGER) {
        return field_value->value.integer >= query_value->value.integer;
      } else if (field_value->type == JSON_NUMBER && query_value->type == JSON_NUMBER) {
        return field_value->value.number >= query_value->value.number;
      } else if (field_value->type == JSON_INTEGER && query_value->type == JSON_NUMBER) {
        return (double)field_value->value.integer >= query_value->value.number;
      } else if (field_value->type == JSON_NUMBER && query_value->type == JSON_INTEGER) {
        return field_value->value.number >= (double)query_value->value.integer;
      } else if (field_value->type == JSON_STRING && query_value->type == JSON_STRING) {
        return strcmp(field_value->value.string, query_value->value.string) >= 0;
      }
      return 0;
    }
      
    case OP_LT: {
      if (field_value->type == JSON_INTEGER && query_value->type == JSON_INTEGER) {
        return field_value->value.integer < query_value->value.integer;
      } else if (field_value->type == JSON_NUMBER && query_value->type == JSON_NUMBER) {
        return field_value->value.number < query_value->value.number;
      } else if (field_value->type == JSON_INTEGER && query_value->type == JSON_NUMBER) {
        return (double)field_value->value.integer < query_value->value.number;
      } else if (field_value->type == JSON_NUMBER && query_value->type == JSON_INTEGER) {
        return field_value->value.number < (double)query_value->value.integer;
      } else if (field_value->type == JSON_STRING && query_value->type == JSON_STRING) {
        return strcmp(field_value->value.string, query_value->value.string) < 0;
      }
      return 0;
    }
      
    case OP_LTE: {
      if (field_value->type == JSON_INTEGER && query_value->type == JSON_INTEGER) {
        return field_value->value.integer <= query_value->value.integer;
      } else if (field_value->type == JSON_NUMBER && query_value->type == JSON_NUMBER) {
        return field_value->value.number <= query_value->value.number;
      } else if (field_value->type == JSON_INTEGER && query_value->type == JSON_NUMBER) {
        return (double)field_value->value.integer <= query_value->value.number;
      } else if (field_value->type == JSON_NUMBER && query_value->type == JSON_INTEGER) {
        return field_value->value.number <= (double)query_value->value.integer;
      } else if (field_value->type == JSON_STRING && query_value->type == JSON_STRING) {
        return strcmp(field_value->value.string, query_value->value.string) <= 0;
      }
      return 0;
    }
      
    case OP_IN: {
      if (query_value->type != JSON_ARRAY) {
        return 0;
      }
      
      for (size_t i = 0; i < json_array_size(query_value); i++) {
        json_value_t* array_value = json_array_get(query_value, i);
        if (json_equals(field_value, array_value)) {
          return 1;
        }
      }
      return 0;
    }
      
    case OP_NIN: {
      if (query_value->type != JSON_ARRAY) {
        return 1;
      }
      
      for (size_t i = 0; i < json_array_size(query_value); i++) {
        json_value_t* array_value = json_array_get(query_value, i);
        if (json_equals(field_value, array_value)) {
          return 0;
        }
      }
      return 1;
    }
      
    case OP_EXISTS: {
      if (query_value->type != JSON_BOOLEAN) {
        return 0;
      }
      return query_value->value.boolean ? 1 : 0;
    }
      
    case OP_TYPE: {
      json_type_t type_val = (json_type_t)-1; /* Invalid type */
      if (query_value->type == JSON_INTEGER) {
        type_val = (json_type_t)query_value->value.integer;
      } else if (query_value->type == JSON_STRING) {
        if (strcmp(query_value->value.string, "null") == 0) {
          type_val = JSON_NULL;
        } else if (strcmp(query_value->value.string, "boolean") == 0) {
          type_val = JSON_BOOLEAN;
        } else if (strcmp(query_value->value.string, "integer") == 0) {
          type_val = JSON_INTEGER;
        } else if (strcmp(query_value->value.string, "number") == 0) {
          type_val = JSON_NUMBER;
        } else if (strcmp(query_value->value.string, "string") == 0) {
          type_val = JSON_STRING;
        } else if (strcmp(query_value->value.string, "array") == 0) {
          type_val = JSON_ARRAY;
        } else if (strcmp(query_value->value.string, "object") == 0) {
          type_val = JSON_OBJECT;
        }
      }
      
      return field_value->type == type_val;
    }
      
    case OP_ALL: {
      if (field_value->type != JSON_ARRAY || query_value->type != JSON_ARRAY) {
        return 0;
      }
      
      for (size_t i = 0; i < json_array_size(query_value); i++) {
        json_value_t* search_value = json_array_get(query_value, i);
        int found = 0;
        
        for (size_t j = 0; j < json_array_size(field_value); j++) {
          json_value_t* array_value = json_array_get(field_value, j);
          if (json_equals(array_value, search_value)) {
            found = 1;
            break;
          }
        }
        
        if (!found) {
          return 0;
        }
      }
      return 1;
    }
      
    case OP_SIZE: {
      if (field_value->type != JSON_ARRAY || 
        (query_value->type != JSON_INTEGER && query_value->type != JSON_NUMBER)) {
        return 0;
      }
      
      int size = json_array_size(field_value);
      return size == (query_value->type == JSON_INTEGER ? 
              (int)query_value->value.integer : (int)query_value->value.number);
    }
      
    default:
      return 0;
  }
}

/* Check if document matches expression */
int query_match_document(query_expr_t* expr, json_value_t* document) {
  if (!expr || !document) {
    return 0;
  }
  
  switch (expr->op) {
    case OP_ROOT:
    case OP_AND: {
      for (int i = 0; i < expr->num_children; i++) {
        if (!query_match_document(expr->children[i], document)) {
          return 0;
        }
      }
      return 1;
    }
      
    case OP_OR: {
      for (int i = 0; i < expr->num_children; i++) {
        if (query_match_document(expr->children[i], document)) {
          return 1;
        }
      }
      return 0;
    }
      
    case OP_NOT: {
      if (expr->num_children != 1 || !expr->children[0]) {
        return 0;
      }
      return !query_match_document(expr->children[0], document);
    }
      
    case OP_NOR: {
      for (int i = 0; i < expr->num_children; i++) {
        if (query_match_document(expr->children[i], document)) {
          return 0;
        }
      }
      return 1;
    }
      
    case OP_EQ:
    case OP_NE:
    case OP_GT:
    case OP_GTE:
    case OP_LT:
    case OP_LTE:
    case OP_IN:
    case OP_NIN:
    case OP_EXISTS:
    case OP_TYPE:
    case OP_ALL:
    case OP_SIZE: {
      json_value_t* field_value = NULL;
      if (expr->field_path) {
        field_value = query_extract_field(document, expr->field_path);
      }
      return match_comparison_operator(expr->op, field_value, expr->value);
    }
      
    case OP_ELEM_MATCH: {
      json_value_t* field_value = NULL;
      if (expr->field_path) {
        field_value = query_extract_field(document, expr->field_path);
      }
      
      if (!field_value || field_value->type != JSON_ARRAY) {
        return 0;
      }
      
      for (size_t i = 0; i < json_array_size(field_value); i++) {
        json_value_t* element = json_array_get(field_value, i);
        if (element->type == JSON_OBJECT) {
          int matched = 1;
          for (int j = 0; j < expr->num_children; j++) {
            if (!query_match_document(expr->children[j], element)) {
              matched = 0;
              break;
            }
          }
          if (matched) {
            return 1;
          }
        }
      }
      return 0;
    }
      
    default:
      return 0;
  }
}

/* Apply projection to a document */
json_value_t* query_apply_projection(json_value_t* document, json_value_t* projection) {
  if (!document || document->type != JSON_OBJECT || !projection || projection->type != JSON_OBJECT) {
    return document ? json_clone(document) : NULL;
  }
  
  /* Determine if we're including fields (1) or excluding fields (0) */
  int include_mode = 1;
  int projection_specified = 0;
  
  const char* proj_key;
  json_value_t* proj_value;
  json_object_foreach(projection, proj_key, proj_value) {
    if (strcmp(proj_key, "uuid") == 0) {
      continue; /* uuid is special, skip it for mode determination */
    }
    
    if (proj_value->type == JSON_INTEGER || proj_value->type == JSON_BOOLEAN) {
      int field_val = proj_value->type == JSON_INTEGER ? 
              (int)proj_value->value.integer : (proj_value->value.boolean ? 1 : 0);
      
      if (!projection_specified) {
        include_mode = field_val ? 1 : 0;
        projection_specified = 1;
      } else {
        if ((include_mode && !field_val) || (!include_mode && field_val)) {
          /* Can't mix include and exclude */
          return json_clone(document);
        }
      }
    }
  }
  
  if (!projection_specified) {
    return json_clone(document);
  }
  
  /* Create a new document with projected fields */
  json_value_t* result = json_create_object();
  
  /* Handle uuid field specially */
  json_value_t* id_proj = json_object_get(projection, "uuid");
  int include_id = 1; /* Include by default */
  
  if (id_proj) {
    if (id_proj->type == JSON_INTEGER) {
      include_id = (int)id_proj->value.integer ? 1 : 0;
    } else if (id_proj->type == JSON_BOOLEAN) {
      include_id = id_proj->value.boolean ? 1 : 0;
    }
  }
  
  if (include_id) {
    json_value_t* id_value = json_object_get(document, "uuid");
    if (id_value) {
      json_object_set(result, "uuid", json_clone(id_value));
    }
  }
  
  /* Handle all other fields */
  const char* doc_key;
  json_value_t* doc_value;
  json_object_foreach(document, doc_key, doc_value) {
    if (strcmp(doc_key, "uuid") == 0) {
      continue; /* Already handled */
    }
    
    json_value_t* field_proj = json_object_get(projection, doc_key);
    int include_field = include_mode ? 0 : 1; /* Default based on mode */
    
    if (field_proj) {
      if (field_proj->type == JSON_INTEGER) {
        include_field = (int)field_proj->value.integer ? 1 : 0;
      } else if (field_proj->type == JSON_BOOLEAN) {
        include_field = field_proj->value.boolean ? 1 : 0;
      }
    }
    
    if ((include_mode && include_field) || (!include_mode && !include_field)) {
      json_object_set(result, doc_key, json_clone(doc_value));
    }
  }
  
  return result;
}

/* Compare two documents for sorting */
static int compare_documents(json_value_t* a, json_value_t* b, const char* field, int ascending) {
  json_value_t* a_value = query_extract_field(a, field);
  json_value_t* b_value = query_extract_field(b, field);
  
  if (!a_value && !b_value) {
    return 0;
  }
  
  if (!a_value) {
    return ascending ? -1 : 1;
  }
  
  if (!b_value) {
    return ascending ? 1 : -1;
  }
  
  int result = 0;
  
  /* Compare based on type */
  if (a_value->type != b_value->type) {
    result = a_value->type - b_value->type;
  } else {
    switch (a_value->type) {
      case JSON_NULL:
        result = 0;
        break;
        
      case JSON_BOOLEAN:
        result = a_value->value.boolean - b_value->value.boolean;
        break;
        
      case JSON_INTEGER:
        if (a_value->value.integer < b_value->value.integer) {
          result = -1;
        } else if (a_value->value.integer > b_value->value.integer) {
          result = 1;
        } else {
          result = 0;
        }
        break;
        
      case JSON_NUMBER:
        if (a_value->value.number < b_value->value.number) {
          result = -1;
        } else if (a_value->value.number > b_value->value.number) {
          result = 1;
        } else {
          result = 0;
        }
        break;
        
      case JSON_STRING:
        result = strcmp(a_value->value.string, b_value->value.string);
        break;
        
      default:
        result = 0;
        break;
    }
  }
  
  return ascending ? result : -result;
}

/* Apply sort to document array */
void query_apply_sort(json_value_t* documents, json_value_t* sort) {
  if (!documents || documents->type != JSON_ARRAY || !sort || sort->type != JSON_OBJECT) {
    return;
  }
  
  /* Convert sort definition to a simple list for comparison */
  typedef struct {
    char* field;
    int ascending;
  } sort_field_t;
  
  int sort_count = json_object_size(sort);
  if (sort_count == 0) {
    return;
  }
  
  sort_field_t* sort_fields = (sort_field_t*)BUFFER_ALLOC(sort_count * sizeof(sort_field_t));
  if (!sort_fields) {
    return;
  }
  
  int index = 0;
  const char* sort_key;
  json_value_t* sort_value;
  json_object_foreach(sort, sort_key, sort_value) {
    sort_fields[index].field = BUFFER_STRDUP(sort_key);
    
    if (sort_value->type == JSON_INTEGER) {
      sort_fields[index].ascending = sort_value->value.integer >= 0 ? 1 : 0;
    } else if (sort_value->type == JSON_STRING) {
      sort_fields[index].ascending = strcmp(sort_value->value.string, "asc") == 0 ? 1 : 0;
    } else {
      sort_fields[index].ascending = 1; /* Default to ascending */
    }
    
    index++;
  }
  
  /* Bubble sort the documents (simple but not efficient for large result sets) */
  size_t size = json_array_size(documents);
  size_t result_size = json_array_size(documents);
  for (size_t i = 0; i < result_size - 1; i++) {
    for (size_t j = 0; j < (size_t)(size - i - 1); j++) {
      json_value_t* a = json_array_get(documents, j);
      json_value_t* b = json_array_get(documents, j + 1);
      
      int swap = 0;
      for (int k = 0; k < sort_count; k++) {
        int cmp = compare_documents(a, b, sort_fields[k].field, sort_fields[k].ascending);
        if (cmp < 0) {
          swap = 0;
          break;
        } else if (cmp > 0) {
          swap = 1;
          break;
        }
      }
      
      if (swap) {
        /* Swap the documents */
        json_array_set(documents, j, b);
        json_array_set(documents, j + 1, a);
      }
    }
  }
  
  /* Free sort fields */
  for (int i = 0; i < sort_count; i++) {
    BUFFER_FREE(sort_fields[i].field);
  }
  BUFFER_FREE(sort_fields);
}

/* Create pagination info from query options and results */
pagination_info_t* query_create_pagination_info(query_options_t* options, int total_count, json_value_t* documents) {
  if (!options || total_count <= 0) {
    return NULL;
  }
  
  pagination_info_t* pagination = (pagination_info_t*)BUFFER_ALLOC(sizeof(pagination_info_t));
  if (!pagination) {
    return NULL;
  }
  
  /* Initialize pagination info */
  pagination->total_count = total_count;
  pagination->page_count = documents ? json_array_size(documents) : 0;
  
  if (options->pagination_type == PAGINATION_OFFSET) {
    /* Page-based pagination */
    pagination->page = options->page;
    pagination->page_size = options->page_size;
    
    /* Calculate total pages */
    pagination->total_pages = (total_count + pagination->page_size - 1) / pagination->page_size;
    
    /* Determine if there are previous or next pages */
    pagination->has_prev_page = pagination->page > 1;
    pagination->has_next_page = pagination->page < pagination->total_pages;
    
    /* No cursors for offset-based pagination */
    pagination->next_cursor = NULL;
    pagination->prev_cursor = NULL;
  } else if (options->pagination_type == PAGINATION_CURSOR) {
    /* Cursor-based pagination */
    pagination->page = 0; /* Not applicable for cursor pagination */
    pagination->page_size = options->limit;
    pagination->total_pages = 0; /* Not applicable for cursor pagination */
    
    /* For cursor pagination, we always have a prev page if cursor is provided */
    pagination->has_prev_page = options->cursor ? 1 : 0;
    
    /* Generate next cursor if there are more results */
    pagination->has_next_page = pagination->page_count == options->limit;
    
    /* Generate cursors if needed */
    pagination->next_cursor = NULL;
    pagination->prev_cursor = options->cursor ? BUFFER_STRDUP(options->cursor) : NULL;
    
    if (pagination->has_next_page && documents && json_array_size(documents) > 0) {
      /* Generate cursor for the last document in current page */
      json_value_t* last_doc = json_array_get(documents, json_array_size(documents) - 1);
      pagination->next_cursor = query_generate_cursor(last_doc, options);
    }
  } else {
    /* No pagination */
    pagination->page = 1;
    pagination->page_size = total_count;
    pagination->total_pages = 1;
    pagination->has_prev_page = 0;
    pagination->has_next_page = 0;
    pagination->next_cursor = NULL;
    pagination->prev_cursor = NULL;
  }
  
  return pagination;
}

/* Free pagination info */
void query_free_pagination_info(pagination_info_t* pagination) {
  if (!pagination) {
    return;
  }
  
  if (pagination->next_cursor) {
    BUFFER_FREE(pagination->next_cursor);
  }
  
  if (pagination->prev_cursor) {
    BUFFER_FREE(pagination->prev_cursor);
  }
  
  BUFFER_FREE(pagination);
}

/* Convert pagination info to JSON */
json_value_t* query_pagination_to_json(pagination_info_t* pagination) {
  if (!pagination) {
    return NULL;
  }
  
  json_value_t* json = json_create_object();
  
  json_object_set(json, "total_count", json_create_integer(pagination->total_count));
  json_object_set(json, "page_count", json_create_integer(pagination->page_count));
  json_object_set(json, "has_next_page", json_create_boolean(pagination->has_next_page));
  json_object_set(json, "has_prev_page", json_create_boolean(pagination->has_prev_page));
  
  if (pagination->page > 0) {
    json_object_set(json, "page", json_create_integer(pagination->page));
  }
  
  if (pagination->page_size > 0) {
    json_object_set(json, "page_size", json_create_integer(pagination->page_size));
  }
  
  if (pagination->total_pages > 0) {
    json_object_set(json, "total_pages", json_create_integer(pagination->total_pages));
  }
  
  if (pagination->next_cursor) {
    json_object_set(json, "next_cursor", json_create_string(pagination->next_cursor));
  }
  
  if (pagination->prev_cursor) {
    json_object_set(json, "prev_cursor", json_create_string(pagination->prev_cursor));
  }
  
  return json;
}

/* Generate cursor for cursor-based pagination 
 * The cursor is a base64-encoded JSON object containing sort keys and their values from the document */
char* query_generate_cursor(json_value_t* document, query_options_t* options) {
  if (!document || !options || !options->sort || options->sort->type != JSON_OBJECT) {
    return NULL;
  }
  
  /* Create a cursor JSON object containing sort field values */
  json_value_t* cursor_data = json_create_object();
  
  /* Extract sort field values from document */
  /* Manual iteration instead of the macro to avoid unused variable warning */
  for (size_t i = 0; i < options->sort->value.object.size; i++) {
    const char* key = options->sort->value.object.entries[i].key;
    json_value_t* field_value = query_extract_field(document, key);
    if (field_value) {
      json_object_set(cursor_data, key, json_clone(field_value));
    }
  }
  
  /* Serialize to JSON string */
  char* json_str = json_stringify(cursor_data);
  json_free(cursor_data);
  
  if (!json_str) {
    return NULL;
  }
  
  /* Encode as base64 (simplified version, in production you'd use a proper base64 library) */
  /* For this example, we'll just use the JSON string directly */
  char* cursor = BUFFER_STRDUP(json_str);
  BUFFER_FREE(json_str);
  
  return cursor;
}

/* Parse cursor for cursor-based pagination */
int query_parse_cursor(const char* cursor, query_options_t* options) {
  if (!cursor || !options) {
    return 0;
  }
  
  /* Decode cursor (in production, you'd decode base64 first) */
  json_value_t* cursor_data = json_parse(cursor);
  if (!cursor_data || cursor_data->type != JSON_OBJECT) {
    if (cursor_data) json_free(cursor_data);
    return 0;
  }
  
  /* TODO: Use cursor data to set up appropriate skip/sort options for continuation */
  /* This is a complex topic and would require more extensive implementation */
  
  json_free(cursor_data);
  return 1;
}

/* Execute a query against a collection of documents */
query_result_t query_execute(query_expr_t* expr, json_value_t* documents, query_options_t* options) {
  TRACE_DB("Executing query against document collection");

  query_result_t result;
  result.documents = json_create_array();
  result.count = 0;
  result.total_count = 0;
  result.pagination = NULL;

  if (!expr || !documents || documents->type != JSON_ARRAY) {
    LOG_ERROR("Invalid query execution parameters (expr: %p, documents: %p, document type: %d)",
         expr, documents, documents ? (int)documents->type : -1);
    return result;
  }

  TRACE_DB("Query execution with collection of %d documents", json_array_size(documents));

  /* Create a temporary array with matching documents */
  json_value_t* matches = json_create_array();
  TRACE_DB("Filtering documents using query expression");

  /* Filter documents */
  int evaluated = 0;
  int matched = 0;

  for (size_t i = 0; i < json_array_size(documents); i++) {
    json_value_t* doc = json_array_get(documents, i);
    evaluated++;

    if (query_match_document(expr, doc)) {
      json_array_append(matches, json_clone(doc));
      matched++;

      /* Log every 100th match for performance monitoring */
      if (matched % 100 == 0) {
        TRACE_DB("Query progress: %d evaluated, %d matched", evaluated, matched);
      }
    }
  }

  /* Store total count of matching documents before pagination */
  result.total_count = json_array_size(matches);
  LOG_INFO("Query matched %d/%d documents (%0.2f%%)",
      result.total_count, json_array_size(documents),
      json_array_size(documents) > 0 ?
        (100.0f * result.total_count / json_array_size(documents)) : 0.0f);
  
  /* Apply sorting if specified */
  if (options && options->sort) {
    TRACE_DB("Applying sort to query results");
    query_apply_sort(matches, options->sort);
  }

  /* Apply pagination */
  int skip = options ? options->skip : 0;
  int limit = options ? options->limit : 0;

  if (skip < 0) skip = 0;
  if (limit < 0) limit = 0;

  TRACE_DB("Applying pagination: skip=%d, limit=%d", skip, limit);

  int max_index = json_array_size(matches);
  if (limit > 0) {
    max_index = skip + limit < max_index ? skip + limit : max_index;
  }
  
  /* Apply projection and copy final results */
  TRACE_DB("Processing final result set from index %d to %d", skip, max_index);

  for (int i = skip; i < max_index; i++) {
    json_value_t* doc = json_array_get(matches, i);

    if (options && options->projection) {
      TRACE_DB("Applying projection to document at index %d", i);
      json_value_t* projected_doc = query_apply_projection(doc, options->projection);
      json_array_append(result.documents, projected_doc);
    } else {
      json_array_append(result.documents, json_clone(doc));
    }
  }

  /* Set count of documents in this result set */
  result.count = json_array_size(result.documents);
  TRACE_DB("Final result set contains %d documents", result.count);

  /* Create pagination info if appropriate */
  if (options && (options->pagination_type != PAGINATION_NONE)) {
    TRACE_DB("Creating pagination info (type: %d)", options->pagination_type);
    result.pagination = query_create_pagination_info(options, result.total_count, result.documents);
  }

  /* Free temporary matches array */
  TRACE_DB("Freeing temporary result set");
  json_free(matches);

  LOG_INFO("Query execution completed: returned %d/%d matching documents",
      result.count, result.total_count);
  return result;
}

/* Parse query options (limit, skip, sort, projection, pagination) */
query_options_t query_parse_options(json_value_t* options_json) {
  query_options_t options;
  options.limit = 0;
  options.skip = 0;
  options.page = 1;
  options.page_size = 0;
  options.pagination_type = PAGINATION_NONE;
  options.cursor = NULL;
  options.sort = NULL;
  options.projection = NULL;
  
  if (!options_json || options_json->type != JSON_OBJECT) {
    return options;
  }
  
  /* Parse limit */
  json_value_t* limit_val = json_object_get(options_json, "limit");
  if (limit_val && (limit_val->type == JSON_INTEGER || limit_val->type == JSON_NUMBER)) {
    options.limit = limit_val->type == JSON_INTEGER ? 
            (int)limit_val->value.integer : (int)limit_val->value.number;
    if (options.limit < 0) options.limit = 0;
  }
  
  /* Parse skip */
  json_value_t* skip_val = json_object_get(options_json, "skip");
  if (skip_val && (skip_val->type == JSON_INTEGER || skip_val->type == JSON_NUMBER)) {
    options.skip = skip_val->type == JSON_INTEGER ? 
           (int)skip_val->value.integer : (int)skip_val->value.number;
    if (options.skip < 0) options.skip = 0;
  }
  
  /* Parse page */
  json_value_t* page_val = json_object_get(options_json, "page");
  if (page_val && (page_val->type == JSON_INTEGER || page_val->type == JSON_NUMBER)) {
    options.page = page_val->type == JSON_INTEGER ? 
           (int)page_val->value.integer : (int)page_val->value.number;
    if (options.page < 1) options.page = 1;
    options.pagination_type = PAGINATION_OFFSET;
  }
  
  /* Parse page_size */
  json_value_t* page_size_val = json_object_get(options_json, "page_size");
  if (page_size_val && (page_size_val->type == JSON_INTEGER || page_size_val->type == JSON_NUMBER)) {
    options.page_size = page_size_val->type == JSON_INTEGER ? 
              (int)page_size_val->value.integer : (int)page_size_val->value.number;
    if (options.page_size < 0) options.page_size = 0;
    options.pagination_type = PAGINATION_OFFSET;
    
    /* Set limit and skip based on page and page_size */
    if (options.page_size > 0) {
      options.limit = options.page_size;
      options.skip = (options.page - 1) * options.page_size;
    }
  }
  
  /* Parse cursor */
  json_value_t* cursor_val = json_object_get(options_json, "cursor");
  if (cursor_val && cursor_val->type == JSON_STRING) {
    options.cursor = BUFFER_STRDUP(cursor_val->value.string);
    options.pagination_type = PAGINATION_CURSOR;
    
    /* Parse cursor to set appropriate options */
    if (options.cursor && strlen(options.cursor) > 0) {
      query_parse_cursor(options.cursor, &options);
    }
  }
  
  /* Parse sort */
  json_value_t* sort_val = json_object_get(options_json, "sort");
  if (sort_val && sort_val->type == JSON_OBJECT) {
    options.sort = json_clone(sort_val);
  }
  
  /* Parse projection */
  json_value_t* projection_val = json_object_get(options_json, "projection");
  if (projection_val && projection_val->type == JSON_OBJECT) {
    options.projection = json_clone(projection_val);
  }
  
  return options;
}