#include "rbac/rbac_db.h"
#include "rbac/rbac_unified_documents.h"
#include "database/document_storage.h"
#include "database/database.h"
#include "database/virtual_layer.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Forward declarations for helper functions */
static json_value_t* find_role_by_uuid(database_t* db, const char* uuid);
static json_value_t* find_user_by_uuid(database_t* db, const char* uuid);

/* Generate a simple UUID */
static char* generate_uuid() {
  char* uuid = (char*)BUFFER_ALLOC(37); /* 36 chars + null terminator */
  if (!uuid) return NULL;
  
  /* Format: 8-4-4-4-12 hexadecimal digits */
  srand((unsigned int)time(NULL) + rand());
  sprintf(uuid, "%08x-%04x-%04x-%04x-%04x%08x",
      rand() & 0xFFFFFFFF,
      rand() & 0xFFFF,
      ((rand() & 0xFFFF) & 0x0FFF) | 0x4000, /* Version 4 */
      ((rand() & 0xFFFF) & 0x3FFF) | 0x8000, /* Variant 1 */
      rand() & 0xFFFF,
      rand() & 0xFFFFFFFF);
  
  return uuid;
}

/* Initialize RBAC collections in the database */
rbac_db_status_t rbac_db_init_collections(database_t* db) {
  rbac_db_status_t status = {0};
  
  if (!db) {
    status.error_message = BUFFER_STRDUP("Database is NULL");
    return status;
  }
  
  /* UNIFIED DOCUMENTS: No collections to create!
   * All RBAC data is stored in the unified "default/documents" collection
   * with type-based discrimination. This function is now a no-op.
   * 
   * Indexes are created automatically by the adaptive indexing system
   * based on query patterns, so we don't need to create them manually.
   */
  status.success = 1;
  status.collections_created = 0; /* No physical collections needed */
  status.indexes_created = 0;     /* Adaptive indexing handles this */
  LOG_DEBUG("RBAC: Using unified documents architecture - no collections to create");
  
  return status;
}

/* Check if RBAC exists in database */
int rbac_db_exists(database_t* db) {
  if (!db) {
    return 0;
  }
  
  /* UNIFIED DOCUMENTS: Check if any users exist in the unified storage
   * We don't check for "collections" because they're virtual.
   * The presence of at least one user indicates RBAC is initialized.
   */
  json_value_t* query = rbac_build_user_query(RBAC_LIBRARY_SYSTEM);
  json_value_t* result = virtual_query(db, DOC_TYPE_NAME_USER, RBAC_LIBRARY_SYSTEM, VIRTUAL_COLLECTION_USERS, query);
  json_free(query);
  
  /* Extract documents array from response object */
  json_value_t* documents = json_object_get(result, "documents");
  int has_users = 0;
  if (documents && documents->type == JSON_ARRAY && documents->value.array.size > 0) {
    has_users = 1;
  }
  
  json_free(result);
  return has_users;
}

/* Convert user document to rbac_user_t */
static rbac_user_t* user_doc_to_rbac_user(json_value_t* user_doc) {
  if (!user_doc || user_doc->type != JSON_OBJECT) {
    LOG_ERROR("Invalid user document.");
    return NULL;
  }
  
  /* Get user fields - try both "id" and "uuid" */
  json_value_t* id_val = json_object_get(user_doc, "uuid");
  if (!id_val || id_val->type != JSON_STRING) {
    id_val = json_object_get(user_doc, "id");
  }
  
  json_value_t* username_val = json_object_get(user_doc, "username");
  json_value_t* password_hash_val = json_object_get(user_doc, "password_hash");
  json_value_t* roles_val = json_object_get(user_doc, "roles");
  
  if (!id_val || id_val->type != JSON_STRING ||
    !username_val || username_val->type != JSON_STRING ||
    !password_hash_val || password_hash_val->type != JSON_STRING ||
    !roles_val || roles_val->type != JSON_ARRAY) {
    LOG_ERROR("Missing or invalid user fields - id=%p, username=%p, password_hash=%p, roles=%p",
         id_val, username_val, password_hash_val, roles_val);
    return NULL;
  }
  
  /* Create user structure */
  rbac_user_t* user = (rbac_user_t*)BUFFER_ALLOC(sizeof(rbac_user_t));
  if (!user) {
    LOG_ERROR("Cannot allocate memory for user.");
    return NULL;
  }
  
  /* Set user fields */
  user->id = BUFFER_STRDUP(id_val->value.string);
  user->username = BUFFER_STRDUP(username_val->value.string);
  user->password_hash = BUFFER_STRDUP(password_hash_val->value.string);
  
  /* Copy roles array */
  user->roles = json_create_array();
  for (size_t i = 0; i < roles_val->value.array.size; i++) {
    json_value_t* role_id = roles_val->value.array.items[i];
    if (role_id->type == JSON_STRING) {
      json_array_append(user->roles, json_create_string(role_id->value.string));
    }
  }
  
  TRACE_RBAC("RBAC_DB: Successfully created rbac_user_t - id: %s, username: %s, roles: %zu",
       user->id, user->username, user->roles->value.array.size);
  
  return user;
}

