#include "utils/ref_counter.h"
#include "utils/diagnostics.h"
#include <stdlib.h>

/**
 * Create a new reference counted object
 */
ref_counted_t* ref_counter_create(void* object, void (*free_fn)(void*)) {
  if (!object) {
    return NULL;
  }
  
  ref_counted_t* rc = (ref_counted_t*)malloc(sizeof(ref_counted_t));
  if (!rc) {
    return NULL;
  }
  
  rc->object = object;
  rc->ref_count = 1; /* Start with a reference count of 1 */
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
  
  rc->ref_count++;
  return rc->ref_count;
}

/**
 * Decrease the reference count for an object.
 * If the reference count reaches zero, the object is freed.
 */
size_t ref_counter_release(ref_counted_t* rc) {
  if (!rc) {
    return 0;
  }
  
  DEBUG_PRINT("Releasing reference counter %p with count %zu for object %p", 
      (void*)rc, rc->ref_count, rc->object);
  
  if (rc->ref_count > 0) {
    rc->ref_count--;
  }
  
  DEBUG_PRINT("After decrement, count is %zu", rc->ref_count);
  
  if (rc->ref_count == 0) {
    void* object_to_free = rc->object;
    void (*free_fn)(void*) = rc->free_fn;
    
    /* Mark object as NULL to prevent double-free if something goes wrong */
    rc->object = NULL;
    rc->free_fn = NULL;
    
    DEBUG_PRINT("Freeing reference counter %p", (void*)rc);
    /* Free the reference counted wrapper itself */
    free(rc);
    
    /* Free the contained object if we have a free function */
    if (free_fn && object_to_free) {
      DEBUG_PRINT("Calling free function for object %p", object_to_free);
      free_fn(object_to_free);
    }
    
    return 0;
  }
  
  return rc->ref_count;
}

/**
 * Get the current reference count
 */
size_t ref_counter_count(ref_counted_t* rc) {
  if (!rc) {
    return 0;
  }
  
  return rc->ref_count;
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