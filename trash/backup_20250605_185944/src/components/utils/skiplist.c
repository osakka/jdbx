#include "utils/skiplist.h"
#include <stdlib.h>
#include <time.h>

static int random_level() {
    int level = 1;
    while ((rand() & 0xFFFF) < (0xFFFF * SKIPLIST_P) && level < SKIPLIST_MAX_LEVEL) {
        level++;
    }
    return level;
}

/* Default comparison function */
static int default_compare(const void* a, size_t a_len, const void* b, size_t b_len) {
    size_t min_len = a_len < b_len ? a_len : b_len;
    int cmp = memcmp(a, b, min_len);
    if (cmp != 0) return cmp;
    return (a_len < b_len) ? -1 : (a_len > b_len) ? 1 : 0;
}

skiplist_t* skiplist_create(int (*compare)(const void*, size_t, const void*, size_t)) {
    skiplist_t* list = calloc(1, sizeof(skiplist_t));
    if (!list) return NULL;
    
    /* Create sentinel head node with dummy values */
    char dummy = 0;
    list->head = skiplist_create_node(SKIPLIST_MAX_LEVEL, &dummy, 1, &dummy, 1);
    if (!list->head) {
        free(list);
        return NULL;
    }
    
    list->compare = compare ? compare : default_compare;
    atomic_init(&list->level, 1);
    atomic_init(&list->size, 0);
    atomic_init(&list->insert_count, 0);
    atomic_init(&list->delete_count, 0);
    atomic_init(&list->search_count, 0);
    
    srand(time(NULL));
    
    return list;
}

void skiplist_destroy(skiplist_t* list) {
    if (!list) return;
    
    skiplist_node_t* node = atomic_load(&list->head->next[0]);
    while (node) {
        skiplist_node_t* next = atomic_load(&node->next[0]);
        skiplist_free_node(node);
        node = next;
    }
    
    skiplist_free_node(list->head);
    free(list);
}

bool skiplist_insert(skiplist_t* list, const void* key, size_t key_len,
                    const void* value, size_t value_len) {
    if (!list || !key || !value) return false;
    
    skiplist_node_t* update[SKIPLIST_MAX_LEVEL];
    skiplist_node_t* current = list->head;
    
    /* Find insertion position at each level */
    for (int i = atomic_load(&list->level) - 1; i >= 0; i--) {
        while (true) {
            skiplist_node_t* next = atomic_load(&current->next[i]);
            if (!next || list->compare(key, key_len, next->key, next->key_len) < 0) {
                break;
            }
            current = next;
        }
        update[i] = current;
    }
    
    /* Check if key already exists */
    current = atomic_load(&current->next[0]);
    if (current && list->compare(key, key_len, current->key, current->key_len) == 0) {
        /* Update existing value */
        free(current->value);
        current->value = malloc(value_len);
        if (!current->value) return false;
        memcpy(current->value, value, value_len);
        current->value_len = value_len;
        return true;
    }
    
    /* Create new node */
    int new_level = random_level();
    skiplist_node_t* new_node = skiplist_create_node(new_level, key, key_len, value, value_len);
    if (!new_node) return false;
    
    /* Update list level if necessary */
    if (new_level > atomic_load(&list->level)) {
        for (int i = atomic_load(&list->level); i < new_level; i++) {
            update[i] = list->head;
        }
        atomic_store(&list->level, new_level);
    }
    
    /* Insert node */
    for (int i = 0; i < new_level; i++) {
        new_node->next[i] = update[i]->next[i];
        atomic_store(&update[i]->next[i], new_node);
    }
    
    atomic_fetch_add(&list->size, 1);
    atomic_fetch_add(&list->insert_count, 1);
    return true;
}