/* Convert role document to rbac_role_t */
static rbac_role_t* role_doc_to_rbac_role(json_value_t* role_doc) {
  if (!role_doc || role_doc->type != JSON_OBJECT) {
    return NULL;
  }
  
  /* Get role fields */
  json_value_t* doc_id_val = json_object_get(role_doc, "uuid"); /* Document ID */
  json_value_t* id_val = json_object_get(role_doc, "id");    /* UUID */
  json_value_t* name_val = json_object_get(role_doc, "name");
  json_value_t* permissions_val = json_object_get(role_doc, "permissions");
  
  if (!name_val || name_val->type != JSON_STRING ||
    !permissions_val || permissions_val->type != JSON_OBJECT) {
    return NULL;
  }
  
  /* Create role structure */
  rbac_role_t* role = (rbac_role_t*)BUFFER_ALLOC(sizeof(rbac_role_t));
  if (!role) {
    return NULL;
  }
  
  /* Set role fields - use document ID as primary ID for database operations */
  if (doc_id_val && doc_id_val->type == JSON_STRING) {
    role->id = BUFFER_STRDUP(doc_id_val->value.string);
  } else if (id_val && id_val->type == JSON_STRING) {
    /* Fallback to UUID if no document ID */
    role->id = BUFFER_STRDUP(id_val->value.string);
  } else {
    BUFFER_FREE(role);
    return NULL;
  }
  
  role->name = BUFFER_STRDUP(name_val->value.string);
  
  /* Deep copy permissions */
  char* permissions_str = json_stringify(permissions_val);
  role->permissions = json_parse(permissions_str);
  BUFFER_FREE(permissions_str);
  
  return role;
}

/* NOTE: Previous unused conversion functions removed to fix compiler warnings.
  The code directly uses json_clone() for document operations instead. */

/* Load RBAC system from database */
rbac_system_t* rbac_db_load(database_t* db) {
  if (!db) {
    return NULL;
  }
  
  /* Check if RBAC collections exist */
  if (!rbac_db_exists(db)) {
    LOG_WARNING("RBAC collections not found in database.");
    return NULL;
  }
  
  /* Create RBAC system */
  rbac_system_t* rbac = (rbac_system_t*)BUFFER_ALLOC(sizeof(rbac_system_t));
  if (!rbac) {
    return NULL;
  }
  
  /* Initialize RBAC structure */
  memset(rbac, 0, sizeof(rbac_system_t));
  
  /* Set database pointer - this is the single source of truth */
  rbac->db = db;
  
  /* SINGLE SOURCE OF TRUTH: Database handles all user/role storage */
  /* No in-memory structures to populate - all data stays in database */
  
  return rbac;
}

/* Save RBAC system to database */
int rbac_db_save(database_t* db, rbac_system_t* rbac) {
  if (!db) {
    return 0;
  }
  
  /* Note: rbac parameter is kept for compatibility but not used since
   * we use database as single source of truth */
  (void)rbac;
  
  /* Initialize RBAC collections if they don't exist */
  rbac_db_status_t status = rbac_db_init_collections(db);
  if (!status.success) {
    LOG_ERROR("Failed to initialize RBAC collections: %s", status.error_message);
    if (status.error_message) BUFFER_FREE(status.error_message);
    return 0;
  }
  
  /* With database as single source of truth, there's nothing to "save" from memory.
   * All RBAC data is already persisted in the database as it's created/modified.
   * This function now just ensures the collections exist. */
  
  LOG_INFO("RBAC collections verified. Database is the single source of truth.");
  
  return 1;
}

/* Create a user in the database */
rbac_user_t* rbac_db_create_user(database_t* db, const char* username, const char* password) {
  if (!db || !username || !password) {
    return NULL;
  }
  
  /* Check if username already exists */
  json_value_t* query = json_create_object();
  json_object_set(query, "username", json_create_string(username));
  json_value_t* result = virtual_query(db, DOC_TYPE_NAME_USER, RBAC_LIBRARY_SYSTEM, VIRTUAL_COLLECTION_USERS, query);
  json_free(query);
  
  /* Extract documents array from response object */
  json_value_t* existing_users = json_object_get(result, "documents");
  if (existing_users && existing_users->type == JSON_ARRAY && existing_users->value.array.size > 0) {
    LOG_ERROR("Username %s already exists", username);
    json_free(result);
    return NULL;
  }
  
  if (result) json_free(result);
  
  /* Create user ID */
  char* id = generate_uuid();
  if (!id) {
    return NULL;
  }
  
  /* Hash password */
  char* password_hash = hash_password(password);
  if (!password_hash) {
    BUFFER_FREE(id);
    return NULL;
  }
  
  /* Create user document */
  json_value_t* user_doc = json_create_object();
  if (!user_doc) {
    BUFFER_FREE(id);
    BUFFER_FREE(password_hash);
    return NULL;
  }
  
  /* Set user properties */
  /* Don't set id - let db_insert_document generate uuid */
  json_object_set(user_doc, "username", json_create_string(username));
  json_object_set(user_doc, "password_hash", json_create_string(password_hash));
  json_object_set(user_doc, "roles", json_create_array());
  
  /* Insert user document using virtual layer */
  json_value_t* insert_result = virtual_insert(db, DOC_TYPE_NAME_USER, RBAC_LIBRARY_SYSTEM, VIRTUAL_COLLECTION_USERS, user_doc, "system-admin");
  if (!insert_result) {
    LOG_ERROR("insert user document into database.");
    BUFFER_FREE(id);
    BUFFER_FREE(password_hash);
    return NULL;
  }
  
  json_free(insert_result);
  
  /* Create user structure */
  rbac_user_t* user = (rbac_user_t*)BUFFER_ALLOC(sizeof(rbac_user_t));
  if (!user) {
    BUFFER_FREE(id);
    BUFFER_FREE(password_hash);
    return NULL;
  }
  
  /* Set user fields */
  user->id = id;
  user->username = BUFFER_STRDUP(username);
  user->password_hash = password_hash;
  user->roles = json_create_array();
  
  return user;
}

