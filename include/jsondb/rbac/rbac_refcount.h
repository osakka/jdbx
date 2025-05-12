#ifndef RBAC_REFCOUNT_H
#define RBAC_REFCOUNT_H

#include "rbac/rbac.h"
#include "utils/memory/ref_counter.h"
#include "utils/memory/ref_json.h"

/**
 * Enhanced RBAC system with reference counting for JSON objects
 */

/**
 * RBAC system with reference counting
 */
typedef struct {
    ref_counted_t* users;  /* Reference counted JSON object of users */
    ref_counted_t* roles;  /* Reference counted JSON object of roles */
} rbac_refcount_t;

/**
 * Create a new reference counted RBAC system
 */
rbac_refcount_t* rbac_refcount_init();

/**
 * Free a reference counted RBAC system
 */
void rbac_refcount_free(rbac_refcount_t* rbac);

/**
 * Save a reference counted RBAC system to file
 */
int rbac_refcount_save(rbac_refcount_t* rbac, const char* path);

/**
 * Load a reference counted RBAC system from file
 */
rbac_refcount_t* rbac_refcount_load(const char* path);

/**
 * Get the underlying JSON objects
 */
json_value_t* rbac_refcount_get_users(rbac_refcount_t* rbac);
json_value_t* rbac_refcount_get_roles(rbac_refcount_t* rbac);

/**
 * Convert between regular RBAC and reference counted RBAC
 */
rbac_refcount_t* rbac_to_refcount(rbac_system_t* rbac);
rbac_system_t* rbac_from_refcount(rbac_refcount_t* rbac);

#endif /* RBAC_REFCOUNT_H */