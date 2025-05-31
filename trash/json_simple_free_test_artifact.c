/**
 * Simplified JSON free implementation for testing memory optimizations
 * This temporarily removes complex double-free protection to isolate issues
 */

#include "utils/json.h"
#include "utils/buffer_pool.h"

/* Simplified recursive free function */
static void json_free_simple_recursive(json_value_t* value) {
    if (!value) return;
    
    switch (value->type) {
        case JSON_STRING:
            if (value->value.string) {
                buffer_pool_free(value->value.string);
            }
            break;
            
        case JSON_ARRAY:
            if (value->value.array.items) {
                for (size_t i = 0; i < value->value.array.size; i++) {
                    if (value->value.array.items[i]) {
                        json_free_simple_recursive(value->value.array.items[i]);
                    }
                }
                buffer_pool_free(value->value.array.items);
            }
            break;
            
        case JSON_OBJECT:
            if (value->value.object.entries) {
                for (size_t i = 0; i < value->value.object.size; i++) {
                    if (value->value.object.entries[i].key) {
                        buffer_pool_free(value->value.object.entries[i].key);
                    }
                    if (value->value.object.entries[i].value) {
                        json_free_simple_recursive(value->value.object.entries[i].value);
                    }
                }
                buffer_pool_free(value->value.object.entries);
            }
            break;
            
        default:
            /* NULL, BOOLEAN, NUMBER, INTEGER - no dynamic memory */
            break;
    }
    
    buffer_pool_free(value);
}

/* Simple free function for testing */
void json_free_simple(json_value_t* value) {
    json_free_simple_recursive(value);
}