/* Delete a user from the database */
int rbac_db_delete_user(database_t* db, const char* user_id) {
  if (!db || !user_id) {
    return 0;
  }
  
  /* Get user document using virtual layer */
  json_value_t* user_query = json_create_object();
  json_object_set(user_query, "uuid", json_create_string(user_id));
  json_value_t* user_results = virtual_query(db, DOC_TYPE_NAME_USER, RBAC_LIBRARY_SYSTEM, VIRTUAL_COLLECTION_USERS, user_query);
  json_free(user_query);
  
  json_value_t* user_doc = NULL;
  if (user_results) {
    json_value_t* documents = json_object_get(user_results, "documents");
    if (documents && documents->type == JSON_ARRAY && documents->value.array.size > 0) {
      user_doc = json_clone(json_array_get(documents, 0));
    }
    json_free(user_results);
  }
  char* actual_doc_id = NULL;
  
  /* If not found, try as UUID */
  if (!user_doc) {
    user_doc = find_user_by_uuid(db, user_id);
    if (!user_doc) {
      LOG_ERROR("User %s not found", user_id);
      return 0;
    }
    
    /* Get the actual document ID */
    json_value_t* doc_id_val = json_object_get(user_doc, "uuid");
    if (doc_id_val && doc_id_val->type == JSON_STRING) {
      actual_doc_id = BUFFER_STRDUP(doc_id_val->value.string);
    }
  } else {
    actual_doc_id = BUFFER_STRDUP(user_id);
  }
  
  /* Get user roles */
  json_value_t* roles_val = json_object_get(user_doc, "roles");
  if (roles_val && roles_val->type == JSON_ARRAY) {
    /* Remove user from all roles */
    for (size_t i = 0; i < roles_val->value.array.size; i++) {
      json_value_t* role_id_val = roles_val->value.array.items[i];
      if (role_id_val->type == JSON_STRING) {
        const char* role_id = role_id_val->value.string;
        
        /* Get role document */
        /* Get role by UUID - need to query since virtual expects name not UUID */
  json_value_t* query = rbac_build_role_query(RBAC_LIBRARY_SYSTEM);
  json_object_set(query, "uuid", json_create_string(role_id));
  json_value_t* result = virtual_query(db, DOC_TYPE_NAME_ROLE, RBAC_LIBRARY_SYSTEM, VIRTUAL_COLLECTION_ROLES, query);
  json_free(query);
  
  json_value_t* role_doc = NULL;
  if (result) {
    json_value_t* docs = json_object_get(result, "documents");
    if (docs && docs->type == JSON_ARRAY && docs->value.array.size > 0) {
      role_doc = json_clone(docs->value.array.items[0]);
    }
    json_free(result);
  }
        if (role_doc && role_doc->type == JSON_OBJECT) {
          /* Get role users */
          json_value_t* users_val = json_object_get(role_doc, "users");
          if (users_val && users_val->type == JSON_ARRAY) {
            /* Remove user from role users */
            json_value_t* updated_users = json_create_array();
            for (size_t j = 0; j < users_val->value.array.size; j++) {
              json_value_t* id_val = users_val->value.array.items[j];
              if (id_val->type == JSON_STRING && strcmp(id_val->value.string, user_id) != 0) {
                json_array_append(updated_users, json_create_string(id_val->value.string));
              }
            }
            
            /* Update role document */
            json_object_set(role_doc, "users", updated_users);
            json_value_t* update_result = virtual_update(db, role_id, role_doc);
            if (update_result) {
              json_free(update_result);
            }
          }
          
          json_free(role_doc);
        }
      }
    }
  }
  
  json_free(user_doc);
  
  /* Delete user document using actual document ID */
  if (!actual_doc_id) {
    LOG_ERROR("get document ID for user %s", user_id);
    return 0;
  }
  
  int result = virtual_delete(db, actual_doc_id);
  BUFFER_FREE(actual_doc_id);
  
  return result;
}

/* Get a user from the database by ID */
rbac_user_t* rbac_db_get_user(database_t* db, const char* user_id) {
  if (!db || !user_id) {
    return NULL;
  }
  
  /* Get user document using virtual layer */
  json_value_t* user_query = json_create_object();
  json_object_set(user_query, "uuid", json_create_string(user_id));
  json_value_t* user_results = virtual_query(db, DOC_TYPE_NAME_USER, RBAC_LIBRARY_SYSTEM, VIRTUAL_COLLECTION_USERS, user_query);
  json_free(user_query);
  
  json_value_t* user_doc = NULL;
  if (user_results) {
    json_value_t* documents = json_object_get(user_results, "documents");
    if (documents && documents->type == JSON_ARRAY && documents->value.array.size > 0) {
      user_doc = json_clone(json_array_get(documents, 0));
    }
    json_free(user_results);
  }
  
  /* If not found, try as UUID */
  if (!user_doc) {
    user_doc = find_user_by_uuid(db, user_id);
    if (!user_doc) {
      return NULL;
    }
  }
  
  /* Convert to rbac_user_t */
  rbac_user_t* user = user_doc_to_rbac_user(user_doc);
  json_free(user_doc);
  
  return user;
}

/* Get a user from the database by username */
rbac_user_t* rbac_db_get_user_by_username(database_t* db, const char* username) {
  TRACE_RBAC("RBAC_DB: rbac_db_get_user_by_username called with username: %s", username);
  
  if (!db || !username) {
    LOG_ERROR("Invalid parameters - db=%p, username=%s", db, username ? username : "NULL");
    return NULL;
  }
  
  /* Create query - UNIFIED DOCUMENTS: Include type filtering */
  json_value_t* query = rbac_build_user_query(RBAC_LIBRARY_SYSTEM);
  json_object_set(query, "username", json_create_string(username));
  
  /* Query user document */
  json_value_t* result = virtual_query(db, DOC_TYPE_NAME_USER, RBAC_LIBRARY_SYSTEM, VIRTUAL_COLLECTION_USERS, query);
  json_free(query);
  
  if (!result || result->type != JSON_OBJECT) {
    LOG_ERROR("Query failed or invalid result type.");
    if (result) json_free(result);
    return NULL;
  }
  
  /* Get documents array from result */
  json_value_t* documents = json_object_get(result, "documents");
  if (!documents || documents->type != JSON_ARRAY || documents->value.array.size == 0) {
    TRACE_RBAC("RBAC_DB: No users found matching username: %s", username);
    json_free(result);
    return NULL;
  }
  
  TRACE_RBAC("RBAC_DB: Found %zu users matching username: %s", documents->value.array.size, username);
  
  /* Get first matching user */
  json_value_t* user_doc = documents->value.array.items[0];
  
  /* Convert to rbac_user_t */
  rbac_user_t* user = user_doc_to_rbac_user(user_doc);
  if (user) {
    TRACE_RBAC("RBAC_DB: Successfully converted user document to rbac_user_t - id: %s", user->id);
  } else {
    LOG_ERROR("Cannot convert user document to rbac_user_t.");
  }
  
  json_free(result);
  
  return user;
}

