#include "rbac/rbac.h"
#include "rbac/rbac_db.h"
#include "database/database.h"
#include "utils/logger.h"
#include "utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* RBAC collection names */
#define RBAC_USERS_COLLECTION "_users"
#define RBAC_ROLES_COLLECTION "_roles"
#define RBAC_PERMISSIONS_COLLECTION "_permissions"
#define RBAC_COLLECTIONS_COLLECTION "_collections"
#define RBAC_SESSIONS_COLLECTION "_sessions"
#define RBAC_PERMISSION_CACHE_COLLECTION "_permission_cache"

/* Permission cache TTL in seconds (5 minutes) */
#define PERMISSION_CACHE_TTL 300

/* Generate standardized document ID using the same format as regular documents */
static char* generate_document_id(void) {
  char* id = (char*)malloc(64);
  if (!id) return NULL;
  
  time_t now = time(NULL);
  snprintf(id, 64, "doc-%ld-%d", (long)now, rand() % 10000);
  return id;
}

/* Clean up ALL duplicate users and roles (not just admin) */
static void cleanup_all_rbac_duplicates(struct database* db) {
  LOG_INFO("RBAC_DB: Cleaning up all duplicate users and roles");
  
  /* Get all users and group by username */
  json_value_t* empty_query = json_create_object();
  json_value_t* all_users = db_query_documents(db, RBAC_USERS_COLLECTION, empty_query);
  json_free(empty_query);
  
  if (all_users) {
    json_value_t* documents = json_object_get(all_users, "documents");
    if (documents && documents->type == JSON_ARRAY) {
      /* Track usernames we've seen */
      json_value_t* seen_usernames = json_create_object();
      
      for (size_t i = 0; i < json_array_size(documents); i++) {
        json_value_t* doc = json_array_get(documents, i);
        json_value_t* username_val = json_object_get(doc, "username");
        json_value_t* id_val = json_object_get(doc, "_id");
        
        if (username_val && username_val->type == JSON_STRING && 
          id_val && id_val->type == JSON_STRING) {
          const char* username = username_val->value.string;
          const char* doc_id = id_val->value.string;
          
          /* Generate expected ID for this username */
          char* expected_id = strdup(username); /* For comparison with old IDs */
          if (!expected_id) continue;
          
          /* Check if we've seen this username before */
          json_value_t* seen = json_object_get(seen_usernames, username);
          if (seen) {
            /* Duplicate username - delete if not using standardized ID */
            if (strcmp(doc_id, expected_id) != 0) {
              LOG_INFO("RBAC_DB: Deleting duplicate user %s with non-standard ID: %s", 
                  username, doc_id);
              db_delete_document(db, RBAC_USERS_COLLECTION, doc_id);
            }
          } else {
            /* First time seeing this username */
            json_object_set(seen_usernames, username, json_create_boolean(1));
            
            /* If it doesn't have the standardized ID, we'll need to recreate it */
            if (strcmp(doc_id, expected_id) != 0) {
              LOG_INFO("RBAC_DB: User %s has non-standard ID: %s (expected: %s)", 
                  username, doc_id, expected_id);
              /* Note: In a production system, we might migrate the data instead of deleting */
            }
          }
          
          free(expected_id);
        }
      }
      
      json_free(seen_usernames);
    }
    json_free(all_users);
  }
  
  /* Same for roles */
  empty_query = json_create_object();
  json_value_t* all_roles = db_query_documents(db, RBAC_ROLES_COLLECTION, empty_query);
  json_free(empty_query);
  
  if (all_roles) {
    json_value_t* documents = json_object_get(all_roles, "documents");
    if (documents && documents->type == JSON_ARRAY) {
      /* Track role names we've seen */
      json_value_t* seen_roles = json_create_object();
      
      for (size_t i = 0; i < json_array_size(documents); i++) {
        json_value_t* doc = json_array_get(documents, i);
        json_value_t* name_val = json_object_get(doc, "name");
        json_value_t* id_val = json_object_get(doc, "_id");
        
        if (name_val && name_val->type == JSON_STRING && 
          id_val && id_val->type == JSON_STRING) {
          const char* rolename = name_val->value.string;
          const char* doc_id = id_val->value.string;
          
          /* Generate expected ID for this role */
          char* expected_id = strdup(rolename); /* For comparison with old IDs */
          if (!expected_id) continue;
          
          /* Check if we've seen this role before */
          json_value_t* seen = json_object_get(seen_roles, rolename);
          if (seen) {
            /* Duplicate role - delete if not using standardized ID */
            if (strcmp(doc_id, expected_id) != 0) {
              LOG_INFO("RBAC_DB: Deleting duplicate role %s with non-standard ID: %s", 
                  rolename, doc_id);
              db_delete_document(db, RBAC_ROLES_COLLECTION, doc_id);
            }
          } else {
            /* First time seeing this role */
            json_object_set(seen_roles, rolename, json_create_boolean(1));
            
            /* If it doesn't have the standardized ID, log it */
            if (strcmp(doc_id, expected_id) != 0) {
              LOG_INFO("RBAC_DB: Role %s has non-standard ID: %s (expected: %s)", 
                  rolename, doc_id, expected_id);
            }
          }
          
          free(expected_id);
        }
      }
      
      json_free(seen_roles);
    }
    json_free(all_roles);
  }
}

