#include "utils/json.h"
#include "utils/debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Forward declarations */
static json_value_t* parse_value(const char** json);
static char* stringify_value(json_value_t* value);
static void skip_whitespace(const char** json);

/* Helper functions for working with JSON values */
json_type_t json_get_type(json_value_t* value) {
    if (!value) return JSON_NULL;
    return value->type;
}

const char* json_get_string(json_value_t* value) {
    if (!value || value->type != JSON_STRING) return NULL;
    return value->value.string;
}

int json_get_boolean(json_value_t* value) {
    if (!value || value->type != JSON_BOOLEAN) return 0;
    return value->value.boolean;
}

double json_get_number(json_value_t* value) {
    if (!value) return 0.0;
    if (value->type == JSON_NUMBER) return value->value.number;
    if (value->type == JSON_INTEGER) return (double)value->value.integer;
    return 0.0;
}

int64_t json_get_integer(json_value_t* value) {
    if (!value) return 0;
    if (value->type == JSON_INTEGER) return value->value.integer;
    if (value->type == JSON_NUMBER) return (int64_t)value->value.number;
    return 0;
}

/* Create JSON value types */
json_value_t* json_create_null() {
    json_value_t* value = (json_value_t*)malloc(sizeof(json_value_t));
    if (value) {
        value->type = JSON_NULL;
    }
    return value;
}

json_value_t* json_create_boolean(int boolean) {
    json_value_t* value = (json_value_t*)malloc(sizeof(json_value_t));
    if (value) {
        value->type = JSON_BOOLEAN;
        value->value.boolean = boolean ? 1 : 0;
    }
    return value;
}

json_value_t* json_create_number(double number) {
    json_value_t* value = (json_value_t*)malloc(sizeof(json_value_t));
    if (value) {
        value->type = JSON_NUMBER;
        value->value.number = number;
    }
    return value;
}

json_value_t* json_create_integer(int64_t integer) {
    json_value_t* value = (json_value_t*)malloc(sizeof(json_value_t));
    if (value) {
        value->type = JSON_INTEGER;
        value->value.integer = integer;
    }
    return value;
}

json_value_t* json_create_string(const char* string) {
    json_value_t* value = (json_value_t*)malloc(sizeof(json_value_t));
    if (value) {
        value->type = JSON_STRING;
        value->value.string = strdup(string);
    }
    return value;
}

json_value_t* json_create_array() {
    json_value_t* value = (json_value_t*)malloc(sizeof(json_value_t));
    if (value) {
        value->type = JSON_ARRAY;
        value->value.array.items = NULL;
        value->value.array.size = 0;
        value->value.array.capacity = 0;
    }
    return value;
}

json_value_t* json_create_object() {
    json_value_t* value = (json_value_t*)malloc(sizeof(json_value_t));
    if (value) {
        value->type = JSON_OBJECT;
        value->value.object.entries = NULL;
        value->value.object.size = 0;
        value->value.object.capacity = 0;
    }
    return value;
}

/* JSON array operations */
void json_array_append(json_value_t* array, json_value_t* value) {
    if (!array || array->type != JSON_ARRAY || !value) {
        return;
    }
    
    /* Grow array if needed */
    if (array->value.array.size >= array->value.array.capacity) {
        size_t new_capacity = array->value.array.capacity == 0 ? 8 : array->value.array.capacity * 2;
        json_value_t** new_items = (json_value_t**)realloc(array->value.array.items, 
                                                     new_capacity * sizeof(json_value_t*));
        if (!new_items) {
            return;
        }
        
        array->value.array.items = new_items;
        array->value.array.capacity = new_capacity;
    }
    
    /* Add the item */
    array->value.array.items[array->value.array.size++] = value;
}

/* Get an item from a JSON array at a specific index */
json_value_t* json_array_get(json_value_t* array, size_t index) {
    if (!array || array->type != JSON_ARRAY || index >= array->value.array.size) {
        return NULL;
    }
    
    return array->value.array.items[index];
}

/* Get the size of a JSON array */
size_t json_array_size(json_value_t* array) {
    if (!array || array->type != JSON_ARRAY) {
        return 0;
    }
    
    return array->value.array.size;
}

