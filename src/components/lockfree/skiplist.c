#include "utils/skiplist.h"
#include "utils/logger.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Default comparison function for binary data */
static int default_compare(const void* a, size_t a_len, const void* b, size_t b_len) {
    size_t min_len = a_len < b_len ? a_len : b_len;
    int cmp = memcmp(a, b, min_len);
    if (cmp != 0) return cmp;
    return (a_len < b_len) ? -1 : (a_len > b_len) ? 1 : 0;
}

/* Generate random level */
static int random_level(void) {
    int level = 1;
    while ((rand() & 1) && level < SKIPLIST_MAX_LEVEL) {
        level++;
    }
    return level;
}

/* Create skip list */
skiplist_t* skiplist_create(int (*compare)(const void*, size_t, const void*, size_t)) {
    skiplist_t* list = calloc(1, sizeof(skiplist_t));
    if (!list) {
        LOG_ERROR("Cannot allocate skip list.");
        return NULL;
    }
    
    /* Initialize random seed */
    srand(time(NULL));
    
    /* Create sentinel head node */
    list->head = skiplist_create_node(SKIPLIST_MAX_LEVEL, NULL, 0, NULL, 0);
    if (!list->head) {
        free(list);
        return NULL;
    }
    
    list->compare = compare ? compare : default_compare;
    list->hp_domain = hp_domain_create();
    if (!list->hp_domain) {
        skiplist_free_node(list->head);
        free(list);
        return NULL;
    }
    
    atomic_init(&list->level, 1);
    atomic_init(&list->size, 0);
    atomic_init(&list->insert_count, 0);
    atomic_init(&list->delete_count, 0);
    atomic_init(&list->search_count, 0);
    
    return list;
}

/* Destroy skip list */
void skiplist_destroy(skiplist_t* list) {
    if (!list) return;
    
    /* Free all nodes */
    skiplist_node_t* curr = list->head;
    while (curr) {
        skiplist_node_t* next = atomic_load(&curr->next[0]);
        skiplist_free_node(curr);
        curr = next;
    }
    
    hp_domain_destroy(list->hp_domain);
    free(list);
}

/* Find predecessors and successors at each level */
static void find_location(skiplist_t* list, const void* key, size_t key_len,
                         skiplist_node_t** preds, skiplist_node_t** succs,
                         hp_record_t* hp_rec) {
    int bottom_level = 0;
    bool marked = false;
    bool snip;
    skiplist_node_t *pred, *curr, *succ;
    
retry:
    while (true) {
        pred = list->head;
        for (int level = SKIPLIST_MAX_LEVEL - 1; level >= bottom_level; level--) {
            HP_PROTECT_PTR(hp_rec, 0, pred);
            curr = atomic_load(&pred->next[level]);
            
            while (true) {
                HP_PROTECT_PTR(hp_rec, 1, curr);
                if (!curr) {
                    succ = NULL;
                    break;
                }
                
                /* Get successor and check if marked */
                succ = atomic_load(&curr->next[level]);
                marked = ((uintptr_t)succ & 1);
                succ = (skiplist_node_t*)((uintptr_t)succ & ~1);
                
                while (marked) {
                    /* Try to physically remove marked node */
                    HP_PROTECT_PTR(hp_rec, 2, succ);
                    snip = atomic_compare_exchange_strong(&pred->next[level],
                                                         &curr, succ);
                    if (!snip) goto retry;
                    
                    /* Node removed, retire it at bottom level */
                    if (level == bottom_level) {
                        hp_retire(list->hp_domain, curr, skiplist_free_node);
                    }
                    
                    curr = succ;
                    if (!curr) break;
                    
                    succ = atomic_load(&curr->next[level]);
                    marked = ((uintptr_t)succ & 1);
                    succ = (skiplist_node_t*)((uintptr_t)succ & ~1);
                }
                
                if (!curr || list->compare(curr->key, curr->key_len, 
                                          key, key_len) >= 0) {
                    break;
                }
                
                pred = curr;
                curr = succ;
            }
            
            preds[level] = pred;
            succs[level] = curr;
        }
        return;
    }
}

