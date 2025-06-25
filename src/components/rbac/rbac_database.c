#include "rbac/rbac.h"
#include "rbac/rbac_db.h"
#include "rbac/rbac_unified_documents.h"
#include "database/database.h"
#include "database/document_storage.h"
#include "utils/logger.h"
#include "utils/json.h"
#include "utils/buffer_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Forward declarations for JDBX structures */
typedef struct library {
    char name[64];
    void* collections;
    pthread_rwlock_t lock;
} library_t;

typedef struct collection {
    char name[64];
    char library[64];
    void* documents;
    void* indexes;
    json_value_t* schema;
    pthread_rwlock_t lock;
} collection_t;


/* Additional RBAC collection names not in header */
#define RBAC_PERMISSIONS_COLLECTION "system/permissions"
#define RBAC_COLLECTIONS_COLLECTION "system/collections"
#define RBAC_SESSIONS_COLLECTION "system/sessions"
#define RBAC_PERMISSION_CACHE_COLLECTION "system/permission_cache"

/* Permission cache TTL in seconds (5 minutes) */
#define PERMISSION_CACHE_TTL 300

/* Document ID generation is now handled by db_insert_document which generates UUIDs */

/* Note: cleanup_all_rbac_duplicates function removed as it was unused.
 * If duplicate cleanup is needed in the future, implement it as part of
 * the regular RBAC maintenance operations. */


/* Forward declarations */
/* Removed old ID generation forward declarations - using generate_document_id() now */

/* Initialize RBAC system with database backend */
rbac_system_t* rbac_database_init(struct database* db, const char* jwt_secret) {
  TRACE_RBAC("Initializing database-backed RBAC system");
  
  if (!db) return NULL;
  
  /* Create wrapper structure */
  rbac_system_t* rbac = BUFFER_ALLOC(sizeof(rbac_system_t));
  if (rbac) memset(rbac, 0, sizeof(rbac_system_t));
  if (!rbac) return NULL;
  
  /* Set up fields - SINGLE SOURCE OF TRUTH: Database Only */
  rbac->db = db;
  rbac->jwt_secret = jwt_secret ? BUFFER_STRDUP(jwt_secret) : BUFFER_STRDUP("change-this-secret-in-production");
  
  if (!rbac->jwt_secret) {
    BUFFER_FREE(rbac);
    return NULL;
  }
  
  LOG_INFO("RBAC system initialized");
  return rbac;
}

/* Direct bootstrap insert function that bypasses normal API and stores directly */
extern json_value_t* db_insert_direct_bootstrap(const char* collection_path, json_value_t* document);

/* bootstrap_insert_document REMOVED - unified documents API only for single source of truth */

