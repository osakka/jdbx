#ifndef RBAC_DB_H
#define RBAC_DB_H

#include "rbac/rbac.h"
#include "database/database.h"

/* System collection for storing configuration */
#define RBAC_CONFIG_COLLECTION "_system"

/* RBAC-specific collections */
#define RBAC_USERS_COLLECTION "_users"
#define RBAC_ROLES_COLLECTION "_roles"

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
rbac_db_status_t rbac_db_init_collections(database_t* db);

/**
 * Load RBAC system from database
 *
 * Loads users, roles, and permissions from the database collections.
 *
 * @param db Database instance
 * @return Initialized RBAC system or NULL on failure
 */
rbac_system_t* rbac_db_load(database_t* db);

/**
 * Save RBAC system to database
 *
 * Saves users, roles, and permissions to the database collections.
 *
 * @param db Database instance
 * @param rbac RBAC system to save
 * @return 1 on success, 0 on failure
 */
int rbac_db_save(database_t* db, rbac_system_t* rbac);

/**
 * Check if RBAC system exists in database
 *
 * @param db Database instance
 * @return 1 if RBAC is initialized in the database, 0 otherwise
 */
int rbac_db_exists(database_t* db);

/**
 * Create a user in the database
 *
 * @param db Database instance
 * @param username Username for the new user
 * @param password Password for the new user
 * @return The created user on success, NULL on failure
 */
rbac_user_t* rbac_db_create_user(database_t* db, const char* username, const char* password);

/**
 * Delete a user from the database
 *
 * @param db Database instance
 * @param user_id ID of the user to delete
 * @return 1 on success, 0 on failure
 */
int rbac_db_delete_user(database_t* db, const char* user_id);

/**
 * Get a user from the database by ID
 *
 * @param db Database instance
 * @param user_id ID of the user to retrieve
 * @return The user on success, NULL on failure
 */
rbac_user_t* rbac_db_get_user(database_t* db, const char* user_id);

/**
 * Get a user from the database by username
 *
 * @param db Database instance
 * @param username Username of the user to retrieve
 * @return The user on success, NULL on failure
 */
rbac_user_t* rbac_db_get_user_by_username(database_t* db, const char* username);

/**
 * Create a role in the database
 *
 * @param db Database instance
 * @param name Name for the new role
 * @return The created role on success, NULL on failure
 */
rbac_role_t* rbac_db_create_role(database_t* db, const char* name);

/**
 * Delete a role from the database
 *
 * @param db Database instance
 * @param role_id ID of the role to delete
 * @return 1 on success, 0 on failure
 */
int rbac_db_delete_role(database_t* db, const char* role_id);

/**
 * Get a role from the database by ID
 *
 * @param db Database instance
 * @param role_id ID of the role to retrieve
 * @return The role on success, NULL on failure
 */
rbac_role_t* rbac_db_get_role(database_t* db, const char* role_id);

/**
 * Add a user to a role in the database
 *
 * @param db Database instance
 * @param user_id ID of the user
 * @param role_id ID of the role
 * @return 1 on success, 0 on failure
 */
int rbac_db_add_user_to_role(database_t* db, const char* user_id, const char* role_id);

/**
 * Remove a user from a role in the database
 *
 * @param db Database instance
 * @param user_id ID of the user
 * @param role_id ID of the role
 * @return 1 on success, 0 on failure
 */
int rbac_db_remove_user_from_role(database_t* db, const char* user_id, const char* role_id);

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
int rbac_db_grant_permission(database_t* db, const char* role_id, rbac_resource_type_t resource_type,
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
int rbac_db_revoke_permission(database_t* db, const char* role_id, rbac_resource_type_t resource_type,
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
int rbac_db_check_permission(database_t* db, const char* user_id, rbac_resource_type_t resource_type,
                            const char* resource_id, rbac_permission_t permission);

/**
 * Authenticate a user in the database
 *
 * @param db Database instance
 * @param username Username of the user
 * @param password Password of the user
 * @return 1 if authentication is successful, 0 otherwise
 */
int rbac_db_authenticate_user(database_t* db, const char* username, const char* password);

/**
 * Migrate RBAC from file to database
 * 
 * @param db Database instance
 * @param rbac RBAC system loaded from file
 * @return 1 on success, 0 on failure
 */
int rbac_db_migrate_from_file(database_t* db, rbac_system_t* rbac);

#endif /* RBAC_DB_H */