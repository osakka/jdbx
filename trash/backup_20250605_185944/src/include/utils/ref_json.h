#ifndef REF_JSON_H
#define REF_JSON_H

#include "utils/json.h"
#include "utils/ref_counter.h"

/**
 * Reference counted JSON values.
 * This provides a wrapper around regular JSON values that uses
 * reference counting to prevent double-free errors with shared objects.
 */

/**
 * Create a reference counted JSON value
 * 
 * @param value The JSON value to wrap
 * @return A reference counted wrapper or NULL on failure
 */
ref_counted_t* ref_json_create(json_value_t* value);

/**
 * Get the JSON value from a reference counted wrapper
 * 
 * @param rc The reference counted wrapper
 * @return The JSON value or NULL if invalid
 */
json_value_t* ref_json_get(ref_counted_t* rc);

/**
 * Get a reference counted JSON value. This creates a new reference
 * if the object is already reference counted, or wraps it if not.
 * 
 * @param value The JSON value to get or wrap
 * @return A reference counted wrapper or NULL on failure
 */
ref_counted_t* ref_json_acquire(json_value_t* value);

/**
 * Release a reference to a JSON value. If this was the last reference,
 * the JSON value will be freed.
 * 
 * @param rc The reference counted wrapper
 * @return The new reference count (0 if object was freed)
 */
size_t ref_json_release(ref_counted_t* rc);

/**
 * Free function for JSON values that can be used with ref_counter_create
 */
void ref_json_free(void* value);

#endif /* REF_JSON_H */