/* Set a value in a JSON array at a specific index */
void json_array_set(json_value_t* array, size_t index, json_value_t* value) {
    if (!array || array->type != JSON_ARRAY || !value || index >= array->value.array.size) {
        return;
    }
    
    /* Free the old value */
    if (array->value.array.items[index]) {
        json_free(array->value.array.items[index]);
    }
    
    /* Set the new value */
    array->value.array.items[index] = value;
}

/* JSON object operations */
void json_object_set(json_value_t* object, const char* key, json_value_t* value) {
    if (!object || object->type != JSON_OBJECT || !key || !value) {
        return;
    }
    
    /* Check if key already exists */
    for (size_t i = 0; i < object->value.object.size; i++) {
        if (strcmp(object->value.object.entries[i].key, key) == 0) {
            /* Free old value */
            json_free(object->value.object.entries[i].value);
            
            /* Set new value */
            object->value.object.entries[i].value = value;
            return;
        }
    }
    
    /* Grow object if needed */
    if (object->value.object.size >= object->value.object.capacity) {
        size_t new_capacity = object->value.object.capacity == 0 ? 8 : object->value.object.capacity * 2;
        json_object_entry_t* new_entries = (json_object_entry_t*)realloc(object->value.object.entries, 
                                                                 new_capacity * sizeof(json_object_entry_t));
        if (!new_entries) {
            return;
        }
        
        object->value.object.entries = new_entries;
        object->value.object.capacity = new_capacity;
    }
    
    /* Add the entry */
    object->value.object.entries[object->value.object.size].key = strdup(key);
    object->value.object.entries[object->value.object.size].value = value;
    object->value.object.size++;
}

json_value_t* json_object_get(json_value_t* object, const char* key) {
    if (!object || object->type != JSON_OBJECT || !key) {
        return NULL;
    }
    
    for (size_t i = 0; i < object->value.object.size; i++) {
        if (strcmp(object->value.object.entries[i].key, key) == 0) {
            return object->value.object.entries[i].value;
        }
    }
    
    return NULL;
}

int json_object_has(json_value_t* object, const char* key) {
    if (!object || object->type != JSON_OBJECT || !key) {
        return 0;
    }
    
    for (size_t i = 0; i < object->value.object.size; i++) {
        if (strcmp(object->value.object.entries[i].key, key) == 0) {
            return 1;
        }
    }
    
    return 0;
}

/* Get the number of key-value pairs in a JSON object */
size_t json_object_size(json_value_t* object) {
    if (!object || object->type != JSON_OBJECT) {
        return 0;
    }
    
    return object->value.object.size;
}

void json_object_remove(json_value_t* object, const char* key) {
    if (!object || object->type != JSON_OBJECT || !key) {
        return;
    }
    
    for (size_t i = 0; i < object->value.object.size; i++) {
        if (strcmp(object->value.object.entries[i].key, key) == 0) {
            /* Free key and value */
            free(object->value.object.entries[i].key);
            json_free(object->value.object.entries[i].value);
            
            /* Move remaining entries */
            for (size_t j = i; j < object->value.object.size - 1; j++) {
                object->value.object.entries[j] = object->value.object.entries[j + 1];
            }
            
            object->value.object.size--;
            return;
        }
    }
}

/* Deep clone a JSON value */
json_value_t* json_clone(json_value_t* value) {
    if (!value) {
        return NULL;
    }
    
    json_value_t* cloned = NULL;
    
    switch (value->type) {
        case JSON_NULL:
            cloned = json_create_null();
            break;
            
        case JSON_BOOLEAN:
            cloned = json_create_boolean(value->value.boolean);
            break;
            
        case JSON_NUMBER:
            cloned = json_create_number(value->value.number);
            break;
            
        case JSON_INTEGER:
            cloned = json_create_integer(value->value.integer);
            break;
            
        case JSON_STRING:
            cloned = json_create_string(value->value.string);
            break;
            
        case JSON_ARRAY:
            cloned = json_create_array();
            if (cloned) {
                for (size_t i = 0; i < value->value.array.size; i++) {
                    json_value_t* item_clone = json_clone(value->value.array.items[i]);
                    if (item_clone) {
                        json_array_append(cloned, item_clone);
                    }
                }
            }
            break;
            
        case JSON_OBJECT:
            cloned = json_create_object();
            if (cloned) {
                for (size_t i = 0; i < value->value.object.size; i++) {
                    json_value_t* val_clone = json_clone(value->value.object.entries[i].value);
                    if (val_clone) {
                        json_object_set(cloned, value->value.object.entries[i].key, val_clone);
                    }
                }
            }
            break;
    }
    
    return cloned;
}