/* Create default admin role */
int create_default_admin_role(struct database* db, char** admin_role_id_out) {
  TRACE_RBAC("Creating default admin role.");
  
  /* Skip duplicate checking during bootstrap mode */
  if (!db->is_bootstrap_mode) {
    /* Check if any role with name "admin" exists using unified documents approach */
    json_value_t* unified_query = rbac_build_role_query(RBAC_LIBRARY_SYSTEM);
    json_object_set(unified_query, "name", json_create_string(DEFAULT_ADMIN_ROLE));
    json_value_t* results = virtual_query(db, DOC_TYPE_NAME_ROLE, RBAC_LIBRARY_SYSTEM, VIRTUAL_COLLECTION_ROLES, unified_query);
    /* CHECKPOINT: json_free(unified_query); */
    
    if (results) {
      json_value_t* documents = json_object_get(results, "documents");
      if (documents && documents->type == JSON_ARRAY && json_array_size(documents) > 0) {
        TRACE_RBAC("Admin role already exists by name.");
        /* Get the UUID from the existing role */
        json_value_t* existing_role = json_array_get(documents, 0);
        json_value_t* role_uuid = json_object_get(existing_role, "uuid");
        if (!role_uuid) {
          role_uuid = json_object_get(existing_role, "uuid");
        }
        
        if (role_uuid && role_uuid->type == JSON_STRING && admin_role_id_out) {
          *admin_role_id_out = BUFFER_STRDUP(role_uuid->value.string);
          TRACE_RBAC("Using existing admin role ID: %s", role_uuid->value.string);
          /* CHECKPOINT: json_free(results); */
          return 1; /* Success - role already exists */
        }
        
        LOG_ERROR("Admin role exists but has no ID.");
        /* CHECKPOINT: json_free(results); */
        return 0;
      }
      /* CHECKPOINT: json_free(results); */
    }
  } else {
    TRACE_RBAC("Bootstrap mode - skipping duplicate role check");
  }
  
  /* Create admin role document using unified documents architecture */
  json_value_t* admin_role = json_create_object();
  
  /* UNIFIED DOCUMENTS: Add mandatory fields for proper storage */
  json_object_set(admin_role, "type", json_create_string(DOC_TYPE_NAME_ROLE));
  json_object_set(admin_role, "library", json_create_string(RBAC_SYSTEM_LIBRARY));
  json_object_set(admin_role, "collection", json_create_string(RBAC_ROLES_COLLECTION_NAME));
  json_object_set(admin_role, "owner", json_create_string("system-admin"));
  
  /* Role-specific fields */
  json_object_set(admin_role, "name", json_create_string(DEFAULT_ADMIN_ROLE));
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
  json_object_set(admin_role, "modified_at", json_create_string(timestamp));
  
  /* Insert role using UNIFIED DOCUMENTS ARCHITECTURE */
  json_value_t* result = NULL;
  TRACE_RBAC("Inserting admin role into unified documents storage (default/documents)");
  result = virtual_insert(db, DOC_TYPE_NAME_ROLE, RBAC_LIBRARY_SYSTEM, VIRTUAL_COLLECTION_ROLES, admin_role, "system-admin");
  /* CHECKPOINT: json_free(admin_role); */
  
  if (!result) {
    LOG_ERROR("Cannot create admin role.");
    return 0;
  }
  
  /* Store the generated ID for later use */
  TRACE_RBAC("Insert result: %s", json_stringify(result));
  json_value_t* id_val = json_object_get(result, "uuid");
  if (id_val && id_val->type == JSON_STRING) {
    TRACE_RBAC("Admin role created with ID: %s", id_val->value.string);
    if (admin_role_id_out) {
      *admin_role_id_out = BUFFER_STRDUP(id_val->value.string);
    }
  } else {
    LOG_ERROR("Cannot get _id from insert result.");
    /* CHECKPOINT: json_free(result); */
    return 0;
  }
  
  /* CHECKPOINT: json_free(result); */
  
  TRACE_RBAC("Admin role created successfully.");
  return 1;
}

/* Create default user role */
int create_default_user_role(struct database* db) {
  TRACE_RBAC("Creating default user role.");
  
  /* Check if any role with name "user" exists to prevent duplicates */
  json_value_t* query = rbac_build_role_query(RBAC_LIBRARY_SYSTEM);
  json_object_set(query, "name", json_create_string("user"));
  json_value_t* results = virtual_query(db, DOC_TYPE_NAME_ROLE, RBAC_LIBRARY_SYSTEM, VIRTUAL_COLLECTION_ROLES, query);
  /* CHECKPOINT: json_free(query); */
  
  if (results) {
    json_value_t* documents = json_object_get(results, "documents");
    if (documents && documents->type == JSON_ARRAY && json_array_size(documents) > 0) {
      TRACE_RBAC("User role already exists by name.");
      /* CHECKPOINT: json_free(results); */
      return 1;  /* OK for user role to exist */
    }
    /* CHECKPOINT: json_free(results); */
  }
  
  /* Create user role document - let database generate UUID */
  json_value_t* user_role = json_create_object();
  /* Don't set _id - let db_insert_document generate it */
  json_object_set(user_role, "name", json_create_string("user"));
  json_object_set(user_role, "cn", json_create_string("Standard User Role"));
  json_object_set(user_role, "description", json_create_string("Standard user with limited access"));
  
  /* Set permissions - users have read access to their own data and limited write */
  json_value_t* permissions = json_create_object();
  json_value_t* collections = json_create_object();
  
  /* Users can read/write their own documents in non-system collections */
  json_value_t* user_perms = json_create_array();
  json_array_append(user_perms, json_create_string("READ"));
  json_array_append(user_perms, json_create_string("UPDATE"));
  json_array_append(user_perms, json_create_string("DELETE"));
  
  /* Apply to all non-system collections (those not starting with _) */
  json_object_set(collections, "*", user_perms);
  
  /* No access to system collections */
  json_value_t* no_perms = json_create_array();
  json_object_set(collections, "_*", no_perms);
  
  json_object_set(permissions, "collections", collections);
  json_object_set(user_role, "permissions", permissions);
  
  /* Add timestamps */
  char timestamp[64];
  time_t now = time(NULL);
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
  json_object_set(user_role, "created_at", json_create_string(timestamp));
  json_object_set(user_role, "updated_at", json_create_string(timestamp));
  
  /* Insert role */
  json_value_t* result = virtual_insert(db, DOC_TYPE_NAME_ROLE, RBAC_LIBRARY_SYSTEM, VIRTUAL_COLLECTION_ROLES, user_role, "system-admin");
  /* CHECKPOINT: json_free(user_role); */
  
  if (!result) {
    LOG_ERROR("Cannot create user role.");
    return 0;
  }
  
  /* Store the generated ID for later use */
  TRACE_RBAC("User role insert result: %s", json_stringify(result));
  json_value_t* id_val = json_object_get(result, "uuid");
  if (id_val && id_val->type == JSON_STRING) {
    TRACE_RBAC("User role created with ID: %s", id_val->value.string);
  }
  
  /* CHECKPOINT: json_free(result); */
  
  TRACE_RBAC("User role created successfully.");
  return 1;
}

