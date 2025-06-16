#include "rbac/rbac_refcount.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils/buffer_pool.h"

/**
 * Create a new reference counted RBAC system
 */
rbac_refcount_t* rbac_refcount_init() {
  rbac_refcount_t* rbac = (rbac_refcount_t*)BUFFER_ALLOC(sizeof(rbac_refcount_t));
  if (!rbac) {
    return NULL;
  }
  
  /* Create empty JSON objects with reference counting */
  json_value_t* users = json_create_object();
  json_value_t* roles = json_create_object();
  
  if (!users || !roles) {
    if (users) json_free(users);
    if (roles) json_free(roles);
    BUFFER_FREE(rbac);
    return NULL;
  }
  
  rbac->users = ref_json_create(users);
  rbac->roles = ref_json_create(roles);
  
  if (!rbac->users || !rbac->roles) {
    if (rbac->users) ref_json_release(rbac->users);
    if (rbac->roles) ref_json_release(rbac->roles);
    BUFFER_FREE(rbac);
    return NULL;
  }
  
  return rbac;
}

/**
 * Free a reference counted RBAC system
 */
void rbac_refcount_free(rbac_refcount_t* rbac) {
  if (!rbac) {
    return;
  }
  
  /* Release references to JSON objects */
  if (rbac->users) {
    ref_json_release(rbac->users);
  }
  
  if (rbac->roles) {
    ref_json_release(rbac->roles);
  }
  
  /* Free the RBAC structure */
  BUFFER_FREE(rbac);
}

/**
 * Save a reference counted RBAC system to file
 */
int rbac_refcount_save(rbac_refcount_t* rbac, const char* path) {
  if (!rbac || !path) {
    return 0;
  }
  
  /* Get the underlying JSON objects */
  json_value_t* users = ref_json_get(rbac->users);
  json_value_t* roles = ref_json_get(rbac->roles);
  
  if (!users || !roles) {
    return 0;
  }
  
  /* Create a JSON object to hold the RBAC system */
  json_value_t* rbac_json = json_create_object();
  if (!rbac_json) {
    return 0;
  }
  
  /* Copy the users and roles to the JSON object */
  json_object_set(rbac_json, "users", json_clone(users));
  json_object_set(rbac_json, "roles", json_clone(roles));
  
  /* Stringify the JSON object */
  char* json_str = json_stringify(rbac_json);
  
  /* Free the JSON object */
  json_free(rbac_json);
  
  if (!json_str) {
    return 0;
  }
  
  /* Write to file */
  FILE* file = fopen(path, "w");
  if (!file) {
    BUFFER_FREE(json_str);
    return 0;
  }
  
  fprintf(file, "%s", json_str);
  fclose(file);
  
  BUFFER_FREE(json_str);
  
  return 1;
}

/**
 * Load a reference counted RBAC system from file
 */
rbac_refcount_t* rbac_refcount_load(const char* path) {
  if (!path) {
    return NULL;
  }
  
  /* Read file contents */
  FILE* file = fopen(path, "r");
  if (!file) {
    return NULL;
  }
  
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);
  
  char* json_str = (char*)BUFFER_ALLOC(size + 1);
  if (!json_str) {
    fclose(file);
    return NULL;
  }
  
  size_t read_size = fread(json_str, 1, size, file);
  json_str[read_size] = '\0';
  fclose(file);
  
  /* Parse the JSON */
  json_value_t* rbac_json = json_parse(json_str);
  BUFFER_FREE(json_str);
  
  if (!rbac_json || rbac_json->type != JSON_OBJECT) {
    if (rbac_json) json_free(rbac_json);
    return NULL;
  }
  
  /* Get users and roles */
  json_value_t* users = json_object_get(rbac_json, "users");
  json_value_t* roles = json_object_get(rbac_json, "roles");
  
  if (!users || users->type != JSON_OBJECT || 
    !roles || roles->type != JSON_OBJECT) {
    json_free(rbac_json);
    return NULL;
  }
  
  /* Create a new RBAC system */
  rbac_refcount_t* rbac = (rbac_refcount_t*)BUFFER_ALLOC(sizeof(rbac_refcount_t));
  if (!rbac) {
    json_free(rbac_json);
    return NULL;
  }
  
  /* Create reference counted wrappers for the JSON objects */
  rbac->users = ref_json_create(json_clone(users));
  rbac->roles = ref_json_create(json_clone(roles));
  
  /* Free the JSON object */
  json_free(rbac_json);
  
  if (!rbac->users || !rbac->roles) {
    if (rbac->users) ref_json_release(rbac->users);
    if (rbac->roles) ref_json_release(rbac->roles);
    BUFFER_FREE(rbac);
    return NULL;
  }
  
  return rbac;
}

/**
 * Get the underlying JSON objects
 */
json_value_t* rbac_refcount_get_users(rbac_refcount_t* rbac) {
  if (!rbac || !rbac->users) {
    return NULL;
  }
  
  return ref_json_get(rbac->users);
}

json_value_t* rbac_refcount_get_roles(rbac_refcount_t* rbac) {
  if (!rbac || !rbac->roles) {
    return NULL;
  }
  
  return ref_json_get(rbac->roles);
}

/**
 * Convert between regular RBAC and reference counted RBAC
 */
rbac_refcount_t* rbac_to_refcount(rbac_system_t* rbac) {
  if (!rbac) {
    return NULL;
  }
  
  rbac_refcount_t* ref_rbac = (rbac_refcount_t*)BUFFER_ALLOC(sizeof(rbac_refcount_t));
  if (!ref_rbac) {
    return NULL;
  }
  
  /* Create reference counted wrappers for the JSON objects */
  ref_rbac->users = ref_json_create(rbac->users);
  ref_rbac->roles = ref_json_create(rbac->roles);
  
  if (!ref_rbac->users || !ref_rbac->roles) {
    if (ref_rbac->users) ref_json_release(ref_rbac->users);
    if (ref_rbac->roles) ref_json_release(ref_rbac->roles);
    BUFFER_FREE(ref_rbac);
    return NULL;
  }
  
  /* Clear the pointers in the original RBAC to prevent double-free */
  /* FIXME: This breaks the API which still uses the original RBAC pointer */
  /* rbac->users = NULL; */
  /* rbac->roles = NULL; */
  
  return ref_rbac;
}

rbac_system_t* rbac_from_refcount(rbac_refcount_t* rbac) {
  if (!rbac) {
    return NULL;
  }
  
  rbac_system_t* reg_rbac = (rbac_system_t*)BUFFER_ALLOC(sizeof(rbac_system_t));
  if (!reg_rbac) {
    return NULL;
  }
  
  /* Get the underlying JSON objects */
  reg_rbac->users = ref_json_get(rbac->users);
  reg_rbac->roles = ref_json_get(rbac->roles);
  
  return reg_rbac;
}