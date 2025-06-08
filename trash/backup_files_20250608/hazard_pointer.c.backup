#include "utils/hazard_pointer.h"
#include "utils/logger.h"
#include <stdlib.h>
#include <string.h>

/* Thread-local storage for retired list */
static __thread retired_node_t* tls_retired_list = NULL;
static __thread size_t tls_retired_count = 0;
static __thread hp_record_t* tls_hp_record = NULL;

/* Initialize hazard pointer domain */
hp_domain_t* hp_domain_create(void) {
    hp_domain_t* domain = calloc(1, sizeof(hp_domain_t));
    if (!domain) {
        LOG_ERROR("Cannot allocate hazard pointer domain.");
        return NULL;
    }
    
    pthread_mutex_init(&domain->list_lock, NULL);
    domain->head = NULL;
    
    return domain;
}

/* Destroy hazard pointer domain */
void hp_domain_destroy(hp_domain_t* domain) {
    if (!domain) return;
    
    /* Clean up all HP records */
    pthread_mutex_lock(&domain->list_lock);
    hp_record_t* curr = domain->head;
    while (curr) {
        hp_record_t* next = curr->next;
        free(curr);
        curr = next;
    }
    pthread_mutex_unlock(&domain->list_lock);
    
    pthread_mutex_destroy(&domain->list_lock);
    free(domain);
}

/* Acquire a hazard pointer record for this thread */
hp_record_t* hp_acquire_record(hp_domain_t* domain) {
    if (!domain) return NULL;
    
    /* Check thread-local cache first */
    if (tls_hp_record && tls_hp_record->active) {
        return tls_hp_record;
    }
    
    pthread_t tid = pthread_self();
    
    pthread_mutex_lock(&domain->list_lock);
    
    /* Look for existing inactive record */
    hp_record_t* curr = domain->head;
    while (curr) {
        if (!curr->active) {
            /* Try to claim it */
            if (!__atomic_exchange_n(&curr->active, true, __ATOMIC_ACQUIRE)) {
                curr->thread_id = tid;
                memset(curr->pointers, 0, sizeof(curr->pointers));
                pthread_mutex_unlock(&domain->list_lock);
                tls_hp_record = curr;
                return curr;
            }
        }
        curr = curr->next;
    }
    
    /* No inactive record found, create new one */
    hp_record_t* new_record = calloc(1, sizeof(hp_record_t));
    if (!new_record) {
        pthread_mutex_unlock(&domain->list_lock);
        LOG_ERROR("Cannot allocate hazard pointer record.");
        return NULL;
    }
    
    new_record->thread_id = tid;
    new_record->active = true;
    new_record->next = domain->head;
    domain->head = new_record;
    
    pthread_mutex_unlock(&domain->list_lock);
    
    tls_hp_record = new_record;
    return new_record;
}

/* Release hazard pointer record */
void hp_release_record(hp_record_t* record) {
    if (!record) return;
    
    /* Clear all hazard pointers */
    hp_unprotect_all(record);
    
    /* Mark as inactive */
    __atomic_store_n(&record->active, false, __ATOMIC_RELEASE);
    
    /* Clear thread-local cache */
    if (tls_hp_record == record) {
        tls_hp_record = NULL;
    }
}

/* Protect a pointer */
void hp_protect(hp_record_t* record, int index, void* ptr) {
    if (!record || index >= MAX_HAZARD_POINTERS) return;
    
    __atomic_store_n(&record->pointers[index], ptr, __ATOMIC_RELEASE);
}

/* Unprotect a pointer */
void hp_unprotect(hp_record_t* record, int index) {
    if (!record || index >= MAX_HAZARD_POINTERS) return;
    
    __atomic_store_n(&record->pointers[index], NULL, __ATOMIC_RELEASE);
}

/* Unprotect all pointers */
void hp_unprotect_all(hp_record_t* record) {
    if (!record) return;
    
    for (int i = 0; i < MAX_HAZARD_POINTERS; i++) {
        __atomic_store_n(&record->pointers[i], NULL, __ATOMIC_RELEASE);
    }
}

/* Check if a pointer is hazardous (protected by any thread) */
static bool is_hazardous(hp_domain_t* domain, void* ptr) {
    hp_record_t* curr = domain->head;
    
    while (curr) {
        if (curr->active) {
            for (int i = 0; i < MAX_HAZARD_POINTERS; i++) {
                if (__atomic_load_n(&curr->pointers[i], __ATOMIC_ACQUIRE) == ptr) {
                    return true;
                }
            }
        }
        curr = curr->next;
    }
    
    return false;
}

/* Retire a pointer */
void hp_retire(hp_domain_t* domain, void* ptr, void (*free_func)(void*)) {
    if (!domain || !ptr || !free_func) return;
    
    retired_node_t* node = malloc(sizeof(retired_node_t));
    if (!node) {
        LOG_ERROR("Cannot allocate retired node.");
        /* Emergency: directly free if we can't retire */
        free_func(ptr);
        return;
    }
    
    node->ptr = ptr;
    node->free_func = free_func;
    node->next = tls_retired_list;
    tls_retired_list = node;
    tls_retired_count++;
    
    /* Scan if we have too many retired nodes */
    if (tls_retired_count >= MAX_RETIRED_NODES) {
        hp_scan(domain);
    }
}

/* Scan and reclaim retired pointers */
void hp_scan(hp_domain_t* domain) {
    if (!domain || !tls_retired_list) return;
    
    retired_node_t* prev = NULL;
    retired_node_t* curr = tls_retired_list;
    
    while (curr) {
        retired_node_t* next = curr->next;
        
        if (!is_hazardous(domain, curr->ptr)) {
            /* Safe to reclaim */
            if (prev) {
                prev->next = next;
            } else {
                tls_retired_list = next;
            }
            
            curr->free_func(curr->ptr);
            free(curr);
            tls_retired_count--;
        } else {
            /* Still hazardous, keep it */
            prev = curr;
        }
        
        curr = next;
    }
}