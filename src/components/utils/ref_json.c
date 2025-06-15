#include "utils/ref_json.h"
#include "utils/diagnostics.h"
#include <stdlib.h>

/* Map to track existing reference counted JSON values */
/* We'll use a simple array for now, but a proper implementation should use a hash table */
#define MAX_REF_JSON_OBJECTS 1000
static struct {
  json_value_t* json_value;
  ref_counted_t* ref_counted;
} g_ref_json_map[MAX_REF_JSON_OBJECTS] = {0};

/**
 * Find a reference counted object in the map
 */
static ref_counted_t* find_ref_json(json_value_t* value) {
  if (!value) {
    return NULL;
  }
  
  DEBUG_PRINT("Looking for JSON value %p in ref map", (void*)value);
  for (int i = 0; i < MAX_REF_JSON_OBJECTS; i++) {
    if (g_ref_json_map[i].json_value == value) {
      DEBUG_PRINT("Found at index %d with ref counted wrapper %p", 
          i, (void*)g_ref_json_map[i].ref_counted);
      return g_ref_json_map[i].ref_counted;
    }
  }
  DEBUG_PRINT("Not found in map");
  return NULL;
}

/**
 * Add a reference counted object to the map
 */
static void add_ref_json(json_value_t* value, ref_counted_t* rc) {
  if (!value || !rc) {
    return;
  }
  
  DEBUG_PRINT("Adding JSON value %p with ref counter %p to ref map", (void*)value, (void*)rc);
  
  for (int i = 0; i < MAX_REF_JSON_OBJECTS; i++) {
    if (g_ref_json_map[i].json_value == NULL) {
      g_ref_json_map[i].json_value = value;
      g_ref_json_map[i].ref_counted = rc;
      DEBUG_PRINT("Added at index %d", i);
      return;
    }
  }
  DEBUG_PRINT("Map is full, couldn't add to ref map");
  /* Map is full, in a real implementation we would resize */
}

/**
 * Remove a reference counted object from the map
 */
static void remove_ref_json(json_value_t* value) {
  if (!value) {
    return;
  }
  
  DEBUG_PRINT("Removing JSON value %p from ref map", (void*)value);
  
  for (int i = 0; i < MAX_REF_JSON_OBJECTS; i++) {
    if (g_ref_json_map[i].json_value == value) {
      DEBUG_PRINT("Found at index %d, removing", i);
      g_ref_json_map[i].json_value = NULL;
      g_ref_json_map[i].ref_counted = NULL;
      return;
    }
  }
  
  DEBUG_PRINT("Value not found in map for removal");
}

/**
 * Free function for JSON values
 */
void ref_json_free(void* value) {
  if (!value) {
    return;
  }
  
  json_value_t* json_val = (json_value_t*)value;
  
  /* Remove from map before freeing */
  remove_ref_json(json_val);
  
  /* Call the original json_free function */
  DEBUG_PRINT("Freeing JSON value %p", (void*)json_val);
  json_free(json_val);
}

/**
 * Create a reference counted JSON value
 */
ref_counted_t* ref_json_create(json_value_t* value) {
  if (!value) {
    return NULL;
  }
  
  /* Check if the value is already reference counted */
  ref_counted_t* existing = find_ref_json(value);
  if (existing) {
    /* Increase reference count and return existing wrapper */
    ref_counter_acquire(existing);
    return existing;
  }
  
  /* Create a new reference counted wrapper */
  ref_counted_t* rc = ref_counter_create(value, ref_json_free);
  if (!rc) {
    return NULL;
  }
  
  /* Add to the map */
  add_ref_json(value, rc);
  
  return rc;
}

/**
 * Get the JSON value from a reference counted wrapper
 */
json_value_t* ref_json_get(ref_counted_t* rc) {
  if (!rc) {
    return NULL;
  }
  
  return (json_value_t*)ref_counter_get(rc);
}

/**
 * Get a reference counted JSON value
 */
ref_counted_t* ref_json_acquire(json_value_t* value) {
  if (!value) {
    return NULL;
  }
  
  /* Check if the value is already reference counted */
  ref_counted_t* existing = find_ref_json(value);
  if (existing) {
    /* Increase reference count and return existing wrapper */
    ref_counter_acquire(existing);
    return existing;
  }
  
  /* Create a new reference counted wrapper */
  return ref_json_create(value);
}

/**
 * Release a reference to a JSON value
 */
size_t ref_json_release(ref_counted_t* rc) {
  if (!rc) {
    return 0;
  }
  
  return ref_counter_release(rc);
}