/* Create default admin user */
int create_default_admin_user(struct database* db, const char* admin_role_id) {
  TRACE_RBAC("Creating default admin user.");
  
  if (!admin_role_id) {
    LOG_ERROR("Admin role ID not provided.");
    return 0;
  }
  
  /* Skip duplicate checking during bootstrap mode */
  if (!db->is_bootstrap_mode) {
    /* Check if any user with username "admin" exists using unified documents approach */
    json_value_t* unified_query = rbac_build_user_query(RBAC_LIBRARY_SYSTEM);
    json_object_set(unified_query, "username", json_create_string(DEFAULT_ADMIN_USERNAME));
    json_value_t* results = virtual_query(db, DOC_TYPE_NAME_USER, RBAC_LIBRARY_SYSTEM, VIRTUAL_COLLECTION_USERS, unified_query);
    /* CHECKPOINT: json_free(unified_query); */
    
    if (results) {
      json_value_t* documents = json_object_get(results, "documents");
      if (documents && documents->type == JSON_ARRAY && json_array_size(documents) > 0) {
        TRACE_RBAC("Admin user already exists by username.");
        
        /* Check if admin user has correct role ID */
        json_value_t* admin_user = json_array_get(documents, 0);
        json_value_t* user_id_val = json_object_get(admin_user, "uuid");
        if (!user_id_val) {
          user_id_val = json_object_get(admin_user, "uuid");
        }
        json_value_t* roles_val = json_object_get(admin_user, "roles");
        
        if (user_id_val && roles_val && roles_val->type == JSON_ARRAY) {
        const char* user_id = user_id_val->value.string;
        int needs_update = 0;
        
        /* Check if roles contain "admin" string instead of role ID */
        for (size_t i = 0; i < json_array_size(roles_val); i++) {
          json_value_t* role = json_array_get(roles_val, i);
          if (role->type == JSON_STRING && strcmp(role->value.string, DEFAULT_ADMIN_ROLE) == 0) {
            needs_update = 1;
            break;
          }
        }
        
        if (needs_update) {
          LOG_INFO("Fixing admin user roles - replacing 'admin' with role ID.");
          
          /* Get admin role ID using unified documents approach */
          json_value_t* role_unified_query = rbac_build_role_query(RBAC_LIBRARY_SYSTEM);
          json_object_set(role_unified_query, "name", json_create_string(DEFAULT_ADMIN_ROLE));
          json_value_t* role_results = virtual_query(db, DOC_TYPE_NAME_ROLE, RBAC_LIBRARY_SYSTEM, VIRTUAL_COLLECTION_ROLES, role_unified_query);
          /* CHECKPOINT: json_free(role_unified_query); */
          
          if (role_results) {
            json_value_t* role_docs = json_object_get(role_results, "documents");
            if (role_docs && role_docs->type == JSON_ARRAY && json_array_size(role_docs) > 0) {
              json_value_t* admin_role = json_array_get(role_docs, 0);
              json_value_t* role_id_val = json_object_get(admin_role, "uuid");
              
              if (role_id_val && role_id_val->type == JSON_STRING) {
                const char* admin_role_id = role_id_val->value.string;
                
                /* Update user with correct role ID */
                json_value_t* updated_user = json_clone(admin_user);
                json_value_t* new_roles = json_create_array();
                json_array_append(new_roles, json_create_string(admin_role_id));
                json_object_set(updated_user, "roles", new_roles);
                
                /* Update the user document in unified storage */
                virtual_update(db, user_id, updated_user);
                /* CHECKPOINT: json_free(updated_user); */
                
                LOG_INFO("Admin user roles updated with role ID: %s", admin_role_id);
              }
            }
            /* CHECKPOINT: json_free(role_results); */
          }
        }
        }
      }
      
      /* CHECKPOINT: json_free(results); */
      return 1;  /* Admin user already exists, that's OK */
    }
    /* CHECKPOINT: json_free(results); */
  } else {
    TRACE_RBAC("Bootstrap mode - skipping duplicate user check");
  }
  
  TRACE_RBAC("Using admin role ID: %s", admin_role_id);
  
  /* Check for initial admin configuration from environment */
  const char* initial_admin_user = getenv("JDBX_BOOTSTRAP_ADMIN_USER");
  const char* initial_admin_pass = getenv("JDBX_BOOTSTRAP_ADMIN_PASS");
  const char* initial_admin_email = getenv("JDBX_DEFAULT_ADMIN_EMAIL");
  
  if (!initial_admin_user || !initial_admin_pass) {
    LOG_INFO("No initial admin configured. Database will require manual admin setup.");
    LOG_INFO("Set JDBX_BOOTSTRAP_ADMIN_USER and JDBX_BOOTSTRAP_ADMIN_PASS environment variables to create initial admin.");
    return 1;  /* Not an error - just no initial admin */
  }
  
  /* Create admin user document with business fields only */
  json_value_t* admin_user = json_create_object();
  
  /* User-specific business fields */
  json_object_set(admin_user, "username", json_create_string(initial_admin_user));
  json_object_set(admin_user, "cn", json_create_string("System Administrator"));
  json_object_set(admin_user, "email", json_create_string(initial_admin_email ? initial_admin_email : "admin@localhost"));
  
  /* Hash the provided password */
  char* password_hash = hash_password(initial_admin_pass);
  if (!password_hash) {
    LOG_ERROR("Cannot hash password.");
    /* CHECKPOINT: json_free(admin_user); */
    return 0;
  }
  
  json_object_set(admin_user, "password_hash", json_create_string(password_hash));
  BUFFER_FREE(password_hash);
  
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
  
  /* Insert user using GENERIC VIRTUAL LAYER (proper architecture) */
  TRACE_RBAC("Creating admin user via generic virtual layer");
  json_value_t* user_result = virtual_insert(db, DOC_TYPE_NAME_USER, RBAC_SYSTEM_LIBRARY, 
                                           RBAC_USERS_COLLECTION_NAME, admin_user, "system-admin");
  /* CHECKPOINT: json_free(admin_user); */
  
  if (!user_result) {
    LOG_ERROR("Cannot create admin user.");
    return 0;
  }
  
  const char* user_id = json_get_string(json_object_get(user_result, "uuid"));
  TRACE_RBAC("Initial admin user '%s' created with ID: %s", initial_admin_user, user_id ? user_id : "(null).");
  /* CHECKPOINT: json_free(user_result); */
  return 1;
}