/* Insert a key-value pair */
bool skiplist_insert(skiplist_t* list, const void* key, size_t key_len,
                    const void* value, size_t value_len) {
    if (!list || !key || !value) return false;
    
    hp_record_t* hp_rec = hp_acquire_record(list->hp_domain);
    if (!hp_rec) return false;
    
    int top_level = random_level();
    skiplist_node_t* preds[SKIPLIST_MAX_LEVEL];
    skiplist_node_t* succs[SKIPLIST_MAX_LEVEL];
    
    while (true) {
        find_location(list, key, key_len, preds, succs, hp_rec);
        
        /* Check if key already exists */
        if (succs[0] && list->compare(succs[0]->key, succs[0]->key_len,
                                      key, key_len) == 0) {
            /* Key exists, update value */
            void* new_value = malloc(value_len);
            if (!new_value) {
                hp_release_record(hp_rec);
                return false;
            }
            memcpy(new_value, value, value_len);
            
            void* old_value = succs[0]->value;
            succs[0]->value = new_value;
            succs[0]->value_len = value_len;
            free(old_value);
            
            hp_release_record(hp_rec);
            return true;
        }
        
        /* Create new node */
        skiplist_node_t* new_node = skiplist_create_node(top_level, 
                                                         key, key_len,
                                                         value, value_len);
        if (!new_node) {
            hp_release_record(hp_rec);
            return false;
        }
        
        /* Link at all levels */
        for (int level = 0; level < top_level; level++) {
            atomic_store(&new_node->next[level], succs[level]);
        }
        
        /* Try to link at bottom level first */
        if (!atomic_compare_exchange_strong(&preds[0]->next[0],
                                          &succs[0], new_node)) {
            /* Failed, retry */
            skiplist_free_node(new_node);
            continue;
        }
        
        /* Link at higher levels */
        for (int level = 1; level < top_level; level++) {
            while (true) {
                if (atomic_compare_exchange_strong(&preds[level]->next[level],
                                                 &succs[level], new_node)) {
                    break;
                }
                /* Retry find for this level */
                find_location(list, key, key_len, preds, succs, hp_rec);
            }
        }
        
        /* Update list level if needed */
        int old_level = atomic_load(&list->level);
        while (top_level > old_level) {
            if (atomic_compare_exchange_weak(&list->level, &old_level, top_level)) {
                break;
            }
        }
        
        atomic_fetch_add(&list->size, 1);
        atomic_fetch_add(&list->insert_count, 1);
        
        hp_release_record(hp_rec);
        return true;
    }
}

/* Delete a key */
bool skiplist_delete(skiplist_t* list, const void* key, size_t key_len) {
    if (!list || !key) return false;
    
    hp_record_t* hp_rec = hp_acquire_record(list->hp_domain);
    if (!hp_rec) return false;
    
    skiplist_node_t* preds[SKIPLIST_MAX_LEVEL];
    skiplist_node_t* succs[SKIPLIST_MAX_LEVEL];
    skiplist_node_t* succ;
    bool marked = false;
    
    while (true) {
        find_location(list, key, key_len, preds, succs, hp_rec);
        
        if (!succs[0] || list->compare(succs[0]->key, succs[0]->key_len,
                                       key, key_len) != 0) {
            /* Key not found */
            hp_release_record(hp_rec);
            return false;
        }
        
        /* Mark for deletion from top to bottom */
        skiplist_node_t* node_to_delete = succs[0];
        for (int level = SKIPLIST_MAX_LEVEL - 1; level >= 1; level--) {
            succ = atomic_load(&node_to_delete->next[level]);
            marked = ((uintptr_t)succ & 1);
            succ = (skiplist_node_t*)((uintptr_t)succ & ~1);
            
            while (!marked) {
                skiplist_node_t* marked_succ = (skiplist_node_t*)((uintptr_t)succ | 1);
                if (atomic_compare_exchange_strong(&node_to_delete->next[level],
                                                 &succ, marked_succ)) {
                    marked = true;
                } else {
                    marked = ((uintptr_t)succ & 1);
                    succ = (skiplist_node_t*)((uintptr_t)succ & ~1);
                }
            }
        }
        
        /* Mark bottom level */
        succ = atomic_load(&node_to_delete->next[0]);
        marked = ((uintptr_t)succ & 1);
        
        while (!marked) {
            skiplist_node_t* marked_succ = (skiplist_node_t*)((uintptr_t)succ | 1);
            if (atomic_compare_exchange_strong(&node_to_delete->next[0],
                                             &succ, marked_succ)) {
                /* Successfully marked, now try to physically remove */
                find_location(list, key, key_len, preds, succs, hp_rec);
                atomic_fetch_sub(&list->size, 1);
                atomic_fetch_add(&list->delete_count, 1);
                hp_release_record(hp_rec);
                return true;
            }
            marked = ((uintptr_t)succ & 1);
        }
        
        /* Already marked by another thread */
        hp_release_record(hp_rec);
        return false;
    }
}