/* Initialize RBAC database collections */
static int init_rbac_collections(struct database* db) {
  LOG_TRACE("RBAC_DB: Initializing RBAC collections");
  
  const char* collections[] = {
    RBAC_USERS_COLLECTION,
    RBAC_ROLES_COLLECTION,
    RBAC_PERMISSIONS_COLLECTION,
    RBAC_COLLECTIONS_COLLECTION,
    RBAC_SESSIONS_COLLECTION,
    RBAC_PERMISSION_CACHE_COLLECTION
  };
  
  for (size_t i = 0; i < sizeof(collections) / sizeof(collections[0]); i++) {
    LOG_TRACE("RBAC_DB: Checking collection %zu: %s", i, collections[i]);
    if (!db_collection_exists(db, collections[i])) {
      LOG_TRACE("RBAC_DB: Creating collection: %s", collections[i]);
      if (!db_create_collection(db, collections[i])) {
        LOG_ERROR("RBAC_DB: Failed to create collection: %s", collections[i]);
        return 0;
      }
      LOG_TRACE("RBAC_DB: Collection created: %s", collections[i]);
    } else {
      LOG_TRACE("RBAC_DB: Collection already exists: %s", collections[i]);
    }
    LOG_TRACE("RBAC_DB: Collection %s processed", collections[i]);
  }
  
  LOG_TRACE("RBAC_DB: All RBAC collections initialized");
  return 1;
}

/* Forward declarations */
/* Removed old ID generation forward declarations - using generate_document_id() now */
static void cleanup_all_rbac_duplicates(struct database* db);
static int create_default_admin_role(struct database* db);
static int create_default_admin_user(struct database* db);

/* Initialize RBAC system with database backend */
rbac_system_t* rbac_database_init(struct database* db, const char* jwt_secret) {
  LOG_TRACE("RBAC_DB: Initializing database-backed RBAC system");
  LOG_TRACE("RBAC_DB: Database pointer: %p", db);
  LOG_TRACE("RBAC_DB: JWT secret: %s", jwt_secret ? "[PROVIDED]" : "[NULL]");
  
  if (!db) {
    LOG_ERROR("RBAC_DB: Database is NULL");
    return NULL;
  }
  
  /* Initialize RBAC collections */
  if (!init_rbac_collections(db)) {
    LOG_ERROR("RBAC_DB: Failed to initialize RBAC collections");
    return NULL;
  }
  
  /* Clean up any duplicate users/roles system-wide */
  cleanup_all_rbac_duplicates(db);
  
  /* Create RBAC system structure */
  rbac_system_t* rbac = (rbac_system_t*)malloc(sizeof(rbac_system_t));
  if (!rbac) {
    LOG_ERROR("RBAC_DB: Failed to allocate memory for RBAC system");
    return NULL;
  }
  
  /* Initialize with empty in-memory structures (we'll use database directly) */
  rbac->users = json_create_object();
  rbac->roles = json_create_object();
  rbac->db = db;
  rbac->jwt_secret = jwt_secret ? strdup(jwt_secret) : strdup("change-this-secret-in-production");
  
  if (!rbac->users || !rbac->roles || !rbac->jwt_secret) {
    LOG_ERROR("RBAC_DB: Failed to initialize RBAC fields");
    if (rbac->users) json_free(rbac->users);
    if (rbac->roles) json_free(rbac->roles);
    if (rbac->jwt_secret) free(rbac->jwt_secret);
    free(rbac);
    return NULL;
  }
  
  /* Create default admin role if it doesn't exist */
  if (!create_default_admin_role(db)) {
    LOG_ERROR("RBAC_DB: Failed to create default admin role");
    json_free(rbac->users);
    json_free(rbac->roles);
    free(rbac);
    return NULL;
  }
  
  /* Create default admin user if it doesn't exist */
  if (!create_default_admin_user(db)) {
    LOG_ERROR("RBAC_DB: Failed to create default admin user");
    json_free(rbac->users);
    json_free(rbac->roles);
    free(rbac);
    return NULL;
  }
  
  LOG_TRACE("RBAC_DB: Database-backed RBAC system initialized");
  return rbac;
}

