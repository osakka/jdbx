#include "utils/ref_counter.h"
#include "utils/diagnostics.h"
#include <stdlib.h>
#include "utils/buffer_pool.h"

/**
 * Create a new reference counted object
 */
ref_counted_t* ref_counter_create(void* object, void (*free_fn)(void*)) {
  if (!object) {
    return NULL;
  }
  
  ref_counted_t* rc = (ref_counted_t*)BUFFER_ALLOC(sizeof(ref_counted_t));
  if (!rc) {
    return NULL;
  }
  
  rc->object = object;
  atomic_init(&rc->ref_count, 1); /* Start with a reference count of 1 */
  rc->free_fn = free_fn;
  
  DEBUG_PRINT("Created reference counter %p for object %p", (void*)rc, object);
  return rc;
}

/**
 * Increase the reference count for an object
 */
size_t ref_counter_acquire(ref_counted_t* rc) {
  if (!rc) {
    return 0;
  }
  
  /* CRITICAL: Thread-safe atomic increment */
  size_t new_count = atomic_fetch_add(&rc->ref_count, 1) + 1;
  return new_count;
}

/**
 * Decrease the reference count for an object.
 * If the reference count reaches zero, the object is freed.
 */
size_t ref_counter_release(ref_counted_t* rc) {
  if (!rc) {
    return 0;
  }
  
  /* CRITICAL: Thread-safe atomic decrement and check */
  size_t old_count = atomic_load(&rc->ref_count);
  DEBUG_PRINT("Releasing reference counter %p with count %zu for object %p", 
      (void*)rc, old_count, rc->object);
  
  if (old_count == 0) {
    return 0; /* Already at zero, prevent underflow */
  }
  
  size_t new_count = atomic_fetch_sub(&rc->ref_count, 1) - 1;
  DEBUG_PRINT("After decrement, count is %zu", new_count);
  
  if (new_count == 0) {
    void* object_to_free = rc->object;
    void (*free_fn)(void*) = rc->free_fn;
    
    /* Mark object as NULL to prevent double-free if something goes wrong */
    rc->object = NULL;
    rc->free_fn = NULL;
    
    DEBUG_PRINT("Freeing reference counter %p", (void*)rc);
    /* Free the reference counted wrapper itself */
    BUFFER_FREE(rc);
    
    /* Free the contained object if we have a free function */
    if (free_fn && object_to_free) {
      DEBUG_PRINT("Calling free function for object %p", object_to_free);
      free_fn(object_to_free);
    }
    
    return 0;
  }
  
  return new_count;
}

/**
 * Get the current reference count
 */
size_t ref_counter_count(ref_counted_t* rc) {
  if (!rc) {
    return 0;
  }
  
  /* Thread-safe atomic read */
  return atomic_load(&rc->ref_count);
}

/**
 * Get the object pointer
 */
void* ref_counter_get(ref_counted_t* rc) {
  if (!rc) {
    return NULL;
  }
  
  return rc->object;
}