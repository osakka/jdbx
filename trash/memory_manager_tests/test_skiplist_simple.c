#include <stdio.h>
#include <string.h>
#include "utils/memory_manager.h"
#include "utils/skiplist.h"

/* Simple string comparison */
static int my_string_compare(const void* a, size_t a_len, const void* b, size_t b_len) {
    size_t min_len = a_len < b_len ? a_len : b_len;
    int cmp = memcmp(a, b, min_len);
    if (cmp != 0) return cmp;
    return (a_len < b_len) ? -1 : (a_len > b_len) ? 1 : 0;
}

int main() {
    printf("Initializing memory manager...\n");
    memory_manager_init();
    
    printf("Creating skiplist...\n");
    skiplist_t* list = skiplist_create(my_string_compare);
    
    if (list) {
        printf("SUCCESS: Skiplist created at %p\n", list);
        skiplist_destroy(list);
        printf("SUCCESS: Skiplist destroyed\n");
    } else {
        printf("ERROR: Failed to create skiplist\n");
    }
    
    return 0;
}