/* Get user by username from database */
rbac_user_t* rbac_database_get_user_by_username(struct database* db, const char* username) {
  TRACE_RBAC("Getting user by username: %s", username);
  
  if (!db || !username) {
    LOG_ERROR("Invalid parameters.");
    return NULL;
  }
  
  /* Query for user in unified documents */
  json_value_t* query = rbac_build_user_query(RBAC_LIBRARY_SYSTEM);
  json_object_set(query, "username", json_create_string(username));
  
  json_value_t* result = virtual_query(db, DOC_TYPE_NAME_USER, RBAC_LIBRARY_SYSTEM, VIRTUAL_COLLECTION_USERS, query);
  /* CHECKPOINT: json_free(query); */
  
  if (!result) {
    LOG_ERROR("Query failed.");
    return NULL;
  }
  
  /* Extract documents array from response object */
  json_value_t* documents = json_object_get(result, "documents");
  if (!documents || documents->type != JSON_ARRAY || json_array_size(documents) == 0) {
    TRACE_RBAC("User not found: %s", username);
    /* CHECKPOINT: json_free(result); */
    return NULL;
  }
  
  /* Get first user document */
  json_value_t* user_doc = json_array_get(documents, 0);
  
  /* Create rbac_user_t structure */
  rbac_user_t* user = (rbac_user_t*)BUFFER_ALLOC(sizeof(rbac_user_t));
  if (!user) {
    LOG_ERROR("Cannot allocate memory for user.");
    /* CHECKPOINT: json_free(result); */
    return NULL;
  }
  
  /* Extract user data */
  json_value_t* id_val = json_object_get(user_doc, "uuid");
  json_value_t* username_val = json_object_get(user_doc, "username");
  json_value_t* password_hash_val = json_object_get(user_doc, "password_hash");
  json_value_t* roles_val = json_object_get(user_doc, "roles");
  
  user->id = BUFFER_STRDUP(id_val ? json_get_string(id_val) : "");
  user->username = BUFFER_STRDUP(username_val ? json_get_string(username_val) : username);
  user->password_hash = BUFFER_STRDUP(password_hash_val ? json_get_string(password_hash_val) : "");
  
  /* Copy roles array */
  if (roles_val && roles_val->type == JSON_ARRAY) {
    user->roles = json_clone(roles_val);
  } else {
    user->roles = json_create_array();
  }
  
  /* CHECKPOINT: json_free(result); */
  
  TRACE_RBAC("User found with ID: %s", user->id);
  return user;
}