/* Authenticate a user in the database */
int rbac_db_authenticate_user(database_t* db, const char* username, const char* password) {
  if (!db || !username || !password) {
    return 0;
  }
  
  /* Get user by username */
  rbac_user_t* user = rbac_db_get_user_by_username(db, username);
  if (!user) {
    return 0;
  }
  
  /* Verify password against hash */
  int result = rbac_authenticate_user(NULL, username, password);
  
  /* Free user */
  rbac_free_user(user);
  
  return result;
}

/* Create a role in the database */
rbac_role_t* rbac_db_create_role(database_t* db, const char* name) {
  if (!db || !name) {
    return NULL;
  }
  
  /* Check if role with same name already exists */
  json_value_t* query = rbac_build_role_query(RBAC_LIBRARY_SYSTEM);
  json_object_set(query, "name", json_create_string(name));
  json_value_t* result = virtual_query(db, DOC_TYPE_NAME_ROLE, RBAC_LIBRARY_SYSTEM, VIRTUAL_COLLECTION_ROLES, query);
  json_free(query);
  
  /* Extract documents array from response object */
  json_value_t* existing_roles = json_object_get(result, "documents");
  if (existing_roles && existing_roles->type == JSON_ARRAY && existing_roles->value.array.size > 0) {
    LOG_ERROR("Role with name %s already exists", name);
    json_free(result);
    return NULL;
  }
  
  if (result) json_free(result);
  
  /* Create role ID */
  char* id = generate_uuid();
  if (!id) {
    return NULL;
  }
  
  /* Create role document */
  json_value_t* role_doc = json_create_object();
  if (!role_doc) {
    BUFFER_FREE(id);
    return NULL;
  }
  
  /* Set role properties */
  /* Don't set id - let db_insert_document generate uuid */
  json_object_set(role_doc, "name", json_create_string(name));
  json_object_set(role_doc, "permissions", json_create_object());
  json_object_set(role_doc, "users", json_create_array());
  
  /* Insert role document */
  json_value_t* insert_result = virtual_insert(db, DOC_TYPE_NAME_ROLE, RBAC_LIBRARY_SYSTEM, VIRTUAL_COLLECTION_ROLES, role_doc, "system-admin");
  if (!insert_result) {
    LOG_ERROR("insert role document into database.");
    BUFFER_FREE(id);
    return NULL;
  }
  
  json_free(insert_result);
  
  /* Create role structure */
  rbac_role_t* role = (rbac_role_t*)BUFFER_ALLOC(sizeof(rbac_role_t));
  if (!role) {
    BUFFER_FREE(id);
    return NULL;
  }
  
  /* Set role fields */
  role->id = id;
  role->name = BUFFER_STRDUP(name);
  role->permissions = json_create_object();
  
  return role;
}

/* Helper function to find role by UUID */
static json_value_t* find_role_by_uuid(database_t* db, const char* uuid) {
  if (!db || !uuid) {
    return NULL;
  }
  
  /* Query for role with matching UUID - UNIFIED DOCUMENTS */
  json_value_t* query = rbac_build_role_query(RBAC_LIBRARY_SYSTEM);
  json_object_set(query, "uuid", json_create_string(uuid));
  
  json_value_t* result = virtual_query(db, DOC_TYPE_NAME_ROLE, RBAC_LIBRARY_SYSTEM, VIRTUAL_COLLECTION_ROLES, query);
  json_free(query);
  
  if (!result || result->type != JSON_OBJECT) {
    if (result) json_free(result);
    return NULL;
  }
  
  json_value_t* documents = json_object_get(result, "documents");
  if (!documents || documents->type != JSON_ARRAY || documents->value.array.size == 0) {
    json_free(result);
    return NULL;
  }
  
  /* Clone the first matching document */
  json_value_t* role_doc = json_clone(documents->value.array.items[0]);
  json_free(result);
  
  return role_doc;
}

/* Helper function to find user by UUID */
static json_value_t* find_user_by_uuid(database_t* db, const char* uuid) {
  if (!db || !uuid) {
    return NULL;
  }
  
  /* Query for user with matching UUID - UNIFIED DOCUMENTS */
  json_value_t* query = rbac_build_user_query(RBAC_LIBRARY_SYSTEM);
  json_object_set(query, "uuid", json_create_string(uuid));
  
  json_value_t* result = virtual_query(db, DOC_TYPE_NAME_USER, RBAC_LIBRARY_SYSTEM, VIRTUAL_COLLECTION_USERS, query);
  json_free(query);
  
  if (!result || result->type != JSON_OBJECT) {
    if (result) json_free(result);
    return NULL;
  }
  
  json_value_t* documents = json_object_get(result, "documents");
  if (!documents || documents->type != JSON_ARRAY || documents->value.array.size == 0) {
    json_free(result);
    return NULL;
  }
  
  /* Clone the first matching document */
  json_value_t* user_doc = json_clone(documents->value.array.items[0]);
  json_free(result);
  
  return user_doc;
}

