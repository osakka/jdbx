#ifndef REF_COUNTER_H
#define REF_COUNTER_H

#include <stddef.h>
#include <stdatomic.h>

/**
 * Reference counting system for shared objects.
 * This helps prevent double-free errors by ensuring objects are only
 * freed when no longer referenced by any component.
 */

/**
 * Reference counted object structure
 */
typedef struct ref_counted {
    void* object;            /* Pointer to the actual object */
    atomic_size_t ref_count; /* ATOMIC: Number of references to this object */
    void (*free_fn)(void*);  /* Function to call when freeing the object */
} ref_counted_t;

/**
 * Create a new reference counted object
 * 
 * @param object The object to be reference counted
 * @param free_fn Function to call when freeing the object (can be NULL)
 * @return A new reference counted object or NULL on failure
 */
ref_counted_t* ref_counter_create(void* object, void (*free_fn)(void*));

/**
 * Increase the reference count for an object
 * 
 * @param rc The reference counted object
 * @return The new reference count
 */
size_t ref_counter_acquire(ref_counted_t* rc);

/**
 * Decrease the reference count for an object.
 * If the reference count reaches zero, the object is freed.
 * 
 * @param rc The reference counted object
 * @return The new reference count (0 if object was freed)
 */
size_t ref_counter_release(ref_counted_t* rc);

/**
 * Get the current reference count
 * 
 * @param rc The reference counted object
 * @return The current reference count
 */
size_t ref_counter_count(ref_counted_t* rc);

/**
 * Get the object pointer
 * 
 * @param rc The reference counted object
 * @return The object pointer
 */
void* ref_counter_get(ref_counted_t* rc);

#endif /* REF_COUNTER_H */