/* Create default admin role */
int create_default_admin_role(struct database* db) {
  LOG_TRACE("RBAC_DB: Creating default admin role");
  
  /* No need to check by old ID since we're not preserving compatibility */
  
  /* Also check if any role with name "admin" exists to prevent duplicates */
  json_value_t* query = json_create_object();
  json_object_set(query, "name", json_create_string("admin"));
  json_value_t* results = db_query_documents(db, RBAC_ROLES_COLLECTION, query);
  json_free(query);
  
  if (results) {
    json_value_t* documents = json_object_get(results, "documents");
    if (documents && documents->type == JSON_ARRAY && json_array_size(documents) > 0) {
      LOG_TRACE("RBAC_DB: Admin role already exists by name");
      json_free(results);
      return 1;
    }
    json_free(results);
  }
  
  /* Create admin role document with standard document ID */
  char* admin_role_id = generate_document_id();
  if (!admin_role_id) {
    LOG_ERROR("RBAC_DB: Failed to generate admin role ID");
    return 0;
  }
  
  json_value_t* admin_role = json_create_object();
  json_object_set(admin_role, "_id", json_create_string(admin_role_id));
  free(admin_role_id);
  json_object_set(admin_role, "name", json_create_string("admin"));
  json_object_set(admin_role, "cn", json_create_string("Administrator Role"));
  json_object_set(admin_role, "description", json_create_string("System administrator with full access"));
  
  /* Set permissions - admin has all permissions on everything */
  json_value_t* permissions = json_create_object();
  json_value_t* collections = json_create_object();
  json_value_t* all_perms = json_create_array();
  
  json_array_append(all_perms, json_create_string("CREATE"));
  json_array_append(all_perms, json_create_string("READ"));
  json_array_append(all_perms, json_create_string("UPDATE"));
  json_array_append(all_perms, json_create_string("DELETE"));
  json_array_append(all_perms, json_create_string("ADMIN"));
  
  json_object_set(collections, "*", all_perms);
  json_object_set(permissions, "collections", collections);
  
  json_value_t* system_perms = json_create_array();
  json_array_append(system_perms, json_create_string("*"));
  json_object_set(permissions, "system", system_perms);
  
  /* Add RBAC permissions - admin has all permissions on all RBAC resources */
  json_object_set(permissions, "0:*", json_create_number(15)); /* All permissions for database */
  json_object_set(permissions, "1:*", json_create_number(15)); /* All permissions for collections */
  json_object_set(permissions, "2:*", json_create_number(15)); /* All permissions for documents */
  json_object_set(permissions, "3:*", json_create_number(15)); /* All permissions for roles */
  json_object_set(permissions, "4:*", json_create_number(15)); /* All permissions for users */
  json_object_set(permissions, "5:*", json_create_number(15)); /* All permissions for permissions */
  
  json_object_set(admin_role, "permissions", permissions);
  
  /* Add timestamps */
  char timestamp[64];
  time_t now = time(NULL);
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
  json_object_set(admin_role, "created_at", json_create_string(timestamp));
  json_object_set(admin_role, "updated_at", json_create_string(timestamp));
  
  /* Insert role */
  json_value_t* result = db_insert_document(db, RBAC_ROLES_COLLECTION, admin_role);
  json_free(admin_role);
  
  if (!result) {
    LOG_ERROR("RBAC_DB: Failed to create admin role");
    return 0;
  }
  
  const char* role_id = json_get_string(json_object_get(result, "_id"));
  json_free(result);
  
  LOG_TRACE("RBAC_DB: Admin role created with ID: %s", role_id ? role_id : "(null)");
  return 1;
}