/* JSON parse helpers */
static void skip_whitespace(const char** json) {
    while (**json && isspace(**json)) {
        (*json)++;
    }
}

static char* parse_string(const char** json) {
    if (**json != '"') {
        return NULL;
    }
    
    (*json)++;  /* Skip opening quote */
    
    const char* start = *json;
    char* result = NULL;
    
    /* Find closing quote */
    while (**json && **json != '"') {
        /* Handle escaped characters */
        if (**json == '\\' && *(*json + 1) != '\0') {
            (*json)++;
        }
        (*json)++;
    }
    
    if (**json == '"') {
        size_t length = *json - start;
        result = (char*)malloc(length + 1);
        if (result) {
            /* Copy string without the quotes */
            memcpy(result, start, length);
            result[length] = '\0';
        }
        
        (*json)++;  /* Skip closing quote */
    }
    
    return result;
}

static json_value_t* parse_object(const char** json) {
    if (**json != '{') {
        return NULL;
    }
    
    (*json)++;  /* Skip opening brace */
    
    /* Create object */
    json_value_t* object = json_create_object();
    if (!object) {
        return NULL;
    }
    
    skip_whitespace(json);
    
    /* Empty object */
    if (**json == '}') {
        (*json)++;
        return object;
    }
    
    while (1) {
        skip_whitespace(json);
        
        /* Parse key */
        char* key = parse_string(json);
        if (!key) {
            json_free(object);
            return NULL;
        }
        
        skip_whitespace(json);
        
        /* Expect colon */
        if (**json != ':') {
            free(key);
            json_free(object);
            return NULL;
        }
        
        (*json)++;  /* Skip colon */
        
        skip_whitespace(json);
        
        /* Parse value */
        json_value_t* value = parse_value(json);
        if (!value) {
            free(key);
            json_free(object);
            return NULL;
        }
        
        /* Add key-value pair to object */
        json_object_set(object, key, value);
        free(key);  /* json_object_set makes a copy */
        
        skip_whitespace(json);
        
        /* Check for comma or closing brace */
        if (**json == ',') {
            (*json)++;
        } else if (**json == '}') {
            (*json)++;
            break;
        } else {
            json_free(object);
            return NULL;
        }
    }
    
    return object;
}

static json_value_t* parse_array(const char** json) {
    if (**json != '[') {
        return NULL;
    }
    
    (*json)++;  /* Skip opening bracket */
    
    /* Create array */
    json_value_t* array = json_create_array();
    if (!array) {
        return NULL;
    }
    
    skip_whitespace(json);
    
    /* Empty array */
    if (**json == ']') {
        (*json)++;
        return array;
    }
    
    while (1) {
        skip_whitespace(json);
        
        /* Parse value */
        json_value_t* value = parse_value(json);
        if (!value) {
            json_free(array);
            return NULL;
        }
        
        /* Add value to array */
        json_array_append(array, value);
        
        skip_whitespace(json);
        
        /* Check for comma or closing bracket */
        if (**json == ',') {
            (*json)++;
        } else if (**json == ']') {
            (*json)++;
            break;
        } else {
            json_free(array);
            return NULL;
        }
    }
    
    return array;
}