/* Delete a role from the database */
int rbac_db_delete_role(database_t* db, const char* role_id) {
  if (!db || !role_id) {
    return 0;
  }
  
  /* Delete role using virtual layer */
  return virtual_delete(db, role_id);
}

/* Update a role in the database */
int rbac_db_update_role(database_t* db, const char* role_id, const char* name, json_value_t* permissions) {
  if (!db || !role_id) {
    return 0;
  }
  
  /* Get existing role document - first try as document ID */
  /* Get role by UUID - need to query since virtual expects name not UUID */
  json_value_t* query = rbac_build_role_query(RBAC_LIBRARY_SYSTEM);
  json_object_set(query, "uuid", json_create_string(role_id));
  json_value_t* result = virtual_query(db, DOC_TYPE_NAME_ROLE, RBAC_LIBRARY_SYSTEM, VIRTUAL_COLLECTION_ROLES, query);
  json_free(query);
  
  json_value_t* role_doc = NULL;
  if (result) {
    json_value_t* docs = json_object_get(result, "documents");
    if (docs && docs->type == JSON_ARRAY && docs->value.array.size > 0) {
      role_doc = json_clone(docs->value.array.items[0]);
    }
    json_free(result);
  }
  char* actual_doc_id = NULL;
  
  /* If not found, try as UUID */
  if (!role_doc) {
    role_doc = find_role_by_uuid(db, role_id);
    if (!role_doc) {
      LOG_ERROR("Role %s not found", role_id);
      return 0;
    }
    
    /* Get the actual document ID */
    json_value_t* doc_id_val = json_object_get(role_doc, "uuid");
    if (doc_id_val && doc_id_val->type == JSON_STRING) {
      actual_doc_id = BUFFER_STRDUP(doc_id_val->value.string);
    }
  } else {
    actual_doc_id = BUFFER_STRDUP(role_id);
  }
  
  if (!actual_doc_id) {
    json_free(role_doc);
    LOG_ERROR("get document ID for role %s", role_id);
    return 0;
  }
  
  /* Update name if provided */
  if (name) {
    json_object_set(role_doc, "name", json_create_string(name));
  }
  
  /* Update permissions if provided */
  if (permissions) {
    json_object_set(role_doc, "permissions", json_clone(permissions));
  }
  
  /* Update the document in database */
  json_value_t* update_result = virtual_update(db, actual_doc_id, role_doc);
  
  json_free(role_doc);
  BUFFER_FREE(actual_doc_id);
  
  if (!update_result) {
    LOG_ERROR("update role document in database.");
    return 0;
  }
  
  json_free(update_result);
  return 1;
}

/* Get a role from the database by ID */
rbac_role_t* rbac_db_get_role(database_t* db, const char* role_id) {
  if (!db || !role_id) {
    return NULL;
  }
  
  /* First try as document ID */
  /* Get role by UUID - need to query since virtual expects name not UUID */
  json_value_t* query = rbac_build_role_query(RBAC_LIBRARY_SYSTEM);
  json_object_set(query, "uuid", json_create_string(role_id));
  json_value_t* result = virtual_query(db, DOC_TYPE_NAME_ROLE, RBAC_LIBRARY_SYSTEM, VIRTUAL_COLLECTION_ROLES, query);
  json_free(query);
  
  json_value_t* role_doc = NULL;
  if (result) {
    json_value_t* docs = json_object_get(result, "documents");
    if (docs && docs->type == JSON_ARRAY && docs->value.array.size > 0) {
      role_doc = json_clone(docs->value.array.items[0]);
    }
    json_free(result);
  }
  
  /* If not found, try as UUID */
  if (!role_doc) {
    role_doc = find_role_by_uuid(db, role_id);
    if (!role_doc) {
      return NULL;
    }
  }
  
  /* Convert to rbac_role_t */
  rbac_role_t* role = role_doc_to_rbac_role(role_doc);
  json_free(role_doc);
  
  return role;
}