/* Create default admin user */
int create_default_admin_user(struct database* db) {
  LOG_TRACE("RBAC_DB: Creating default admin user");
  
  /* No need to check by old ID since we're not preserving compatibility */
  
  /* Also check if any user with username "admin" exists to prevent duplicates */
  json_value_t* query = json_create_object();
  json_object_set(query, "username", json_create_string("admin"));
  json_value_t* results = db_query_documents(db, RBAC_USERS_COLLECTION, query);
  json_free(query);
  
  if (results) {
    json_value_t* documents = json_object_get(results, "documents");
    if (documents && documents->type == JSON_ARRAY && json_array_size(documents) > 0) {
      LOG_TRACE("RBAC_DB: Admin user already exists by username");
      
      /* Check if admin user has correct role ID */
      json_value_t* admin_user = json_array_get(documents, 0);
      json_value_t* user_id_val = json_object_get(admin_user, "_id");
      json_value_t* roles_val = json_object_get(admin_user, "roles");
      
      if (user_id_val && roles_val && roles_val->type == JSON_ARRAY) {
        const char* user_id = user_id_val->value.string;
        int needs_update = 0;
        
        /* Check if roles contain "admin" string instead of role ID */
        for (size_t i = 0; i < json_array_size(roles_val); i++) {
          json_value_t* role = json_array_get(roles_val, i);
          if (role->type == JSON_STRING && strcmp(role->value.string, "admin") == 0) {
            needs_update = 1;
            break;
          }
        }
        
        if (needs_update) {
          LOG_INFO("RBAC_DB: Fixing admin user roles - replacing 'admin' with role ID");
          
          /* Get admin role ID */
          json_value_t* role_query = json_create_object();
          json_object_set(role_query, "name", json_create_string("admin"));
          json_value_t* role_results = db_query_documents(db, RBAC_ROLES_COLLECTION, role_query);
          json_free(role_query);
          
          if (role_results) {
            json_value_t* role_docs = json_object_get(role_results, "documents");
            if (role_docs && role_docs->type == JSON_ARRAY && json_array_size(role_docs) > 0) {
              json_value_t* admin_role = json_array_get(role_docs, 0);
              json_value_t* role_id_val = json_object_get(admin_role, "_id");
              
              if (role_id_val && role_id_val->type == JSON_STRING) {
                const char* admin_role_id = role_id_val->value.string;
                
                /* Update user with correct role ID */
                json_value_t* updated_user = json_clone(admin_user);
                json_value_t* new_roles = json_create_array();
                json_array_append(new_roles, json_create_string(admin_role_id));
                json_object_set(updated_user, "roles", new_roles);
                
                /* Update the user document */
                db_update_document(db, RBAC_USERS_COLLECTION, user_id, updated_user);
                json_free(updated_user);
                
                LOG_INFO("RBAC_DB: Admin user roles updated with role ID: %s", admin_role_id);
              }
            }
            json_free(role_results);
          }
        }
      }
      
      json_free(results);
      return 1;
    }
    json_free(results);
  }
  
  /* Look up the admin role by name to get its ID */
  json_value_t* role_query = json_create_object();
  json_object_set(role_query, "name", json_create_string("admin"));
  json_value_t* role_results = db_query_documents(db, RBAC_ROLES_COLLECTION, role_query);
  json_free(role_query);
  
  const char* admin_role_id = NULL;
  if (role_results) {
    json_value_t* documents = json_object_get(role_results, "documents");
    if (documents && documents->type == JSON_ARRAY && json_array_size(documents) > 0) {
      json_value_t* admin_role = json_array_get(documents, 0);
      json_value_t* role_id_val = json_object_get(admin_role, "_id");
      if (role_id_val && role_id_val->type == JSON_STRING) {
        admin_role_id = role_id_val->value.string;
      }
    }
  }
  
  if (!admin_role_id) {
    LOG_ERROR("RBAC_DB: Admin role not found");
    if (role_results) json_free(role_results);
    return 0;
  }
  
  /* Create admin user document with standard document ID */
  char* admin_id = generate_document_id();
  if (!admin_id) {
    LOG_ERROR("RBAC_DB: Failed to generate admin user ID");
    return 0;
  }
  
  json_value_t* admin_user = json_create_object();
  json_object_set(admin_user, "_id", json_create_string(admin_id));
  free(admin_id);
  json_object_set(admin_user, "username", json_create_string("admin"));
  json_object_set(admin_user, "cn", json_create_string("System Administrator"));
  json_object_set(admin_user, "email", json_create_string("admin@localhost"));
  
  /* Hash the default password "admin" */
  char* password_hash = hash_password("admin");
  if (!password_hash) {
    LOG_ERROR("RBAC_DB: Failed to hash password");
    json_free(admin_user);
    return 0;
  }
  
  json_object_set(admin_user, "password_hash", json_create_string(password_hash));
  free(password_hash);
  
  /* Add admin role */
  json_value_t* user_roles = json_create_array();
  json_array_append(user_roles, json_create_string(admin_role_id));
  json_object_set(admin_user, "roles", user_roles);
  
  /* Add metadata */
  json_object_set(admin_user, "active", json_create_boolean(1));
  
  char timestamp[64];
  time_t now = time(NULL);
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
  json_object_set(admin_user, "created_at", json_create_string(timestamp));
  json_object_set(admin_user, "updated_at", json_create_string(timestamp));
  
  /* Insert user */
  json_value_t* user_result = db_insert_document(db, RBAC_USERS_COLLECTION, admin_user);
  json_free(admin_user);
  
  if (!user_result) {
    LOG_ERROR("RBAC_DB: Failed to create admin user");
    if (role_results) json_free(role_results);
    return 0;
  }
  
  const char* user_id = json_get_string(json_object_get(user_result, "_id"));
  LOG_TRACE("RBAC_DB: Admin user created with ID: %s", user_id ? user_id : "(null)");
  json_free(user_result);
  if (role_results) json_free(role_results);
  return 1;
}