/* Search for a key */
void* skiplist_search(skiplist_t* list, const void* key, size_t key_len,
                     size_t* value_len) {
    if (!list || !key) return NULL;
    
    hp_record_t* hp_rec = hp_acquire_record(list->hp_domain);
    if (!hp_rec) return NULL;
    
    skiplist_node_t* preds[SKIPLIST_MAX_LEVEL];
    skiplist_node_t* succs[SKIPLIST_MAX_LEVEL];
    
    find_location(list, key, key_len, preds, succs, hp_rec);
    
    atomic_fetch_add(&list->search_count, 1);
    
    if (succs[0] && list->compare(succs[0]->key, succs[0]->key_len,
                                  key, key_len) == 0) {
        /* Found */
        void* result = malloc(succs[0]->value_len);
        if (result) {
            memcpy(result, succs[0]->value, succs[0]->value_len);
            if (value_len) *value_len = succs[0]->value_len;
        }
        hp_release_record(hp_rec);
        return result;
    }
    
    hp_release_record(hp_rec);
    return NULL;
}

/* Create iterator */
skiplist_iterator_t* skiplist_iterator_create(skiplist_t* list) {
    if (!list) return NULL;
    
    skiplist_iterator_t* iter = calloc(1, sizeof(skiplist_iterator_t));
    if (!iter) return NULL;
    
    iter->list = list;
    iter->hp_record = hp_acquire_record(list->hp_domain);
    if (!iter->hp_record) {
        free(iter);
        return NULL;
    }
    
    /* Start at first real node */
    iter->current = atomic_load(&list->head->next[0]);
    HP_PROTECT_PTR(iter->hp_record, 0, iter->current);
    
    /* Skip marked nodes */
    while (iter->current) {
        skiplist_node_t* next = atomic_load(&iter->current->next[0]);
        if (!((uintptr_t)next & 1)) break;
        iter->current = (skiplist_node_t*)((uintptr_t)next & ~1);
        HP_PROTECT_PTR(iter->hp_record, 0, iter->current);
    }
    
    return iter;
}

/* Destroy iterator */
void skiplist_iterator_destroy(skiplist_iterator_t* iter) {
    if (!iter) return;
    
    hp_release_record(iter->hp_record);
    free(iter);
}

/* Get next item from iterator */
bool skiplist_iterator_next(skiplist_iterator_t* iter,
                           void** key, size_t* key_len,
                           void** value, size_t* value_len) {
    if (!iter || !iter->current) return false;
    
    /* Return current item */
    if (key) {
        *key = malloc(iter->current->key_len);
        if (*key) {
            memcpy(*key, iter->current->key, iter->current->key_len);
            if (key_len) *key_len = iter->current->key_len;
        }
    }
    
    if (value) {
        *value = malloc(iter->current->value_len);
        if (*value) {
            memcpy(*value, iter->current->value, iter->current->value_len);
            if (value_len) *value_len = iter->current->value_len;
        }
    }
    
    /* Move to next non-marked node */
    skiplist_node_t* next;
    do {
        next = atomic_load(&iter->current->next[0]);
        iter->current = (skiplist_node_t*)((uintptr_t)next & ~1);
        HP_PROTECT_PTR(iter->hp_record, 0, iter->current);
    } while (iter->current && ((uintptr_t)atomic_load(&iter->current->next[0]) & 1));
    
    return true;
}

/* Get skip list size */
size_t skiplist_size(skiplist_t* list) {
    if (!list) return 0;
    return atomic_load(&list->size);
}

/* Get skip list statistics */
void skiplist_stats(skiplist_t* list, uint64_t* inserts,
                   uint64_t* deletes, uint64_t* searches) {
    if (!list) return;
    
    if (inserts) *inserts = atomic_load(&list->insert_count);
    if (deletes) *deletes = atomic_load(&list->delete_count);
    if (searches) *searches = atomic_load(&list->search_count);
}