/* Add a user to a role in the database */
int rbac_db_add_user_to_role(database_t* db, const char* user_id, const char* role_id) {
  if (!db || !user_id || !role_id) {
    return 0;
  }
  
  /* Get user - supports both document ID and UUID */
  rbac_user_t* user = rbac_db_get_user(db, user_id);
  if (!user) {
    LOG_ERROR("User %s not found", user_id);
    return 0;
  }
  char* actual_user_id = BUFFER_STRDUP(user->id);
  
  /* Get role - supports both document ID and UUID */
  rbac_role_t* role = rbac_db_get_role(db, role_id);
  if (!role) {
    LOG_ERROR("Role %s not found", role_id);
    BUFFER_FREE(actual_user_id);
    rbac_free_user(user);
    return 0;
  }
  char* actual_role_id = BUFFER_STRDUP(role->id);
  
  rbac_free_user(user);
  rbac_free_role(role);
  
  /* Get user document using actual ID */
  json_value_t* user_doc = virtual_get_user_by_uuid(db, actual_user_id);
  if (!user_doc) {
    LOG_ERROR("User document %s not found", actual_user_id);
    BUFFER_FREE(actual_user_id);
    BUFFER_FREE(actual_role_id);
    return 0;
  }
  
  /* Get role document using actual ID */
  json_value_t* role_doc = virtual_get_role_by_uuid(db, actual_role_id);
  if (!role_doc) {
    LOG_ERROR("Role document %s not found", actual_role_id);
    json_free(user_doc);
    BUFFER_FREE(actual_user_id);
    BUFFER_FREE(actual_role_id);
    return 0;
  }
  
  /* Get user roles */
  json_value_t* user_roles = json_object_get(user_doc, "roles");
  if (!user_roles || user_roles->type != JSON_ARRAY) {
    json_free(user_doc);
    json_free(role_doc);
    BUFFER_FREE(actual_user_id);
    BUFFER_FREE(actual_role_id);
    return 0;
  }
  
  /* Get role users */
  json_value_t* role_users = json_object_get(role_doc, "users");
  if (!role_users || role_users->type != JSON_ARRAY) {
    json_free(user_doc);
    json_free(role_doc);
    BUFFER_FREE(actual_user_id);
    BUFFER_FREE(actual_role_id);
    return 0;
  }
  
  /* Check if user already in role */
  int user_has_role = 0;
  for (size_t i = 0; i < user_roles->value.array.size; i++) {
    json_value_t* id = user_roles->value.array.items[i];
    if (id->type == JSON_STRING && strcmp(id->value.string, actual_role_id) == 0) {
      user_has_role = 1;
      break;
    }
  }
  
  /* Check if role already has user */
  int role_has_user = 0;
  for (size_t i = 0; i < role_users->value.array.size; i++) {
    json_value_t* id = role_users->value.array.items[i];
    if (id->type == JSON_STRING && strcmp(id->value.string, actual_user_id) == 0) {
      role_has_user = 1;
      break;
    }
  }
  
  /* Add role to user roles if needed */
  if (!user_has_role) {
    json_array_append(user_roles, json_create_string(actual_role_id));
    json_value_t* update_result = virtual_update(db, actual_user_id, user_doc);
    if (update_result) {
      json_free(update_result);
    } else {
      LOG_ERROR("update user document.");
      json_free(user_doc);
      json_free(role_doc);
      BUFFER_FREE(actual_user_id);
      BUFFER_FREE(actual_role_id);
      return 0;
    }
  }
  
  /* Add user to role users if needed */
  if (!role_has_user) {
    json_array_append(role_users, json_create_string(actual_user_id));
    json_value_t* update_result = virtual_update(db, actual_role_id, role_doc);
    if (update_result) {
      json_free(update_result);
    } else {
      LOG_ERROR("update role document.");
      json_free(user_doc);
      json_free(role_doc);
      BUFFER_FREE(actual_user_id);
      BUFFER_FREE(actual_role_id);
      return 0;
    }
  }
  
  json_free(user_doc);
  json_free(role_doc);
  BUFFER_FREE(actual_user_id);
  BUFFER_FREE(actual_role_id);
  
  return 1;
}

/* Remove a user from a role in the database */
int rbac_db_remove_user_from_role(database_t* db, const char* user_id, const char* role_id) {
  if (!db || !user_id || !role_id) {
    return 0;
  }
  
  /* Get user - supports both document ID and UUID */
  rbac_user_t* user = rbac_db_get_user(db, user_id);
  if (!user) {
    LOG_ERROR("User %s not found", user_id);
    return 0;
  }
  char* actual_user_id = BUFFER_STRDUP(user->id);
  
  /* Get role - supports both document ID and UUID */
  rbac_role_t* role = rbac_db_get_role(db, role_id);
  if (!role) {
    LOG_ERROR("Role %s not found", role_id);
    BUFFER_FREE(actual_user_id);
    rbac_free_user(user);
    return 0;
  }
  char* actual_role_id = BUFFER_STRDUP(role->id);
  
  rbac_free_user(user);
  rbac_free_role(role);
  
  /* Get user document using actual ID */
  json_value_t* user_doc = virtual_get_user_by_uuid(db, actual_user_id);
  if (!user_doc) {
    LOG_ERROR("User document %s not found", actual_user_id);
    BUFFER_FREE(actual_user_id);
    BUFFER_FREE(actual_role_id);
    return 0;
  }
  
  /* Get role document using actual ID */
  json_value_t* role_doc = virtual_get_role_by_uuid(db, actual_role_id);
  if (!role_doc) {
    LOG_ERROR("Role document %s not found", actual_role_id);
    json_free(user_doc);
    BUFFER_FREE(actual_user_id);
    BUFFER_FREE(actual_role_id);
    return 0;
  }
  
  /* Get user roles */
  json_value_t* user_roles = json_object_get(user_doc, "roles");
  if (!user_roles || user_roles->type != JSON_ARRAY) {
    json_free(user_doc);
    json_free(role_doc);
    BUFFER_FREE(actual_user_id);
    BUFFER_FREE(actual_role_id);
    return 0;
  }
  
  /* Get role users */
  json_value_t* role_users = json_object_get(role_doc, "users");
  if (!role_users || role_users->type != JSON_ARRAY) {
    json_free(user_doc);
    json_free(role_doc);
    BUFFER_FREE(actual_user_id);
    BUFFER_FREE(actual_role_id);
    return 0;
  }
  
  /* Remove role from user roles */
  int user_role_found = 0;
  json_value_t* updated_roles = json_create_array();
  for (size_t i = 0; i < user_roles->value.array.size; i++) {
    json_value_t* id = user_roles->value.array.items[i];
    if (id->type == JSON_STRING && strcmp(id->value.string, actual_role_id) != 0) {
      json_array_append(updated_roles, json_create_string(id->value.string));
    } else {
      user_role_found = 1;
    }
  }
  
  /* Update user document if role was found */
  if (user_role_found) {
    json_object_set(user_doc, "roles", updated_roles);
    json_value_t* update_result = db_update_document(db, STORAGE_LIBRARY, STORAGE_COLLECTION, actual_user_id, user_doc);
    if (update_result) {
      json_free(update_result);
    } else {
      LOG_ERROR("update user document.");
      json_free(user_doc);
      json_free(role_doc);
      return 0;
    }
  } else {
    json_free(updated_roles);
  }
  
  /* Remove user from role users */
  int role_user_found = 0;
  json_value_t* updated_users = json_create_array();
  for (size_t i = 0; i < role_users->value.array.size; i++) {
    json_value_t* id = role_users->value.array.items[i];
    if (id->type == JSON_STRING && strcmp(id->value.string, actual_user_id) != 0) {
      json_array_append(updated_users, json_create_string(id->value.string));
    } else {
      role_user_found = 1;
    }
  }
  
  /* Update role document if user was found */
  if (role_user_found) {
    json_object_set(role_doc, "users", updated_users);
    json_value_t* update_result = db_update_document(db, STORAGE_LIBRARY, STORAGE_COLLECTION, actual_role_id, role_doc);
    if (update_result) {
      json_free(update_result);
    } else {
      LOG_ERROR("update role document.");
      json_free(user_doc);
      json_free(role_doc);
      return 0;
    }
  } else {
    json_free(updated_users);
  }
  
  json_free(user_doc);
  json_free(role_doc);
  BUFFER_FREE(actual_user_id);
  BUFFER_FREE(actual_role_id);
  
  return user_role_found || role_user_found;
}

