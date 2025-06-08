#ifndef HAZARD_POINTER_H
#define HAZARD_POINTER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>

/* Hazard pointer implementation for safe memory reclamation
 * in lock-free data structures
 */

#define MAX_HAZARD_POINTERS 8
#define MAX_RETIRED_NODES 1000

/* Thread-local hazard pointer record */
typedef struct hp_record {
    void* pointers[MAX_HAZARD_POINTERS];
    struct hp_record* next;
    pthread_t thread_id;
    bool active;
} hp_record_t;

/* Retired node waiting for reclamation */
typedef struct retired_node {
    void* ptr;
    void (*free_func)(void*);
    struct retired_node* next;
} retired_node_t;

/* Hazard pointer domain */
typedef struct hp_domain {
    hp_record_t* head;
    pthread_mutex_t list_lock;
} hp_domain_t;

/* Initialize hazard pointer domain */
hp_domain_t* hp_domain_create(void);
void hp_domain_destroy(hp_domain_t* domain);

/* Acquire/release hazard pointers */
hp_record_t* hp_acquire_record(hp_domain_t* domain);
void hp_release_record(hp_record_t* record);

/* Protect and unprotect pointers */
void hp_protect(hp_record_t* record, int index, void* ptr);
void hp_unprotect(hp_record_t* record, int index);
void hp_unprotect_all(hp_record_t* record);

/* Retire a pointer for later reclamation */
void hp_retire(hp_domain_t* domain, void* ptr, void (*free_func)(void*));

/* Scan and reclaim retired pointers */
void hp_scan(hp_domain_t* domain);

/* Helper macros for common patterns */
#define HP_PROTECT_PTR(record, idx, ptr) do { \
    void* _p; \
    do { \
        _p = __atomic_load_n(&(ptr), __ATOMIC_ACQUIRE); \
        hp_protect(record, idx, _p); \
    } while (_p != __atomic_load_n(&(ptr), __ATOMIC_ACQUIRE)); \
} while(0)

#endif /* HAZARD_POINTER_H */