/* Get user by username from database */
rbac_user_t* rbac_database_get_user_by_username(struct database* db, const char* username) {
  LOG_TRACE("RBAC_DB: Getting user by username: %s", username);
  
  if (!db || !username) {
    LOG_ERROR("RBAC_DB: Invalid parameters");
    return NULL;
  }
  
  /* Query for user */
  json_value_t* query = json_create_object();
  json_object_set(query, "username", json_create_string(username));
  
  json_value_t* result = db_query_documents(db, RBAC_USERS_COLLECTION, query);
  json_free(query);
  
  if (!result) {
    LOG_ERROR("RBAC_DB: Query failed");
    return NULL;
  }
  
  /* Check if user found */
  json_value_t* docs = json_object_get(result, "documents");
  if (!docs || docs->value.array.size == 0) {
    LOG_TRACE("RBAC_DB: User not found: %s", username);
    json_free(result);
    return NULL;
  }
  
  /* Get first user document */
  json_value_t* user_doc = docs->value.array.items[0];
  
  /* Create rbac_user_t structure */
  rbac_user_t* user = (rbac_user_t*)malloc(sizeof(rbac_user_t));
  if (!user) {
    LOG_ERROR("RBAC_DB: Failed to allocate memory for user");
    json_free(result);
    return NULL;
  }
  
  /* Extract user data */
  json_value_t* id_val = json_object_get(user_doc, "_id");
  json_value_t* username_val = json_object_get(user_doc, "username");
  json_value_t* password_hash_val = json_object_get(user_doc, "password_hash");
  json_value_t* roles_val = json_object_get(user_doc, "roles");
  
  user->id = strdup(id_val ? id_val->value.string : "");
  user->username = strdup(username_val ? username_val->value.string : username);
  user->password_hash = strdup(password_hash_val ? password_hash_val->value.string : "");
  
  /* Copy roles array */
  if (roles_val && roles_val->type == JSON_ARRAY) {
    user->roles = json_clone(roles_val);
  } else {
    user->roles = json_create_array();
  }
  
  json_free(result);
  
  LOG_TRACE("RBAC_DB: User found with ID: %s", user->id);
  return user;
}