/* Get resource permission key */
static char* get_resource_permission_key(rbac_resource_type_t resource_type, const char* resource_id) {
  if (!resource_id) {
    return NULL;
  }
  
  /* Create key string */
  char* key = (char*)BUFFER_ALLOC(strlen(resource_id) + 32);
  if (!key) {
    return NULL;
  }
  
  /* Format key as "type:id" */
  sprintf(key, "%d:%s", resource_type, resource_id);
  
  return key;
}

/* Grant a permission to a role in the database */
int rbac_db_grant_permission(database_t* db, const char* role_id, rbac_resource_type_t resource_type,
              const char* resource_id, rbac_permission_t permission) {
  if (!db || !role_id || !resource_id) {
    return 0;
  }
  
  /* Get role document */
  /* Get role by UUID - need to query since virtual expects name not UUID */
  json_value_t* query = rbac_build_role_query(RBAC_LIBRARY_SYSTEM);
  json_object_set(query, "uuid", json_create_string(role_id));
  json_value_t* result = virtual_query(db, DOC_TYPE_NAME_ROLE, RBAC_LIBRARY_SYSTEM, VIRTUAL_COLLECTION_ROLES, query);
  json_free(query);
  
  json_value_t* role_doc = NULL;
  if (result) {
    json_value_t* docs = json_object_get(result, "documents");
    if (docs && docs->type == JSON_ARRAY && docs->value.array.size > 0) {
      role_doc = json_clone(docs->value.array.items[0]);
    }
    json_free(result);
  }
  if (!role_doc) {
    LOG_ERROR("Role %s not found", role_id);
    return 0;
  }
  
  /* Get role permissions */
  json_value_t* permissions = json_object_get(role_doc, "permissions");
  if (!permissions || permissions->type != JSON_OBJECT) {
    json_free(role_doc);
    return 0;
  }
  
  /* Create resource permission key */
  char* key = get_resource_permission_key(resource_type, resource_id);
  if (!key) {
    json_free(role_doc);
    return 0;
  }
  
  /* Get current permission value */
  int current_permission = 0;
  json_value_t* current = json_object_get(permissions, key);
  if (current && current->type == JSON_NUMBER) {
    current_permission = (int)current->value.number;
  }
  
  /* Add permission */
  current_permission |= permission;
  
  /* Update permission */
  json_object_set(permissions, key, json_create_number(current_permission));
  
  /* Update role document */
  json_value_t* update_result = db_update_document(db, STORAGE_LIBRARY, STORAGE_COLLECTION, role_id, role_doc);
  if (!update_result) {
    LOG_ERROR("update role document.");
    BUFFER_FREE(key);
    json_free(role_doc);
    return 0;
  }
  
  json_free(update_result);
  BUFFER_FREE(key);
  json_free(role_doc);
  
  return 1;
}

/* Revoke a permission from a role in the database */
int rbac_db_revoke_permission(database_t* db, const char* role_id, rbac_resource_type_t resource_type,
              const char* resource_id, rbac_permission_t permission) {
  if (!db || !role_id || !resource_id) {
    return 0;
  }
  
  /* Get role document using virtual layer */
  json_value_t* role_doc = virtual_get_role_by_uuid(db, role_id);
  if (!role_doc) {
    LOG_ERROR("Role %s not found", role_id);
    return 0;
  }
  
  /* Get role permissions */
  json_value_t* permissions = json_object_get(role_doc, "permissions");
  if (!permissions || permissions->type != JSON_OBJECT) {
    json_free(role_doc);
    return 0;
  }
  
  /* Create resource permission key */
  char* key = get_resource_permission_key(resource_type, resource_id);
  if (!key) {
    json_free(role_doc);
    return 0;
  }
  
  /* Get current permission value */
  int current_permission = 0;
  json_value_t* current = json_object_get(permissions, key);
  if (current && current->type == JSON_NUMBER) {
    current_permission = (int)current->value.number;
  } else {
    /* No permission set, nothing to revoke */
    BUFFER_FREE(key);
    json_free(role_doc);
    return 1;
  }
  
  /* Remove permission */
  current_permission &= ~permission;
  
  /* Update permission */
  if (current_permission == 0) {
    json_object_remove(permissions, key);
  } else {
    json_object_set(permissions, key, json_create_number(current_permission));
  }
  
  /* Update role document */
  json_value_t* update_result = db_update_document(db, STORAGE_LIBRARY, STORAGE_COLLECTION, role_id, role_doc);
  if (!update_result) {
    LOG_ERROR("update role document.");
    BUFFER_FREE(key);
    json_free(role_doc);
    return 0;
  }
  
  json_free(update_result);
  BUFFER_FREE(key);
  json_free(role_doc);
  
  return 1;
}

