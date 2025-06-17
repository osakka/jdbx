#ifndef VIRTUAL_LAYER_H
#define VIRTUAL_LAYER_H

#include "database/database.h"
#include "utils/json.h"
#include <time.h>

/**
 * Virtual Layer - Business Logic Interface
 * 
 * This is the ONLY interface that application code should use for
 * database operations. It provides high-level business logic operations
 * that automatically handle:
 * 
 * - Document type discrimination
 * - UUID generation and standardization
 * - Field validation and normalization
 * - Timestamp management
 * - Library/namespace isolation
 * 
 * CRITICAL: Never bypass this layer to access storage directly!
 */

/* REMOVED: UUID generation handled by generic virtual_insert in database.c (single source of truth) */

/* ===== USER MANAGEMENT ===== */

/**
 * Create a new user
 * @param db Database instance
 * @param username Username (must be unique within library)
 * @param password_hash Hashed password
 * @param library Library namespace (NULL for "default")
 * @return Created user document or NULL on error
 */
json_value_t* virtual_create_user(database_t* db, const char* username, 
                                 const char* password_hash, const char* library);

/**
 * Get user by UUID
 * @param db Database instance
 * @param uuid User's UUID
 * @return User document or NULL if not found
 */
json_value_t* virtual_get_user_by_uuid(database_t* db, const char* uuid);

/**
 * Get user by username
 * @param db Database instance
 * @param username Username
 * @param library Library namespace (NULL for "default")
 * @return User document or NULL if not found
 */
json_value_t* virtual_get_user_by_name(database_t* db, const char* username, 
                                      const char* library);

/**
 * Update user
 * @param db Database instance
 * @param uuid User's UUID
 * @param updates JSON object with fields to update
 * @return Updated user document or NULL on error
 */
json_value_t* virtual_update_user(database_t* db, const char* uuid, 
                                 json_value_t* updates);

/**
 * Delete user
 * @param db Database instance
 * @param uuid User's UUID
 * @return 1 on success, 0 on failure
 */
int virtual_delete_user(database_t* db, const char* uuid);

/* REMOVED: virtual_query_users_new - using virtual_query_users from database.c (single source of truth) */

/* ===== ROLE MANAGEMENT ===== */

/**
 * Create a new role
 * @param db Database instance
 * @param name Role name (must be unique within library)
 * @param library Library namespace (NULL for "default")
 * @return Created role document or NULL on error
 */
json_value_t* virtual_create_role(database_t* db, const char* name, 
                                 const char* library);

/**
 * Get role by UUID
 * @param db Database instance
 * @param uuid Role's UUID
 * @return Role document or NULL if not found
 */
json_value_t* virtual_get_role_by_uuid(database_t* db, const char* uuid);

/**
 * Get role by name
 * @param db Database instance
 * @param name Role name
 * @param library Library namespace (NULL for "default")
 * @return Role document or NULL if not found
 */
json_value_t* virtual_get_role_by_name(database_t* db, const char* name, 
                                      const char* library);

/**
 * Update role
 * @param db Database instance
 * @param uuid Role's UUID
 * @param updates JSON object with fields to update
 * @return Updated role document or NULL on error
 */
json_value_t* virtual_update_role(database_t* db, const char* uuid, 
                                 json_value_t* updates);

/**
 * Delete role
 * @param db Database instance
 * @param uuid Role's UUID
 * @return 1 on success, 0 on failure
 */
int virtual_delete_role(database_t* db, const char* uuid);

/**
 * Query roles
 * @param db Database instance
 * @param filters Optional filters (library, etc.)
 * @return Query result with documents array
 */
json_value_t* virtual_query_roles(database_t* db, json_value_t* filters);

/* ===== SESSION MANAGEMENT ===== */

/**
 * Create a new session
 * @param db Database instance
 * @param user_uuid User's UUID
 * @param token Session token
 * @param expires_at Expiration time
 * @param ip_address Client IP address (optional)
 * @param user_agent Client user agent (optional)
 * @return Created session document or NULL on error
 */
json_value_t* virtual_create_session(database_t* db, const char* user_uuid, 
                                    const char* token, time_t expires_at,
                                    const char* ip_address, const char* user_agent);

/**
 * Get session by token
 * @param db Database instance
 * @param token Session token
 * @return Session document or NULL if not found
 */
json_value_t* virtual_get_session_by_token(database_t* db, const char* token);

/**
 * Get session by UUID
 * @param db Database instance
 * @param uuid Session's UUID
 * @return Session document or NULL if not found
 */
json_value_t* virtual_get_session_by_uuid(database_t* db, const char* uuid);

/**
 * Update session
 * @param db Database instance
 * @param uuid Session's UUID
 * @param updates JSON object with fields to update
 * @return Updated session document or NULL on error
 */
json_value_t* virtual_update_session(database_t* db, const char* uuid, 
                                    json_value_t* updates);

/**
 * Delete session
 * @param db Database instance
 * @param uuid Session's UUID
 * @return 1 on success, 0 on failure
 */
int virtual_delete_session(database_t* db, const char* uuid);

/**
 * Invalidate all sessions for a user
 * @param db Database instance
 * @param user_uuid User's UUID
 * @return Number of sessions invalidated
 */
int virtual_invalidate_user_sessions(database_t* db, const char* user_uuid);

/* ===== USER-ROLE MANAGEMENT ===== */

/**
 * Add role to user
 * @param db Database instance
 * @param user_uuid User's UUID
 * @param role_uuid Role's UUID
 * @return 1 on success, 0 on failure
 */
int virtual_add_role_to_user(database_t* db, const char* user_uuid, 
                            const char* role_uuid);

/**
 * Remove role from user
 * @param db Database instance
 * @param user_uuid User's UUID
 * @param role_uuid Role's UUID
 * @return 1 on success, 0 on failure
 */
int virtual_remove_role_from_user(database_t* db, const char* user_uuid, 
                                 const char* role_uuid);

/**
 * Get all roles for a user
 * @param db Database instance
 * @param user_uuid User's UUID
 * @return Array of role documents or NULL
 */
json_value_t* virtual_get_user_roles(database_t* db, const char* user_uuid);

/**
 * Get all users with a role
 * @param db Database instance
 * @param role_uuid Role's UUID
 * @return Array of user documents or NULL
 */
json_value_t* virtual_get_role_users(database_t* db, const char* role_uuid);

/* ===== PERMISSION MANAGEMENT ===== */

/**
 * Grant permission to role
 * @param db Database instance
 * @param role_uuid Role's UUID
 * @param resource Resource identifier
 * @param permission Permission string
 * @return 1 on success, 0 on failure
 */
int virtual_grant_permission(database_t* db, const char* role_uuid, 
                           const char* resource, const char* permission);

/**
 * Revoke permission from role
 * @param db Database instance
 * @param role_uuid Role's UUID
 * @param resource Resource identifier
 * @param permission Permission string
 * @return 1 on success, 0 on failure
 */
int virtual_revoke_permission(database_t* db, const char* role_uuid, 
                            const char* resource, const char* permission);

/**
 * Check if user has permission
 * @param db Database instance
 * @param user_uuid User's UUID
 * @param resource Resource identifier
 * @param permission Permission string
 * @return 1 if user has permission, 0 otherwise
 */
int virtual_check_permission(database_t* db, const char* user_uuid, 
                           const char* resource, const char* permission);

#endif /* VIRTUAL_LAYER_H */