#ifndef JSON_H
#define JSON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* JSON value types */
typedef enum {
    JSON_NULL,
    JSON_BOOLEAN,
    JSON_NUMBER,
    JSON_INTEGER,  /* Added for compatibility */
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} json_type_t;

/* Forward declaration for json_value */
struct json_value;

/* JSON object key-value pair */
typedef struct {
    char* key;
    struct json_value* value;
} json_object_entry_t;

/* JSON array */
typedef struct {
    struct json_value** items;
    size_t size;
    size_t capacity;
} json_array_t;

/* JSON object */
typedef struct {
    json_object_entry_t* entries;
    size_t size;
    size_t capacity;
} json_object_t;

/* JSON value */
typedef struct json_value {
    json_type_t type;
    union {
        int boolean;
        double number;
        int64_t integer;  /* Added for compatibility */
        char* string;
        json_array_t array;
        json_object_t object;
    } value;
} json_value_t;

/* Function prototypes */
json_value_t* json_parse(const char* json_str);
char* json_stringify(json_value_t* value);
void json_free(json_value_t* value);

json_value_t* json_create_null();
json_value_t* json_create_boolean(int boolean);
json_value_t* json_create_number(double number);
json_value_t* json_create_integer(int64_t integer);
json_value_t* json_create_string(const char* string);
json_value_t* json_create_array();
json_value_t* json_create_object();

void json_array_append(json_value_t* array, json_value_t* value);
json_value_t* json_array_get(json_value_t* array, size_t index);
size_t json_array_size(json_value_t* array);
void json_array_set(json_value_t* array, size_t index, json_value_t* value);
void json_array_remove(json_value_t* array, size_t index);
void json_object_set(json_value_t* object, const char* key, json_value_t* value);
json_value_t* json_object_get(json_value_t* object, const char* key);
int json_object_has(json_value_t* object, const char* key);
void json_object_remove(json_value_t* object, const char* key);
size_t json_object_size(json_value_t* object);
json_value_t* json_clone(json_value_t* value);
json_value_t* json_deep_copy(json_value_t* value);
json_value_t* json_deep_copy_optimized(json_value_t* value);
void json_array_set(json_value_t* array, size_t index, json_value_t* value);
int json_equals(json_value_t* value1, json_value_t* value2);

/* Helper functions for working with JSON values */
json_type_t json_get_type(json_value_t* value);
const char* json_get_string(json_value_t* value);
int json_get_boolean(json_value_t* value);
double json_get_number(json_value_t* value);
int64_t json_get_integer(json_value_t* value);

/* Forward declaration for transaction log */
struct transaction_log;
typedef struct transaction_log transaction_log_t;

/* Function to get transaction log statistics */
json_value_t* transaction_log_get_stats(transaction_log_t* log);

/* Helper macro for iterating through object fields */
#define json_object_foreach(obj, key_var, val_var) \
    for(size_t __i = 0; __i < (obj)->value.object.size && \
        (((key_var) = (obj)->value.object.entries[__i].key), \
         ((val_var) = (obj)->value.object.entries[__i].value), 1); \
        ++__i)

#endif /* JSON_H */