/* Create new user in database */
rbac_user_t* rbac_database_create_user(struct database* db, const char* username, const char* password) {
  LOG_TRACE("RBAC_DB: Creating user: %s", username);
  
  if (!db || !username || !password) {
    LOG_ERROR("RBAC_DB: Invalid parameters");
    return NULL;
  }
  
  /* Check if user already exists */
  rbac_user_t* existing = rbac_db_get_user_by_username(db, username);
  if (existing) {
    LOG_ERROR("RBAC_DB: User already exists: %s", username);
    rbac_free_user(existing);
    return NULL;
  }
  
  /* Hash password */
  char* password_hash = hash_password(password);
  if (!password_hash) {
    LOG_ERROR("RBAC_DB: Failed to hash password");
    return NULL;
  }
  
  /* Generate standard document ID */
  char* user_id = generate_document_id();
  if (!user_id) {
    LOG_ERROR("RBAC_DB: Failed to generate user ID");
    free(password_hash);
    return NULL;
  }
  
  /* Check if user with this ID already exists (shouldn't happen after cleanup) */
  json_value_t* existing_by_id = db_get_document(db, RBAC_USERS_COLLECTION, user_id);
  if (existing_by_id) {
    LOG_ERROR("RBAC_DB: User with ID %s already exists", user_id);
    json_free(existing_by_id);
    free(user_id);
    free(password_hash);
    return NULL;
  }
  
  /* Create user document with standardized ID */
  json_value_t* user_doc = json_create_object();
  json_object_set(user_doc, "_id", json_create_string(user_id));
  json_object_set(user_doc, "username", json_create_string(username));
  json_object_set(user_doc, "password_hash", json_create_string(password_hash));
  json_object_set(user_doc, "roles", json_create_array());
  json_object_set(user_doc, "active", json_create_boolean(1));
  
  /* Add timestamps */
  char timestamp[64];
  time_t now = time(NULL);
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
  json_object_set(user_doc, "created_at", json_create_string(timestamp));
  json_object_set(user_doc, "updated_at", json_create_string(timestamp));
  
  /* Insert user */
  json_value_t* insert_result = db_insert_document(db, RBAC_USERS_COLLECTION, user_doc);
  json_free(user_doc);
  
  if (!insert_result) {
    LOG_ERROR("RBAC_DB: Failed to insert user");
    free(user_id);
    free(password_hash);
    return NULL;
  }
  
  json_free(insert_result);
  
  /* Create rbac_user_t structure */
  rbac_user_t* user = (rbac_user_t*)malloc(sizeof(rbac_user_t));
  if (!user) {
    LOG_ERROR("RBAC_DB: Failed to allocate memory for user");
    free(user_id);
    free(password_hash);
    return NULL;
  }
  
  user->id = user_id;
  user->username = strdup(username);
  user->password_hash = password_hash;
  user->roles = json_create_array();
  
  LOG_TRACE("RBAC_DB: User created with ID: %s", user_id);
  return user;
}

/* Create new role in database */
rbac_role_t* rbac_database_create_role(struct database* db, const char* rolename, const char* description) {
  LOG_TRACE("RBAC_DB: Creating role: %s", rolename);
  
  if (!db || !rolename) {
    LOG_ERROR("RBAC_DB: Invalid parameters");
    return NULL;
  }
  
  /* Generate standard document ID */
  char* role_id = generate_document_id();
  if (!role_id) {
    LOG_ERROR("RBAC_DB: Failed to generate role ID");
    return NULL;
  }
  
  /* Check if role already exists by name */
  json_value_t* query = json_create_object();
  json_object_set(query, "name", json_create_string(rolename));
  json_value_t* results = db_query_documents(db, RBAC_ROLES_COLLECTION, query);
  json_free(query);
  
  if (results) {
    json_value_t* documents = json_object_get(results, "documents");
    if (documents && documents->type == JSON_ARRAY && json_array_size(documents) > 0) {
      LOG_ERROR("RBAC_DB: Role already exists: %s", rolename);
      json_free(results);
      free(role_id);
      return NULL;
    }
    json_free(results);
  }
  
  /* Check if role with this ID already exists (shouldn't happen after cleanup) */
  json_value_t* existing_by_id = db_get_document(db, RBAC_ROLES_COLLECTION, role_id);
  if (existing_by_id) {
    LOG_ERROR("RBAC_DB: Role with ID %s already exists", role_id);
    json_free(existing_by_id);
    free(role_id);
    return NULL;
  }
  
  /* Create role document with standardized ID */
  json_value_t* role_doc = json_create_object();
  json_object_set(role_doc, "_id", json_create_string(role_id));
  json_object_set(role_doc, "name", json_create_string(rolename));
  json_object_set(role_doc, "description", json_create_string(description ? description : ""));
  json_object_set(role_doc, "permissions", json_create_object());
  
  /* Add timestamps */
  char timestamp[64];
  time_t now = time(NULL);
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
  json_object_set(role_doc, "created_at", json_create_string(timestamp));
  json_object_set(role_doc, "updated_at", json_create_string(timestamp));
  
  /* Insert role */
  json_value_t* insert_result = db_insert_document(db, RBAC_ROLES_COLLECTION, role_doc);
  json_free(role_doc);
  
  if (!insert_result) {
    LOG_ERROR("RBAC_DB: Failed to insert role");
    free(role_id);
    return NULL;
  }
  
  json_free(insert_result);
  
  /* Create rbac_role_t structure */
  rbac_role_t* role = (rbac_role_t*)malloc(sizeof(rbac_role_t));
  if (!role) {
    LOG_ERROR("RBAC_DB: Failed to allocate memory for role");
    free(role_id);
    return NULL;
  }
  
  role->id = role_id;
  role->name = strdup(rolename);
  role->permissions = json_create_object();
  
  LOG_TRACE("RBAC_DB: Role created with ID: %s", role_id);
  return role;
}