void* skiplist_search(skiplist_t* list, const void* key, size_t key_len,
                     size_t* value_len) {
    if (!list || !key) return NULL;
    
    skiplist_node_t* current = list->head;
    
    /* Search from top level down */
    for (int i = atomic_load(&list->level) - 1; i >= 0; i--) {
        while (true) {
            skiplist_node_t* next = atomic_load(&current->next[i]);
            if (!next || list->compare(key, key_len, next->key, next->key_len) <= 0) {
                break;
            }
            current = next;
        }
    }
    
    /* Move to actual node */
    current = atomic_load(&current->next[0]);
    
    /* Check if found */
    if (current && list->compare(key, key_len, current->key, current->key_len) == 0) {
        if (value_len) *value_len = current->value_len;
        void* result = malloc(current->value_len);
        if (result) {
            memcpy(result, current->value, current->value_len);
        }
        atomic_fetch_add(&list->search_count, 1);
        return result;
    }
    
    return NULL;
}

bool skiplist_delete(skiplist_t* list, const void* key, size_t key_len) {
    if (!list || !key) return false;
    
    skiplist_node_t* update[SKIPLIST_MAX_LEVEL];
    skiplist_node_t* current = list->head;
    
    /* Find deletion position at each level */
    for (int i = atomic_load(&list->level) - 1; i >= 0; i--) {
        while (true) {
            skiplist_node_t* next = atomic_load(&current->next[i]);
            if (!next || list->compare(key, key_len, next->key, next->key_len) < 0) {
                break;
            }
            current = next;
        }
        update[i] = current;
    }
    
    /* Check if key exists */
    current = atomic_load(&current->next[0]);
    if (!current || list->compare(key, key_len, current->key, current->key_len) != 0) {
        return false; /* Not found */
    }
    
    /* Remove from all levels */
    for (int i = 0; i < atomic_load(&list->level); i++) {
        if (atomic_load(&update[i]->next[i]) != current) {
            break;
        }
        atomic_store(&update[i]->next[i], atomic_load(&current->next[i]));
    }
    
    /* Update list level if necessary */
    while (atomic_load(&list->level) > 1 && 
           atomic_load(&list->head->next[atomic_load(&list->level) - 1]) == NULL) {
        atomic_fetch_sub(&list->level, 1);
    }
    
    skiplist_free_node(current);
    atomic_fetch_sub(&list->size, 1);
    atomic_fetch_add(&list->delete_count, 1);
    
    return true;
}

skiplist_iterator_t* skiplist_iterator_create(skiplist_t* list) {
    if (!list) return NULL;
    
    skiplist_iterator_t* iter = malloc(sizeof(skiplist_iterator_t));
    if (!iter) return NULL;
    
    iter->list = list;
    iter->current = atomic_load(&list->head->next[0]);
    iter->hp_record = NULL; /* Would use hazard pointers in production */
    
    return iter;
}

void skiplist_iterator_destroy(skiplist_iterator_t* iter) {
    free(iter);
}

bool skiplist_iterator_next(skiplist_iterator_t* iter, 
                           void** key, size_t* key_len,
                           void** value, size_t* value_len) {
    if (!iter || !iter->current) return false;
    
    /* Return current node's data */
    if (key) {
        *key = malloc(iter->current->key_len);
        memcpy(*key, iter->current->key, iter->current->key_len);
    }
    if (key_len) *key_len = iter->current->key_len;
    
    if (value) {
        *value = malloc(iter->current->value_len);
        memcpy(*value, iter->current->value, iter->current->value_len);
    }
    if (value_len) *value_len = iter->current->value_len;
    
    /* Move to next node */
    iter->current = atomic_load(&iter->current->next[0]);
    
    return true;
}

size_t skiplist_size(skiplist_t* list) {
    return list ? atomic_load(&list->size) : 0;
}

void skiplist_stats(skiplist_t* list, uint64_t* inserts, 
                   uint64_t* deletes, uint64_t* searches) {
    if (!list) return;
    
    if (inserts) *inserts = atomic_load(&list->insert_count);
    if (deletes) *deletes = atomic_load(&list->delete_count);
    if (searches) *searches = atomic_load(&list->search_count);
}