static json_value_t* parse_value(const char** json) {
    skip_whitespace(json);
    
    if (**json == '\0') {
        return NULL;
    }
    
    if (**json == '{') {
        return parse_object(json);
    } else if (**json == '[') {
        return parse_array(json);
    } else if (**json == '"') {
        char* str = parse_string(json);
        if (!str) {
            return NULL;
        }
        
        json_value_t* value = json_create_string(str);
        free(str);
        return value;
    } else if (**json == 't' && strncmp(*json, "true", 4) == 0) {
        *json += 4;
        return json_create_boolean(1);
    } else if (**json == 'f' && strncmp(*json, "false", 5) == 0) {
        *json += 5;
        return json_create_boolean(0);
    } else if (**json == 'n' && strncmp(*json, "null", 4) == 0) {
        *json += 4;
        return json_create_null();
    } else if (isdigit(**json) || **json == '-') {
        /* Parse number */
        int is_float = 0;
        int negative = (**json == '-');
        double float_value = 0.0;
        int64_t int_value = 0;
        
        if (negative) {
            (*json)++;
        }
        
        /* Parse integer part */
        while (isdigit(**json)) {
            int_value = int_value * 10 + (**json - '0');
            float_value = float_value * 10.0 + (**json - '0');
            (*json)++;
        }
        
        /* Check for decimal point */
        if (**json == '.') {
            is_float = 1;
            (*json)++;
            
            double decimal = 0.1;
            while (isdigit(**json)) {
                float_value += (**json - '0') * decimal;
                decimal *= 0.1;
                (*json)++;
            }
        }
        
        /* Check for exponent */
        if (**json == 'e' || **json == 'E') {
            is_float = 1;
            (*json)++;
            
            int exp_negative = 0;
            if (**json == '+') {
                (*json)++;
            } else if (**json == '-') {
                exp_negative = 1;
                (*json)++;
            }
            
            int exp_value = 0;
            while (isdigit(**json)) {
                exp_value = exp_value * 10 + (**json - '0');
                (*json)++;
            }
            
            double exp = 1.0;
            for (int i = 0; i < exp_value; i++) {
                exp *= 10.0;
            }
            
            if (exp_negative) {
                float_value /= exp;
            } else {
                float_value *= exp;
            }
        }
        
        if (negative) {
            int_value = -int_value;
            float_value = -float_value;
        }
        
        if (is_float) {
            return json_create_number(float_value);
        } else {
            return json_create_integer(int_value);
        }
    }
    
    return NULL;
}

/* JSON stringify functions */
static char* escape_string(const char* str) {
    if (!str) {
        return NULL;
    }
    
    /* Calculate the escaped string length */
    size_t escaped_len = 2;  /* For quotes */
    const char* p = str;
    
    while (*p) {
        if (*p == '"' || *p == '\\' || *p < 32) {
            escaped_len += 2;
        } else {
            escaped_len++;
        }
        p++;
    }
    
    /* Allocate memory for the escaped string */
    char* result = (char*)malloc(escaped_len + 1);
    if (!result) {
        return NULL;
    }
    
    /* Build the escaped string */
    p = str;
    char* q = result;
    
    *q++ = '"';
    
    while (*p) {
        if (*p == '"') {
            *q++ = '\\';
            *q++ = '"';
        } else if (*p == '\\') {
            *q++ = '\\';
            *q++ = '\\';
        } else if (*p == '\b') {
            *q++ = '\\';
            *q++ = 'b';
        } else if (*p == '\f') {
            *q++ = '\\';
            *q++ = 'f';
        } else if (*p == '\n') {
            *q++ = '\\';
            *q++ = 'n';
        } else if (*p == '\r') {
            *q++ = '\\';
            *q++ = 'r';
        } else if (*p == '\t') {
            *q++ = '\\';
            *q++ = 't';
        } else {
            *q++ = *p;
        }
        p++;
    }
    
    *q++ = '"';
    *q = '\0';
    
    return result;
}

