#ifndef RBAC_DB_H
#define RBAC_DB_H

#include "rbac/rbac.h"
#include <time.h>

/* RBAC system library and collection names - SINGLE SOURCE OF TRUTH */
#define RBAC_SYSTEM_LIBRARY "system"
#define RBAC_DEFAULT_LIBRARY "default"
#define RBAC_CONFIG_COLLECTION_NAME "config"
#define RBAC_USERS_COLLECTION_NAME "users"  
#define RBAC_ROLES_COLLECTION_NAME "roles"
#define RBAC_SESSIONS_COLLECTION_NAME "sessions"
#define RBAC_PERMISSIONS_COLLECTION_NAME "permissions"
#define RBAC_COLLECTIONS_COLLECTION_NAME "collections"
#define RBAC_PERMISSION_CACHE_COLLECTION_NAME "permission_cache"
#define RBAC_DOCUMENTS_COLLECTION_NAME "documents"
#define RBAC_LIBRARIES_COLLECTION_NAME "libraries"

/* Collection creation/upgrade status */
typedef struct {
    int success;
    int collections_created;
    int indexes_created;
    char* error_message;
} rbac_db_status_t;

/* RBAC database function prototypes */

/**
 * Initialize RBAC collections in the database
 *
 * This will create the necessary collections and indexes for RBAC if they don't exist.
 *
 * @param db Database instance
 * @return Status of the operation
 */
rbac_db_status_t rbac_db_init_collections(struct database* db);

/**
 * Load RBAC system from database
 *
 * Loads users, roles, and permissions from the database collections.
 *
 * @param db Database instance
 * @return Initialized RBAC system or NULL on failure
 */
rbac_system_t* rbac_db_load(struct database* db);

/**
 * Save RBAC system to database
 *
 * Saves users, roles, and permissions to the database collections.
 *
 * @param db Database instance
 * @param rbac RBAC system to save
 * @return 1 on success, 0 on failure
 */
int rbac_db_save(struct database* db, rbac_system_t* rbac);

/**
 * Fixed implementation of RBAC system save that avoids hanging
 * This version doesn't try to clear all existing data first, which was causing
 * a potential deadlock in the database operations.
 * 
 * @param db Database instance
 * @param rbac RBAC system to save
 * @return 1 on success, 0 on failure
 */
int rbac_database_persist(struct database* db, rbac_system_t* rbac);

/**
 * Check if RBAC system exists in database
 *
 * @param db Database instance
 * @return 1 if RBAC is initialized in the database, 0 otherwise
 */
int rbac_db_exists(struct database* db);

/**
 * Create a user in the database
 *
 * @param db Database instance
 * @param username Username for the new user
 * @param password Password for the new user
 * @return The created user on success, NULL on failure
 */
rbac_user_t* rbac_db_create_user(struct database* db, const char* username, const char* password);

/**
 * Delete a user from the database
 *
 * @param db Database instance
 * @param user_id ID of the user to delete
 * @return 1 on success, 0 on failure
 */
int rbac_db_delete_user(struct database* db, const char* user_id);

/**
 * Get a user from the database by ID
 *
 * @param db Database instance
 * @param user_id ID of the user to retrieve
 * @return The user on success, NULL on failure
 */
rbac_user_t* rbac_db_get_user(struct database* db, const char* user_id);

/**
 * Get a user from the database by username
 *
 * @param db Database instance
 * @param username Username of the user to retrieve
 * @return The user on success, NULL on failure
 */
rbac_user_t* rbac_db_get_user_by_username(struct database* db, const char* username);

/**
 * Create a role in the database
 *
 * @param db Database instance
 * @param name Name for the new role
 * @return The created role on success, NULL on failure
 */
rbac_role_t* rbac_db_create_role(struct database* db, const char* name);

/**
 * Delete a role from the database
 *
 * @param db Database instance
 * @param role_id ID of the role to delete
 * @return 1 on success, 0 on failure
 */