/* Check user permissions on a resource */
int rbac_database_check_permission(struct database* db, const char* user_id, rbac_resource_type_t resource_type,
              const char* resource_id, rbac_permission_t permission) {
  /* Convert resource type to string */
  const char* resource_type_str = "unknown";
  switch (resource_type) {
    case RBAC_DATABASE: resource_type_str = "database"; break;
    case RBAC_COLLECTION: resource_type_str = "collection"; break;
    case RBAC_DOCUMENT: resource_type_str = "document"; break;
    case RBAC_USER: resource_type_str = "user"; break;
    case RBAC_ROLE: resource_type_str = "role"; break;
    case RBAC_PERMISSION: resource_type_str = "permission"; break;
    default: break;
  }
  
  /* Convert permission to string */
  const char* permission_str = "unknown";
  if (permission & RBAC_READ) permission_str = "READ";
  else if (permission & RBAC_WRITE) permission_str = "WRITE";
  else if (permission & RBAC_DELETE) permission_str = "DELETE";
  else if (permission & RBAC_ADMIN) permission_str = "ADMIN";
  
  LOG_TRACE("RBAC_DB: Checking permission - user: %s, resource: %s:%s, permission: %s",
       user_id, resource_type_str, resource_id, permission_str);
  
  if (!db || !user_id || !resource_id) {
    LOG_ERROR("RBAC_DB: Invalid parameters");
    return 0;
  }
  
  /* First check permission cache */
  char cache_key[256];
  snprintf(cache_key, sizeof(cache_key), "%s:%s:%s", user_id, resource_type_str, resource_id);
  
  json_value_t* cache_query = json_create_object();
  json_object_set(cache_query, "_id", json_create_string(cache_key));
  
  json_value_t* cache_result = db_query_documents(db, RBAC_PERMISSION_CACHE_COLLECTION, cache_query);
  json_free(cache_query);
  
  if (cache_result) {
    json_value_t* docs = json_object_get(cache_result, "documents");
    if (docs && docs->value.array.size > 0) {
      json_value_t* cache_doc = docs->value.array.items[0];
      json_value_t* expires_at = json_object_get(cache_doc, "expires_at");
      
      /* Check if cache is still valid */
      if (expires_at) {
        time_t now = time(NULL);
        time_t expiry = (time_t)expires_at->value.number;
        
        if (now < expiry) {
          LOG_TRACE("RBAC_DB: Using cached permissions");
          json_value_t* perms = json_object_get(cache_doc, "permissions");
          
          if (perms && perms->type == JSON_ARRAY) {
            for (size_t i = 0; i < perms->value.array.size; i++) {
              json_value_t* perm = perms->value.array.items[i];
              if (perm->type == JSON_STRING && 
                strcmp(perm->value.string, permission_str) == 0) {
                LOG_TRACE("RBAC_DB: Permission granted (cached)");
                json_free(cache_result);
                return 1;
              }
            }
          }
          
          LOG_TRACE("RBAC_DB: Permission denied (cached)");
          json_free(cache_result);
          return 0;
        }
      }
    }
    json_free(cache_result);
  }
  
  /* Cache miss or expired - compute permissions */
  LOG_TRACE("RBAC_DB: Computing permissions from database");
  
  /* TODO: Implement full permission resolution algorithm */
  /* For now, just check if user has admin role */
  json_value_t* user_query = json_create_object();
  json_object_set(user_query, "_id", json_create_string(user_id));
  
  LOG_TRACE("RBAC_DB: Querying user with _id: %s", user_id);
  
  json_value_t* user_result = db_query_documents(db, RBAC_USERS_COLLECTION, user_query);
  json_free(user_query);
  
  if (!user_result) {
    LOG_ERROR("RBAC_DB: Failed to query user");
    return 0;
  }
  
  json_value_t* user_docs = json_object_get(user_result, "documents");
  if (!user_docs || user_docs->value.array.size == 0) {
    LOG_ERROR("RBAC_DB: User not found with id: %s", user_id);
    LOG_TRACE("RBAC_DB: Query result: %s", json_stringify(user_result));
    json_free(user_result);
    return 0;
  }
  
  LOG_TRACE("RBAC_DB: Found %zu users", user_docs->value.array.size);
  
  /* Get user's roles and check permissions */
  json_value_t* user_doc = user_docs->value.array.items[0];
  json_value_t* user_roles = json_object_get(user_doc, "roles");
  
  int has_permission = 0;
  
  if (user_roles && user_roles->type == JSON_ARRAY) {
    LOG_TRACE("RBAC_DB: User has %zu roles", user_roles->value.array.size);
    
    /* Check each role's permissions */
    for (size_t i = 0; i < user_roles->value.array.size; i++) {
      json_value_t* role_id_val = user_roles->value.array.items[i];
      if (role_id_val->type != JSON_STRING) continue;
      
      const char* role_id = role_id_val->value.string;
      LOG_TRACE("RBAC_DB: Checking role: %s", role_id);
      
      /* Query the role */
      json_value_t* role_doc = db_get_document(db, RBAC_ROLES_COLLECTION, role_id);
      if (!role_doc) {
        LOG_TRACE("RBAC_DB: Role not found: %s", role_id);
        continue;
      }
      
      /* Check role permissions */
      json_value_t* permissions = json_object_get(role_doc, "permissions");
      if (permissions) {
        /* Build permission key: resource_type:resource_id */
        char perm_key[64];
        snprintf(perm_key, sizeof(perm_key), "%d:%s", (int)resource_type, resource_id);
        
        LOG_TRACE("RBAC_DB: Looking for permission key: %s", perm_key);
        
        /* Check exact match first */
        json_value_t* perm_value = json_object_get(permissions, perm_key);
        if (perm_value && perm_value->type == JSON_NUMBER) {
          int perm_mask = (int)perm_value->value.number;
          LOG_TRACE("RBAC_DB: Found exact permission: %d (checking for %d)", perm_mask, permission);
          if (perm_mask & permission) {
            has_permission = 1;
            json_free(role_doc);
            break;
          }
        }
        
        /* Check wildcard permission */
        snprintf(perm_key, sizeof(perm_key), "%d:*", (int)resource_type);
        perm_value = json_object_get(permissions, perm_key);
        if (perm_value && perm_value->type == JSON_NUMBER) {
          int perm_mask = (int)perm_value->value.number;
          LOG_TRACE("RBAC_DB: Found wildcard permission: %d (checking for %d)", perm_mask, permission);
          if (perm_mask & permission) {
            has_permission = 1;
            json_free(role_doc);
            break;
          }
        }
        
        /* For admin endpoints, also check legacy system permissions */
        if (resource_type == RBAC_USER || resource_type == RBAC_ROLE) {
          json_value_t* system_perms = json_object_get(permissions, "system");
          if (system_perms && system_perms->type == JSON_ARRAY) {
            for (size_t j = 0; j < system_perms->value.array.size; j++) {
              json_value_t* sys_perm = system_perms->value.array.items[j];
              if (sys_perm->type == JSON_STRING && strcmp(sys_perm->value.string, "*") == 0) {
                LOG_TRACE("RBAC_DB: User has system:* permission");
                has_permission = 1;
                break;
              }
            }
          }
        }
      }
      
      json_free(role_doc);
      
      if (has_permission) break;
    }
  } else {
    LOG_TRACE("RBAC_DB: User has no roles");
  }
  
  json_free(user_result);
  
  LOG_TRACE("RBAC_DB: Permission %s", has_permission ? "granted" : "denied");
  return has_permission;
}