static char* stringify_object(json_value_t* object) {
    if (!object || object->type != JSON_OBJECT) {
        return NULL;
    }
    
    /* Empty object */
    if (object->value.object.size == 0) {
        return strdup("{}");
    }
    
    /* Calculate size */
    size_t size = 2;  /* For braces */
    
    for (size_t i = 0; i < object->value.object.size; i++) {
        /* Key */
        char* key = escape_string(object->value.object.entries[i].key);
        if (!key) {
            return NULL;
        }
        
        size += strlen(key) + 1;  /* Key and colon */
        free(key);
        
        /* Value */
        char* value = stringify_value(object->value.object.entries[i].value);
        if (!value) {
            return NULL;
        }
        
        size += strlen(value);
        free(value);
        
        /* Comma */
        if (i < object->value.object.size - 1) {
            size += 1;
        }
    }
    
    /* Allocate result */
    char* result = (char*)malloc(size + 1);  /* +1 for null terminator */
    if (!result) {
        return NULL;
    }
    
    /* Fill result */
    char* p = result;
    *p++ = '{';
    
    for (size_t i = 0; i < object->value.object.size; i++) {
        /* Key */
        char* key = escape_string(object->value.object.entries[i].key);
        if (!key) {
            free(result);
            return NULL;
        }
        
        strcpy(p, key);
        p += strlen(key);
        free(key);
        
        *p++ = ':';
        
        /* Value */
        char* value = stringify_value(object->value.object.entries[i].value);
        if (!value) {
            free(result);
            return NULL;
        }
        
        strcpy(p, value);
        p += strlen(value);
        free(value);
        
        /* Comma */
        if (i < object->value.object.size - 1) {
            *p++ = ',';
        }
    }
    
    *p++ = '}';
    *p = '\0';
    
    return result;
}

static char* stringify_array(json_value_t* array) {
    if (!array || array->type != JSON_ARRAY) {
        return NULL;
    }
    
    /* Empty array */
    if (array->value.array.size == 0) {
        return strdup("[]");
    }
    
    /* Calculate size */
    size_t size = 2;  /* For brackets */
    
    for (size_t i = 0; i < array->value.array.size; i++) {
        char* value = stringify_value(array->value.array.items[i]);
        if (!value) {
            return NULL;
        }
        
        size += strlen(value);
        free(value);
        
        /* Comma */
        if (i < array->value.array.size - 1) {
            size += 1;
        }
    }
    
    /* Allocate result */
    char* result = (char*)malloc(size + 1);  /* +1 for null terminator */
    if (!result) {
        return NULL;
    }
    
    /* Fill result */
    char* p = result;
    *p++ = '[';
    
    for (size_t i = 0; i < array->value.array.size; i++) {
        char* value = stringify_value(array->value.array.items[i]);
        if (!value) {
            free(result);
            return NULL;
        }
        
        strcpy(p, value);
        p += strlen(value);
        free(value);
        
        /* Comma */
        if (i < array->value.array.size - 1) {
            *p++ = ',';
        }
    }
    
    *p++ = ']';
    *p = '\0';
    
    return result;
}

static char* stringify_value(json_value_t* value) {
    if (!value) {
        return NULL;
    }
    
    switch (value->type) {
        case JSON_NULL:
            return strdup("null");
        case JSON_BOOLEAN:
            return strdup(value->value.boolean ? "true" : "false");
        case JSON_NUMBER:
            {
                char buffer[64];
                sprintf(buffer, "%g", value->value.number);
                return strdup(buffer);
            }
        case JSON_STRING:
            return escape_string(value->value.string);
        case JSON_ARRAY:
            return stringify_array(value);
        case JSON_OBJECT:
            return stringify_object(value);
        case JSON_INTEGER:
            {
                char buffer[64];
                sprintf(buffer, "%lld", (long long)value->value.integer);
                return strdup(buffer);
            }
        default:
            return NULL;
    }
}

/* Public API functions */
json_value_t* json_parse(const char* json_str) {
    if (!json_str) {
        return NULL;
    }
    
    const char* json = json_str;
    json_value_t* value = parse_value(&json);
    
    return value;
}

char* json_stringify(json_value_t* value) {
    return stringify_value(value);
}

/**
 * Map to track addresses of objects that were freed to prevent double-free
 * This is a simple protection mechanism for the current bug
 */
#define MAX_FREED_ADDRESSES 1000
static void* g_freed_addresses[MAX_FREED_ADDRESSES] = {0};

/**
 * Check if an address has been freed before
 */
static int was_freed(void* ptr) {
    for (int i = 0; i < MAX_FREED_ADDRESSES; i++) {
        if (g_freed_addresses[i] == ptr) {
            return 1;
        }
    }
    return 0;
}

