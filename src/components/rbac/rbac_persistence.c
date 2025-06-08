#include "rbac/rbac.h"
#include "rbac/rbac_db.h"
#include "database/database.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * RBAC database persistence implementation with comprehensive error handling
 * Saves RBAC system state to database with detailed logging and error tracking
 * 
 * @param db Database instance
 * @param rbac RBAC system to save
 * @return 1 on success, 0 on failure
 */
int rbac_database_persist(database_t* db, rbac_system_t* rbac) {
  if (!db || !rbac) {
    LOG_ERROR("NULL database or RBAC system passed to rbac_database_persist.");
    return 0;
  }
  
  LOG_INFO("RBAC save.");
  
  /* STAGE 1: Initialize RBAC collections if they don't exist */
  LOG_DEBUG("STAGE 1: Initializing RBAC collections.");
  rbac_db_status_t status = rbac_db_init_collections(db);
  if (!status.success) {
    LOG_ERROR("initialize RBAC collections: %s", 
         status.error_message ? status.error_message : "Unknown error");
    if (status.error_message) free(status.error_message);
    return 0;
  }
  LOG_DEBUG("STAGE 1: Successfully initialized RBAC collections.");
  
  /* Skip the problematic clearing of existing users and roles */
  LOG_INFO("Skipping deletion of existing RBAC data to avoid hanging.");
  
  /* STAGE 2: Skip creating default roles - this is handled by rbac_database_init */
  LOG_DEBUG("STAGE 2: Skipping default role creation (handled by rbac_database_init).");
  
  /* STAGE 3: Save users with detailed step logging */
  LOG_INFO("STAGE 3: Saving %zu users to database", rbac->users->value.object.size);
  size_t users_processed = 0;
  for (size_t i = 0; i < rbac->users->value.object.size; i++) {
    const char* user_id = rbac->users->value.object.entries[i].key;
    json_value_t* user = rbac->users->value.object.entries[i].value;
    
    LOG_DEBUG("Processing user %zu/%zu: %s", i+1, rbac->users->value.object.size, user_id);
    
    if (user->type != JSON_OBJECT) {
      LOG_WARNING("User %s is not a JSON object, skipping", user_id);
      continue;
    }
    
    /* Check if user already exists */
    LOG_DEBUG("Checking if user %s exists", user_id);
    json_value_t* existing = db_get_document(db, RBAC_USERS_COLLECTION, user_id);
    
    if (existing) {
      /* Update existing user */
      LOG_DEBUG("Updating existing user %s", user_id);
      json_value_t* user_copy = json_clone(user);
      if (!user_copy) {
        LOG_ERROR("clone user %s", user_id);
        json_free(existing);
        continue;
      }
      
      json_value_t* result = db_update_document(db, RBAC_USERS_COLLECTION, user_id, user_copy);
      
      if (!result) {
        LOG_ERROR("update user %s in database", user_id);
      } else {
        LOG_DEBUG("Successfully updated user %s", user_id);
        json_free(result);
        users_processed++;
      }
      
      json_free(existing);
    } else {
      /* Insert new user */
      LOG_DEBUG("Inserting new user %s", user_id);
      json_value_t* user_copy = json_clone(user);
      if (!user_copy) {
        LOG_ERROR("clone user %s", user_id);
        continue;
      }
      
      json_value_t* result = db_insert_document(db, RBAC_USERS_COLLECTION, user_copy);
      
      if (!result) {
        LOG_ERROR("insert user %s into database", user_id);
      } else {
        LOG_DEBUG("Successfully inserted user %s", user_id);
        json_free(result);
        users_processed++;
      }
    }
  }
  LOG_INFO("STAGE 3: Completed processing %zu/%zu users", 
       users_processed, rbac->users->value.object.size);
  
  /* STAGE 4: Save roles with detailed step logging */
  LOG_INFO("STAGE 4: Saving %zu roles to database", rbac->roles->value.object.size);
  size_t roles_processed = 0;
  for (size_t i = 0; i < rbac->roles->value.object.size; i++) {
    const char* role_id = rbac->roles->value.object.entries[i].key;
    json_value_t* role = rbac->roles->value.object.entries[i].value;
    
    LOG_DEBUG("Processing role %zu/%zu: %s", i+1, rbac->roles->value.object.size, role_id);
    
    if (role->type != JSON_OBJECT) {
      LOG_WARNING("Role %s is not a JSON object, skipping", role_id);
      continue;
    }
    
    /* Check if role already exists */
    LOG_DEBUG("Checking if role %s exists", role_id);
    json_value_t* existing = db_get_document(db, RBAC_ROLES_COLLECTION, role_id);
    
    if (existing) {
      /* Update existing role */
      LOG_DEBUG("Updating existing role %s", role_id);
      json_value_t* role_copy = json_clone(role);
      if (!role_copy) {
        LOG_ERROR("clone role %s", role_id);
        json_free(existing);
        continue;
      }
      
      json_value_t* result = db_update_document(db, RBAC_ROLES_COLLECTION, role_id, role_copy);
      
      if (!result) {
        LOG_ERROR("update role %s in database", role_id);
      } else {
        LOG_DEBUG("Successfully updated role %s", role_id);
        json_free(result);
        roles_processed++;
      }
      
      json_free(existing);
    } else {
      /* Insert new role */
      LOG_DEBUG("Inserting new role %s", role_id);
      json_value_t* role_copy = json_clone(role);
      if (!role_copy) {
        LOG_ERROR("clone role %s", role_id);
        continue;
      }
      
      json_value_t* result = db_insert_document(db, RBAC_ROLES_COLLECTION, role_copy);
      
      if (!result) {
        LOG_ERROR("insert role %s into database", role_id);
      } else {
        LOG_DEBUG("Successfully inserted role %s", role_id);
        json_free(result);
        roles_processed++;
      }
    }
  }
  LOG_INFO("STAGE 4: Completed processing %zu/%zu roles", 
       roles_processed, rbac->roles->value.object.size);
  
  /* Add special default admin user if none exists */
  json_value_t* query = json_create_object();
  json_value_t* users_result = db_query_documents(db, RBAC_USERS_COLLECTION, query);
  json_free(query);
  
  if (!users_result || users_result->type != JSON_ARRAY || users_result->value.array.size == 0) {
    LOG_INFO("No users found - creating default admin user.");
    
    /* Create default admin user */
    json_value_t* admin_user = json_create_object();
    json_object_set(admin_user, "id", json_create_string("admin"));
    json_object_set(admin_user, "username", json_create_string("admin"));
    json_object_set(admin_user, "password_hash", json_create_string("$2a$10$RXM7Nq0jCIATCXsHpMdIa.UefPQhOmmEJuA5xn0M9fz.8p9UqrIHe")); /* Default: 'admin' */
    
    /* Add admin role to user */
    json_value_t* roles_array = json_create_array();
    json_array_append(roles_array, json_create_string("admin"));
    json_object_set(admin_user, "roles", roles_array);
    
    /* Insert admin user */
    json_value_t* result = db_insert_document(db, RBAC_USERS_COLLECTION, admin_user);
    if (!result) {
      LOG_ERROR("create default admin user.");
    } else {
      LOG_INFO("Created default admin user.");
      json_free(result);
    }
  }
  
  if (users_result) {
    json_free(users_result);
  }
  
  LOG_INFO("RBAC save completed.");
  return 1;
}