/* Check if a user has a permission for a resource in the database */
int rbac_db_check_permission(database_t* db, const char* user_id, rbac_resource_type_t resource_type,
              const char* resource_id, rbac_permission_t permission) {
  TRACE_RBAC("RBAC_DB: check_permission called - user_id=%s, resource_type=%d, resource_id=%s, permission=%d",
       user_id, resource_type, resource_id, permission);
       
  if (!db || !user_id || !resource_id) {
    LOG_ERROR("Invalid parameters for permission check.");
    return 0;
  }
  
  /* Get user document */
  json_value_t* user_doc = virtual_get_user_by_uuid(db, user_id);
  if (!user_doc) {
    LOG_ERROR("User %s not found", user_id);
    return 0;
  }
  
  TRACE_RBAC("RBAC_DB: User document retrieved for %s", user_id);
  
  /* Get user roles */
  json_value_t* roles = json_object_get(user_doc, "roles");
  if (!roles || roles->type != JSON_ARRAY) {
    LOG_ERROR("User %s has no roles or invalid roles format", user_id);
    json_free(user_doc);
    return 0;
  }
  
  TRACE_RBAC("RBAC_DB: User %s has %zu roles", user_id, roles->value.array.size);
  
  /* Create resource permission key */
  char* key = get_resource_permission_key(resource_type, resource_id);
  if (!key) {
    json_free(user_doc);
    return 0;
  }
  
  /* Create wildcard resource permission key */
  char* wildcard_key = get_resource_permission_key(resource_type, "*");
  if (!wildcard_key) {
    BUFFER_FREE(key);
    json_free(user_doc);
    return 0;
  }
  
  TRACE_RBAC("RBAC_DB: Looking for permission keys: %s or %s", key, wildcard_key);
  
  /* Check permission in each role */
  int has_permission = 0;
  for (size_t i = 0; i < roles->value.array.size; i++) {
    json_value_t* role_id_val = roles->value.array.items[i];
    if (role_id_val->type != JSON_STRING) {
      continue;
    }
    
    const char* role_id = role_id_val->value.string;
    
    /* Get role document using virtual layer */
    json_value_t* role_doc = virtual_get_role_by_uuid(db, role_id);
    if (!role_doc || role_doc->type != JSON_OBJECT) {
      TRACE_RBAC("RBAC_DB: Role %s not found or invalid", role_id);
      continue;
    }
    
    TRACE_RBAC("RBAC_DB: Checking permissions in role %s", role_id);
    
    /* Get role permissions */
    json_value_t* permissions = json_object_get(role_doc, "permissions");
    if (!permissions || permissions->type != JSON_OBJECT) {
      TRACE_RBAC("RBAC_DB: Role %s has no permissions or invalid format", role_id);
      json_free(role_doc);
      continue;
    }
    
    /* Log all permission keys in the role */
    TRACE_RBAC("RBAC_DB: Role %s has %zu permission entries", role_id, permissions->value.object.size);
    for (size_t j = 0; j < permissions->value.object.size; j++) {
      TRACE_RBAC("RBAC_DB: Permission key: %s", permissions->value.object.entries[j].key);
    }
    
    /* Check specific resource permission */
    json_value_t* perm_val = json_object_get(permissions, key);
    if (perm_val && (perm_val->type == JSON_NUMBER || perm_val->type == JSON_INTEGER)) {
      int perm;
      if (perm_val->type == JSON_INTEGER) {
        perm = (int)perm_val->value.integer;
        TRACE_RBAC("RBAC_DB: Found INTEGER permission for key %s: %d (checking for %d)", key, perm, permission);
      } else {
        perm = (int)perm_val->value.number;
        TRACE_RBAC("RBAC_DB: Found NUMBER permission for key %s: %d (checking for %d)", key, perm, permission);
      }
      if ((perm & permission) == permission) {
        TRACE_RBAC("RBAC_DB: Permission granted!");
        has_permission = 1;
        json_free(role_doc);
        break;
      }
    }
    
    /* Check wildcard resource permission */
    perm_val = json_object_get(permissions, wildcard_key);
    TRACE_RBAC("RBAC_DB: Looking up wildcard key %s, result: %p", wildcard_key, perm_val);
    if (perm_val) {
      TRACE_RBAC("RBAC_DB: Wildcard permission value type: %d (JSON_NUMBER=%d)", perm_val->type, JSON_NUMBER);
    }
    if (perm_val && (perm_val->type == JSON_NUMBER || perm_val->type == JSON_INTEGER)) {
      int perm;
      if (perm_val->type == JSON_INTEGER) {
        perm = (int)perm_val->value.integer;
        TRACE_RBAC("RBAC_DB: Found INTEGER permission for wildcard key %s: %d (checking for %d)", wildcard_key, perm, permission);
      } else {
        perm = (int)perm_val->value.number;
        TRACE_RBAC("RBAC_DB: Found NUMBER permission for wildcard key %s: %d (checking for %d)", wildcard_key, perm, permission);
      }
      if ((perm & permission) == permission) {
        TRACE_RBAC("RBAC_DB: Permission granted via wildcard!");
        has_permission = 1;
        json_free(role_doc);
        break;
      }
    } else {
      TRACE_RBAC("RBAC_DB: No permission found for wildcard key %s", wildcard_key);
    }
    
    json_free(role_doc);
  }
  
  BUFFER_FREE(key);
  BUFFER_FREE(wildcard_key);
  json_free(user_doc);
  
  return has_permission;
}

/**
 * DEPRECATED: This function is no longer used as we've removed file-based RBAC fallback
 * It is kept here for API compatibility only, but will be removed in a future release
 */
int rbac_db_migrate_from_file(database_t* db, rbac_system_t* rbac) {
  (void)db;
  (void)rbac;
  LOG_WARNING("RBAC_db_migrate_from_file is deprecated and will be removed in a future release.");
  return 0; /* Always fail - migration is no longer supported */
}