int rbac_db_delete_role(struct database* db, const char* role_id);

/**
 * Update a role in the database
 *
 * @param db Database instance
 * @param role_id ID of the role to update
 * @param name New name for the role (can be NULL to keep existing)
 * @param permissions New permissions for the role (can be NULL to keep existing)
 * @return 1 on success, 0 on failure
 */
int rbac_db_update_role(struct database* db, const char* role_id, const char* name, json_value_t* permissions);

/**
 * Get a role from the database by ID
 *
 * @param db Database instance
 * @param role_id ID of the role to retrieve
 * @return The role on success, NULL on failure
 */
rbac_role_t* rbac_db_get_role(struct database* db, const char* role_id);

/**
 * Add a user to a role in the database
 *
 * @param db Database instance
 * @param user_id ID of the user
 * @param role_id ID of the role
 * @return 1 on success, 0 on failure
 */
int rbac_db_add_user_to_role(struct database* db, const char* user_id, const char* role_id);

/**
 * Remove a user from a role in the database
 *
 * @param db Database instance
 * @param user_id ID of the user
 * @param role_id ID of the role
 * @return 1 on success, 0 on failure
 */
int rbac_db_remove_user_from_role(struct database* db, const char* user_id, const char* role_id);

/**
 * Grant a permission to a role in the database
 *
 * @param db Database instance
 * @param role_id ID of the role
 * @param resource_type Type of the resource
 * @param resource_id ID of the resource
 * @param permission Permission to grant
 * @return 1 on success, 0 on failure
 */
int rbac_db_grant_permission(struct database* db, const char* role_id, rbac_resource_type_t resource_type,
                            const char* resource_id, rbac_permission_t permission);

/**
 * Revoke a permission from a role in the database
 *
 * @param db Database instance
 * @param role_id ID of the role
 * @param resource_type Type of the resource
 * @param resource_id ID of the resource
 * @param permission Permission to revoke
 * @return 1 on success, 0 on failure
 */
int rbac_db_revoke_permission(struct database* db, const char* role_id, rbac_resource_type_t resource_type,
                             const char* resource_id, rbac_permission_t permission);

/**
 * Check if a user has a permission for a resource in the database
 *
 * @param db Database instance
 * @param user_id ID of the user
 * @param resource_type Type of the resource
 * @param resource_id ID of the resource
 * @param permission Permission to check
 * @return 1 if the user has the permission, 0 otherwise
 */
int rbac_db_check_permission(struct database* db, const char* user_id, rbac_resource_type_t resource_type,
                            const char* resource_id, rbac_permission_t permission);

/**
 * Authenticate a user in the database
 *
 * @param db Database instance
 * @param username Username of the user
 * @param password Password of the user
 * @return 1 if authentication is successful, 0 otherwise
 */
int rbac_db_authenticate_user(struct database* db, const char* username, const char* password);

/**
 * DEPRECATED: Migrate RBAC from file to database
 * This function is no longer used as we've removed file-based RBAC fallback.
 * It is kept here for API compatibility only, but will be removed in a future release.
 * 
 * @param db Database instance
 * @param rbac RBAC system loaded from file
 * @return 1 on success, 0 on failure (always returns 0 now)
 */
int rbac_db_migrate_from_file(struct database* db, rbac_system_t* rbac);

/* Bootstrap functions for admin setup */
int create_default_admin_role(struct database* db, char** admin_role_id_out);
/* NOTE: create_default_admin_user is declared in rbac_database.h - single source of truth */

/* Session management functions */
char* rbac_db_create_session(struct database* db, const char* user_id, const char* token,
                             time_t expires_at, const char* ip_address, const char* user_agent);
char* rbac_db_validate_session(struct database* db, const char* token);
int rbac_db_revoke_session(struct database* db, const char* session_id);
int rbac_db_invalidate_session(struct database* db, const char* session_id);
int rbac_db_invalidate_sessions_by_token(struct database* db, const char* token);

#endif /* RBAC_DB_H */