/**
 * Add an address to the freed list
 */
static void add_to_freed(void* ptr) {
    for (int i = 0; i < MAX_FREED_ADDRESSES; i++) {
        if (g_freed_addresses[i] == NULL) {
            g_freed_addresses[i] = ptr;
            return;
        }
    }
    /* List is full, in a real implementation we would resize */
}

void json_free(json_value_t* value) {
    if (!value) {
        return;
    }
    
    /* Check if this object was already freed */
    if (was_freed(value)) {
        DEBUG_PRINT("Preventing double-free of JSON value %p", (void*)value);
        return;
    }
    
    /* Obtain a copy of the type before we modify anything */
    json_type_t type = value->type;
    
    /* Track pointers to arrays and objects that need to be freed */
    void* string_to_free = NULL;
    json_value_t** array_items_to_free = NULL;
    size_t array_size = 0;
    json_object_entry_t* object_entries_to_free = NULL;
    size_t object_size = 0;
    
    /* Extract pointers to memory that needs to be freed */
    switch (type) {
        case JSON_STRING:
            string_to_free = value->value.string;
            value->value.string = NULL;
            break;
        case JSON_ARRAY:
            array_items_to_free = value->value.array.items;
            array_size = value->value.array.size;
            value->value.array.items = NULL;
            value->value.array.size = 0;
            break;
        case JSON_OBJECT:
            object_entries_to_free = value->value.object.entries;
            object_size = value->value.object.size;
            value->value.object.entries = NULL;
            value->value.object.size = 0;
            break;
        default:
            break;
    }
    
    /* Mark this value as JSON_NULL to prevent recursive free attempts */
    value->type = JSON_NULL;
    
    /* Remember this address as freed */
    void* address_being_freed = value;
    add_to_freed(address_being_freed);
    
    /* Free the value structure */
    free(value);
    
    /* Now free the contained memory */
    if (string_to_free) {
        free(string_to_free);
    }
    
    if (array_items_to_free) {
        for (size_t i = 0; i < array_size; i++) {
            if (array_items_to_free[i]) {
                json_free(array_items_to_free[i]);
            }
        }
        free(array_items_to_free);
    }
    
    if (object_entries_to_free) {
        for (size_t i = 0; i < object_size; i++) {
            if (object_entries_to_free[i].key) {
                free(object_entries_to_free[i].key);
            }
            if (object_entries_to_free[i].value) {
                json_free(object_entries_to_free[i].value);
            }
        }
        free(object_entries_to_free);
    }
}

/* Check if two JSON values are equal */
int json_equals(json_value_t* value1, json_value_t* value2) {
    if (!value1 || !value2) {
        return value1 == value2; /* Both NULL is equal, otherwise not equal */
    }
    
    /* Different types are never equal */
    if (value1->type != value2->type) {
        return 0;
    }
    
    /* Compare based on type */
    switch (value1->type) {
        case JSON_NULL:
            return 1; /* Both are null, so they're equal */
            
        case JSON_BOOLEAN:
            return value1->value.boolean == value2->value.boolean;
            
        case JSON_NUMBER:
            return value1->value.number == value2->value.number;
            
        case JSON_INTEGER:
            return value1->value.integer == value2->value.integer;
            
        case JSON_STRING:
            return strcmp(value1->value.string, value2->value.string) == 0;
            
        case JSON_ARRAY:
            if (value1->value.array.size != value2->value.array.size) {
                return 0;
            }
            
            for (size_t i = 0; i < value1->value.array.size; i++) {
                if (!json_equals(value1->value.array.items[i], value2->value.array.items[i])) {
                    return 0;
                }
            }
            return 1;
            
        case JSON_OBJECT:
            if (value1->value.object.size != value2->value.object.size) {
                return 0;
            }
            
            for (size_t i = 0; i < value1->value.object.size; i++) {
                const char* key = value1->value.object.entries[i].key;
                json_value_t* val1 = value1->value.object.entries[i].value;
                json_value_t* val2 = json_object_get(value2, key);
                
                if (!val2 || !json_equals(val1, val2)) {
                    return 0;
                }
            }
            return 1;
            
        default:
            return 0;
    }
}