#include <stdio.h>
#include <string.h>
#include "utils/memory_manager.h"
#include "utils/buffer_pool.h"
#include "utils/skiplist.h"
#include "utils/hazard_pointer.h"

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
    
    printf("\nTest 1: BUFFER_CALLOC for skiplist_t\n");
    skiplist_t* list = BUFFER_CALLOC(1, sizeof(skiplist_t));
    printf("BUFFER_CALLOC(1, sizeof(skiplist_t)) = %p\n", list);
    
    if (!list) {
        printf("ERROR: Failed to allocate skiplist structure\n");
        return 1;
    }
    
    printf("\nTest 2: Create sentinel head node\n");
    printf("Creating node with SKIPLIST_MAX_LEVEL=%d\n", SKIPLIST_MAX_LEVEL);
    skiplist_node_t* head = skiplist_create_node(SKIPLIST_MAX_LEVEL, NULL, 0, NULL, 0);
    printf("skiplist_create_node returned: %p\n", head);
    
    if (!head) {
        printf("ERROR: Failed to create head node\n");
        BUFFER_FREE(list);
        return 1;
    }
    
    printf("\nTest 3: Create hazard pointer domain\n");
    hp_domain_t* hp_domain = hp_domain_create();
    printf("hp_domain_create returned: %p\n", hp_domain);
    
    if (!hp_domain) {
        printf("ERROR: Failed to create hazard pointer domain\n");
        skiplist_free_node(head);
        BUFFER_FREE(list);
        return 1;
    }
    
    printf("\nSUCCESS: All components created successfully\n");
    
    /* Cleanup */
    hp_domain_destroy(hp_domain);
    skiplist_free_node(head);
    BUFFER_FREE(list);
    
    return 0;
}