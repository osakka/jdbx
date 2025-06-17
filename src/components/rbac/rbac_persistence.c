#include "rbac/rbac.h"
#include "rbac/rbac_db.h"
#include "database/database.h"
#include "database/document_storage.h"
#include "database/virtual_layer.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils/buffer_pool.h"

/**
 * RBAC database persistence implementation
 * Since we use the database as the single source of truth, this function
 * now only ensures RBAC collections are initialized. All RBAC data is
 * stored directly in the database, not persisted from memory structures.
 * 
 * @param db Database instance
 * @param rbac RBAC system (parameter kept for compatibility but not used)
 * @return 1 on success, 0 on failure
 */
int rbac_database_persist(database_t* db, rbac_system_t* rbac) {
  if (!db) {
    LOG_ERROR("NULL database passed to rbac_database_persist.");
    return 0;
  }
  
  /* Note: rbac parameter is no longer used since we don't persist from memory structures */
  (void)rbac;
  
  LOG_INFO("Initializing RBAC database collections.");
  
  /* Initialize RBAC collections if they don't exist */
  rbac_db_status_t status = rbac_db_init_collections(db);
  if (!status.success) {
    LOG_ERROR("Failed to initialize RBAC collections: %s", 
         status.error_message ? status.error_message : "Unknown error");
    if (status.error_message) BUFFER_FREE(status.error_message);
    return 0;
  }
  
  LOG_INFO("RBAC database collections initialized successfully.");
  
  /* Check if we need to create a default admin user */
  json_value_t* query = json_create_object();
  json_value_t* users_result = virtual_query(db, DOC_TYPE_NAME_USER, "system", VIRTUAL_COLLECTION_USERS, query);
  json_free(query);
  
  /* Extract documents array from response object */
  json_value_t* users_documents = json_object_get(users_result, "documents");
  if (!users_documents || users_documents->type != JSON_ARRAY || users_documents->value.array.size == 0) {
    LOG_INFO("No users found - creating default admin user.");
    
    /* Create default admin user */
    json_value_t* admin_user = json_create_object();
    json_object_set(admin_user, "id", json_create_string("admin"));
    json_object_set(admin_user, "username", json_create_string("admin"));
    json_object_set(admin_user, "password_hash", json_create_string("$2a$10$RXM7Nq0jCIATCXsHpMdIa.UefPQhOmmEJuA5xn0M9fz.8p9UqrIHe")); /* Default: 'admin' */
    json_object_set(admin_user, "type", json_create_string("user"));
    json_object_set(admin_user, "library", json_create_string("system"));
    json_object_set(admin_user, "collection", json_create_string("users"));
    
    /* Add admin role to user */
    json_value_t* roles_array = json_create_array();
    json_array_append(roles_array, json_create_string("admin"));
    json_object_set(admin_user, "roles", roles_array);
    
    /* Insert admin user */
    json_value_t* result = virtual_insert(db, DOC_TYPE_NAME_USER, "system", VIRTUAL_COLLECTION_USERS, admin_user, SYSTEM_USER_ADMIN);
    if (!result) {
      LOG_ERROR("Failed to create default admin user.");
    } else {
      LOG_INFO("Created default admin user.");
      json_free(result);
    }
  }
  
  if (users_result) {
    json_free(users_result);
  }
  
  return 1;
}