/* Create new user in database */
rbac_user_t* rbac_database_create_user(struct database* db, const char* username, const char* password) {
  TRACE_RBAC("Creating user: %s", username);
  
  if (!db || !username || !password) {
    LOG_ERROR("Invalid parameters.");
    return NULL;
  }
  
  /* Check if user already exists */
  rbac_user_t* existing = rbac_db_get_user_by_username(db, username);
  if (existing) {
    LOG_ERROR("User already exists: %s", username);
    rbac_free_user(existing);
    return NULL;
  }
  
  /* Hash password */
  char* password_hash = hash_password(password);
  if (!password_hash) {
    LOG_ERROR("Cannot hash password.");
    return NULL;
  }
  
  /* Create user document - let database generate UUID */
  json_value_t* user_doc = json_create_object();
  /* Don't set _id - let db_insert_document generate it */
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
  json_value_t* insert_result = virtual_insert(db, DOC_TYPE_NAME_USER, RBAC_LIBRARY_SYSTEM, VIRTUAL_COLLECTION_USERS, user_doc, "system-admin");
  /* CHECKPOINT: json_free(user_doc); */
  
  if (!insert_result) {
    LOG_ERROR("Cannot insert user.");
    BUFFER_FREE(password_hash);
    return NULL;
  }
  
  /* Get the actual ID from the insert result */
  const char* actual_id = json_get_string(json_object_get(insert_result, "uuid"));
  if (!actual_id) {
    LOG_ERROR("Cannot get user ID from insert result.");
    /* CHECKPOINT: json_free(insert_result); */
    BUFFER_FREE(password_hash);
    return NULL;
  }
  
  /* Create rbac_user_t structure */
  rbac_user_t* user = (rbac_user_t*)BUFFER_ALLOC(sizeof(rbac_user_t));
  if (!user) {
    LOG_ERROR("Cannot allocate memory for user.");
    /* CHECKPOINT: json_free(insert_result); */
    BUFFER_FREE(password_hash);
    return NULL;
  }
  
  user->id = BUFFER_STRDUP(actual_id);
  user->username = BUFFER_STRDUP(username);
  user->password_hash = password_hash;
  user->roles = json_create_array();
  
  /* CHECKPOINT: json_free(insert_result); */
  
  TRACE_RBAC("User created with ID: %s", user->id);
  return user;
}

