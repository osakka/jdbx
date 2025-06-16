#include <stdio.h>
#include <string.h>
#include <time.h>
#include "utils/memory_manager.h"
#include "utils/skiplist.h"

/* Same default comparison as in skiplist.c */
static int default_compare(const void* a, size_t a_len, const void* b, size_t b_len) {
    size_t min_len = a_len < b_len ? a_len : b_len;
    int cmp = memcmp(a, b, min_len);
    if (cmp != 0) return cmp;
    return (a_len < b_len) ? -1 : (a_len > b_len) ? 1 : 0;
}

/* Same as skiplist_string_compare in database.c */
static int skiplist_string_compare(const void* a, size_t a_len, const void* b, size_t b_len) {
    (void)a_len;
    (void)b_len;
    return strcmp((const char*)a, (const char*)b);
}

int main() {
    printf("Initializing memory manager...\n");
    memory_manager_init();
    
    printf("\nMimicking exact skiplist_create flow:\n");
    
    /* Step 1: Allocate skiplist structure */
    printf("1. BUFFER_CALLOC(1, sizeof(skiplist_t))\n");
    skiplist_t* list = BUFFER_CALLOC(1, sizeof(skiplist_t));
    printf("   Result: %p\n", list);
    if (!list) {
        printf("   ERROR: Failed to allocate\n");
        return 1;
    }
    
    /* Step 2: Initialize random seed */
    printf("2. srand(time(NULL))\n");
    srand(time(NULL));
    
    /* Step 3: Create sentinel head node */
    printf("3. skiplist_create_node(SKIPLIST_MAX_LEVEL=%d, NULL, 0, NULL, 0)\n", SKIPLIST_MAX_LEVEL);
    list->head = skiplist_create_node(SKIPLIST_MAX_LEVEL, NULL, 0, NULL, 0);
    printf("   Result: %p\n", list->head);
    if (!list->head) {
        printf("   ERROR: Failed to create head node\n");
        BUFFER_FREE(list);
        return 1;
    }
    
    /* Step 4: Set compare function */
    printf("4. Setting compare function\n");
    list->compare = skiplist_string_compare;
    
    /* Step 5: Create hazard pointer domain */
    printf("5. hp_domain_create()\n");
    list->hp_domain = hp_domain_create();
    printf("   Result: %p\n", list->hp_domain);
    if (!list->hp_domain) {
        printf("   ERROR: Failed to create hp_domain\n");
        skiplist_free_node(list->head);
        BUFFER_FREE(list);
        return 1;
    }
    
    /* Step 6: Initialize atomics */
    printf("6. Initializing atomic fields\n");
    atomic_init(&list->level, 1);
    atomic_init(&list->size, 0);
    atomic_init(&list->insert_count, 0);
    atomic_init(&list->delete_count, 0);
    atomic_init(&list->search_count, 0);
    
    printf("\nSUCCESS: Skiplist created successfully!\n");
    
    /* Test with actual skiplist_create */
    printf("\nNow testing actual skiplist_create:\n");
    skiplist_t* real_list = skiplist_create(skiplist_string_compare);
    printf("skiplist_create returned: %p\n", real_list);
    
    if (real_list) {
        printf("SUCCESS: Real skiplist created\n");
        skiplist_destroy(real_list);
    } else {
        printf("ERROR: Real skiplist creation failed\n");
    }
    
    /* Cleanup manual list */
    skiplist_destroy(list);
    
    return 0;
}