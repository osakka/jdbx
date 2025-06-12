#ifndef RBAC_DATABASE_H
#define RBAC_DATABASE_H

#include "rbac/rbac.h"

/**
 * Initialize RBAC system with database backend
 * This replaces the minimal in-memory implementation with full database persistence
 * 
 * @param db Database instance
 * @param jwt_secret Secret key for JWT token generation
 * @return Initialized RBAC system or NULL on failure
 */
rbac_system_t* rbac_database_init(struct database* db, const char* jwt_secret);

/**
 * Create default admin role in database
 * @param db Database instance
 * @param admin_role_id_out Output parameter for the created role ID (optional)
 * @return 1 on success, 0 on failure
 */
int create_default_admin_role(struct database* db, char** admin_role_id_out);

/**
 * Create default admin user in database
 * @param db Database instance
 * @param admin_role_id Role ID to assign to the user
 * @return 1 on success, 0 on failure
 */
int create_default_admin_user(struct database* db, const char* admin_role_id);

/**
 * Get user by username from database
 * @param db Database instance
 * @param username Username to search for
 * @return User structure or NULL if not found
 */
rbac_user_t* rbac_database_get_user_by_username(struct database* db, const char* username);

/**
 * Create new user in database
 * @param db Database instance
 * @param username Username
 * @param password Plain text password (will be hashed)
 * @return User structure or NULL on failure
 */
rbac_user_t* rbac_database_create_user(struct database* db, const char* username, const char* password);

/**
 * Create new role in database
 * @param db Database instance
 * @param rolename Role name
 * @param description Role description (optional)
 * @return Role structure or NULL on failure
 */
rbac_role_t* rbac_database_create_role(struct database* db, const char* rolename, const char* description);

/**
 * Check if user has permission on a resource
 * @param db Database instance
 * @param user_id User ID
 * @param resource_type Type of resource (collection/document)
 * @param resource_id Resource identifier
 * @param permission Permission to check (CREATE/READ/UPDATE/DELETE/ADMIN)
 * @return 1 if permission granted, 0 if denied
 */
int rbac_database_check_permission(struct database* db, const char* user_id, rbac_resource_type_t resource_type,
                           const char* resource_id, rbac_permission_t permission);

/**
 * Add user to role
 * @param db Database instance
 * @param user_id User ID
 * @param role_id Role ID
 * @return 1 on success, 0 on failure
 */
int rbac_db_add_user_to_role(struct database* db, const char* user_id, const char* role_id);

/**
 * Create new role
 * @param db Database instance
 * @param name Role name
 * @param description Role description
 * @return Role ID on success, NULL on failure
 */
char* rbac_db_create_role(struct database* db, const char* name, const char* description);

/**
 * Grant permission to role on collection
 * @param db Database instance
 * @param role_id Role ID
 * @param collection Collection name
 * @param permissions Array of permission strings
 * @param num_permissions Number of permissions
 * @return 1 on success, 0 on failure
 */
int rbac_db_grant_role_permission(struct database* db, const char* role_id, const char* collection,
                                const char** permissions, size_t num_permissions);

/**
 * Create user session
 * @param db Database instance
 * @param user_id User ID
 * @param token JWT token
 * @param expires_at Expiration timestamp
 * @param ip_address Client IP address
 * @param user_agent Client user agent
 * @return Session ID on success, NULL on failure
 */
char* rbac_db_create_session(struct database* db, const char* user_id, const char* token,
                           time_t expires_at, const char* ip_address, const char* user_agent);

/**
 * Validate session
 * @param db Database instance
 * @param token JWT token
 * @return User ID if session valid, NULL otherwise
 */
char* rbac_db_validate_session(struct database* db, const char* token);

/**
 * Revoke session
 * @param db Database instance
 * @param session_id Session ID
 * @return 1 on success, 0 on failure
 */
int rbac_db_revoke_session(struct database* db, const char* session_id);

/**
 * Invalidate session
 * @param db Database instance
 * @param session_id Session ID
 * @return 1 on success, 0 on failure
 */
int rbac_db_invalidate_session(struct database* db, const char* session_id);

/**
 * Create collection with owner
 * @param db Database instance
 * @param name Collection name
 * @param owner_id Owner user ID
 * @return 1 on success, 0 on failure
 */
int rbac_db_create_collection(struct database* db, const char* name, const char* owner_id);

/**
 * Get collection owner
 * @param db Database instance
 * @param collection Collection name
 * @return Owner user ID or NULL
 */
char* rbac_db_get_collection_owner(struct database* db, const char* collection);

/**
 * Transfer collection ownership
 * @param db Database instance
 * @param collection Collection name
 * @param new_owner_id New owner user ID
 * @return 1 on success, 0 on failure
 */
int rbac_db_transfer_collection_ownership(struct database* db, const char* collection, const char* new_owner_id);

/**
 * Invalidate permission cache for user
 * @param db Database instance
 * @param user_id User ID
 * @return 1 on success, 0 on failure
 */
int rbac_db_invalidate_permission_cache(struct database* db, const char* user_id);

#endif /* RBAC_DATABASE_H */