/* Create new role in database */
rbac_role_t* rbac_database_create_role(struct database* db, const char* rolename, const char* description) {
  TRACE_RBAC("Creating role: %s", rolename);
  
  if (!db || !rolename) {
    LOG_ERROR("Invalid parameters.");
    return NULL;
  }
  
  /* Check if role already exists by name */
  json_value_t* query = rbac_build_role_query(RBAC_LIBRARY_SYSTEM);
  json_object_set(query, "name", json_create_string(rolename));
  json_value_t* results = virtual_query(db, DOC_TYPE_NAME_ROLE, RBAC_LIBRARY_SYSTEM, VIRTUAL_COLLECTION_ROLES, query);
  /* CHECKPOINT: json_free(query); */
  
  if (results) {
    json_value_t* documents = json_object_get(results, "documents");
    if (documents && documents->type == JSON_ARRAY && json_array_size(documents) > 0) {
      LOG_ERROR("Role already exists: %s", rolename);
      /* CHECKPOINT: json_free(results); */
      return NULL;
    }
    /* CHECKPOINT: json_free(results); */
  }
  
  /* Create role document - let database generate UUID */
  json_value_t* role_doc = json_create_object();
  /* Don't set _id - let db_insert_document generate it */
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
  json_value_t* insert_result = virtual_insert(db, DOC_TYPE_NAME_ROLE, RBAC_LIBRARY_SYSTEM, VIRTUAL_COLLECTION_ROLES, role_doc, "system-admin");
  /* CHECKPOINT: json_free(role_doc); */
  
  if (!insert_result) {
    LOG_ERROR("Cannot insert role.");
    return NULL;
  }
  
  /* Get the actual ID from the insert result */
  const char* actual_id = json_get_string(json_object_get(insert_result, "uuid"));
  if (!actual_id) {
    LOG_ERROR("Cannot get role ID from insert result.");
    /* CHECKPOINT: json_free(insert_result); */
    return NULL;
  }
  
  /* Create rbac_role_t structure */
  rbac_role_t* role = (rbac_role_t*)BUFFER_ALLOC(sizeof(rbac_role_t));
  if (!role) {
    LOG_ERROR("Cannot allocate memory for role.");
    /* CHECKPOINT: json_free(insert_result); */
    return NULL;
  }
  
  role->id = BUFFER_STRDUP(actual_id);
  role->name = BUFFER_STRDUP(rolename);
  role->permissions = json_create_object();
  
  /* CHECKPOINT: json_free(insert_result); */
  
  TRACE_RBAC("Role created with ID: %s", role->id);
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
  
  TRACE_RBAC("Checking permission - user: %s, resource: %s:%s, permission: %s",
       user_id, resource_type_str, resource_id, permission_str);
  
  if (!db || !user_id || !resource_id) {
    LOG_ERROR("Invalid parameters.");
    return 0;
  }
  
  /* First check permission cache */
  char cache_key[256];
  snprintf(cache_key, sizeof(cache_key), "%s:%s:%s", user_id, resource_type_str, resource_id);
  
  json_value_t* cache_query = json_create_object();
  json_object_set(cache_query, "type", json_create_string("permission_cache"));
  json_object_set(cache_query, "library", json_create_string(RBAC_LIBRARY_SYSTEM));
  json_object_set(cache_query, "uuid", json_create_string(cache_key));
  
  json_value_t* cache_result = virtual_query(db, "permission_cache", RBAC_LIBRARY_SYSTEM, "cache", cache_query);
  /* CHECKPOINT: json_free(cache_query); */
  
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
          TRACE_RBAC("Using cached permissions.");
          json_value_t* perms = json_object_get(cache_doc, "permissions");
          
          if (perms && perms->type == JSON_ARRAY) {
            for (size_t i = 0; i < perms->value.array.size; i++) {
              json_value_t* perm = perms->value.array.items[i];
              if (perm->type == JSON_STRING && 
                strcmp(perm->value.string, permission_str) == 0) {
                TRACE_RBAC("Permission granted (cached).");
                /* CHECKPOINT: json_free(cache_result); */
                return 1;
              }
            }
          }
          
          TRACE_RBAC("Permission denied (cached).");
          /* CHECKPOINT: json_free(cache_result); */
          return 0;
        }
      }
    }
    /* CHECKPOINT: json_free(cache_result); */
  }
  
  /* Cache miss or expired - compute permissions */
  TRACE_RBAC("Computing permissions from database.");
  
  /* TODO: Implement full permission resolution algorithm */
  /* For now, just check if user has admin role */
  json_value_t* user_query = rbac_build_user_query(RBAC_LIBRARY_SYSTEM);
  json_object_set(user_query, "uuid", json_create_string(user_id));
  
  TRACE_RBAC("Querying user with _id: %s", user_id);
  
  json_value_t* user_result = virtual_query(db, DOC_TYPE_NAME_USER, RBAC_LIBRARY_SYSTEM, VIRTUAL_COLLECTION_USERS, user_query);
  /* CHECKPOINT: json_free(user_query); */
  
  if (!user_result) {
    LOG_ERROR("Cannot query user.");
    return 0;
  }
  
  json_value_t* user_docs = json_object_get(user_result, "documents");
  if (!user_docs || user_docs->value.array.size == 0) {
    LOG_ERROR("User not found with id: %s", user_id);
    TRACE_RBAC("Query result: %s", json_stringify(user_result));
    /* CHECKPOINT: json_free(user_result); */
    return 0;
  }
  
  TRACE_RBAC("Found %zu users", user_docs->value.array.size);
  
  /* Get user's roles and check permissions */
  json_value_t* user_doc = user_docs->value.array.items[0];
  json_value_t* user_roles = json_object_get(user_doc, "roles");
  
  int has_permission = 0;
  
  if (user_roles && user_roles->type == JSON_ARRAY) {
    TRACE_RBAC("User has %zu roles", user_roles->value.array.size);
    
    /* Check each role's permissions */
    for (size_t i = 0; i < user_roles->value.array.size; i++) {
      json_value_t* role_id_val = user_roles->value.array.items[i];
      if (role_id_val->type != JSON_STRING) continue;
      
      const char* role_id = role_id_val->value.string;
      TRACE_RBAC("Checking role: %s", role_id);
      
      /* Query the role */
      json_value_t* role_doc = db_get_document(db, RBAC_SYSTEM_LIBRARY, RBAC_ROLES_COLLECTION_NAME, role_id);
      if (!role_doc) {
        TRACE_RBAC("Role not found: %s", role_id);
        continue;
      }
      
      /* Check role permissions */
      json_value_t* permissions = json_object_get(role_doc, "permissions");
      if (permissions) {
        /* Build permission key: resource_type:resource_id */
        char perm_key[64];
        snprintf(perm_key, sizeof(perm_key), "%d:%s", (int)resource_type, resource_id);
        
        TRACE_RBAC("Looking for permission key: %s", perm_key);
        
        /* Check exact match first */
        json_value_t* perm_value = json_object_get(permissions, perm_key);
        if (perm_value && perm_value->type == JSON_NUMBER) {
          int perm_mask = (int)perm_value->value.number;
          TRACE_RBAC("Found exact permission: %d (checking for %d)", perm_mask, permission);
          if (perm_mask & permission) {
            has_permission = 1;
            /* CHECKPOINT: json_free(role_doc); */
            break;
          }
        }
        
        /* Check wildcard permission */
        snprintf(perm_key, sizeof(perm_key), "%d:*", (int)resource_type);
        perm_value = json_object_get(permissions, perm_key);
        if (perm_value && perm_value->type == JSON_NUMBER) {
          int perm_mask = (int)perm_value->value.number;
          TRACE_RBAC("Found wildcard permission: %d (checking for %d)", perm_mask, permission);
          if (perm_mask & permission) {
            has_permission = 1;
            /* CHECKPOINT: json_free(role_doc); */
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
                TRACE_RBAC("User has system:* permission.");
                has_permission = 1;
                break;
              }
            }
          }
        }
      }
      
      /* CHECKPOINT: json_free(role_doc); */
      
      if (has_permission) break;
    }
  } else {
    TRACE_RBAC("User has no roles.");
  }
  
  /* CHECKPOINT: json_free(user_result); */
  
  TRACE_RBAC("Permission %s", has_permission ? "granted" : "denied");
  return has_permission;
}