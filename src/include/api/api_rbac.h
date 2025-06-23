/**
 * @file api_rbac.h
 * @brief RBAC (Role-Based Access Control) API handlers
 * 
 * Provides RESTful API endpoints for user and role management operations.
 * Implements comprehensive user lifecycle management, role administration,
 * and permission system integration with unified documents architecture.
 */

#ifndef API_RBAC_H
#define API_RBAC_H

#include "api/api.h"

/**
 * @defgroup RBAC_API User and Role Management API
 * @brief Complete user and role management system
 * 
 * Provides comprehensive user lifecycle management, role administration,
 * and permission system integration. All operations use unified documents
 * architecture with proper authentication and authorization checks.
 * 
 * @{
 */

/**
 * @defgroup RBAC_Users User Management
 * @brief User lifecycle operations
 * @{
 */

/**
 * @brief Get list of all users
 * 
 * Retrieves all users in the system with filtering and pagination support.
 * Returns user information without sensitive data like password hashes.
 * 
 * @param ctx API context with database and RBAC system
 * @param request HTTP request (query parameters for filtering)
 * @return HTTP response with users array
 * 
 * @note Requires admin privileges
 * @note Response format: {"users": [...], "count": N}
 */
http_response_t* api_handle_users_list(api_context_t* ctx, http_request_t* request);

/**
 * @brief Get specific user by ID
 * 
 * Retrieves detailed information for a specific user including
 * roles, permissions, and account status.
 * 
 * @param ctx API context with database and RBAC system
 * @param request HTTP request with user ID in path
 * @return HTTP response with user details
 * 
 * @note Users can access their own information, admins can access any user
 * @note Response excludes sensitive authentication data
 */
http_response_t* api_handle_user_get(api_context_t* ctx, http_request_t* request);

/**
 * @brief Create new user
 * 
 * Creates a new user account with specified username, password, and
 * optional role assignments. Validates uniqueness and complexity requirements.
 * 
 * @param ctx API context with database and RBAC system
 * @param request HTTP request with user creation data
 * @return HTTP response with created user information
 * 
 * @note Requires admin privileges for creation
 * @note Password complexity rules enforced
 * @note Automatic role assignment based on system configuration
 */
http_response_t* api_handle_user_create(api_context_t* ctx, http_request_t* request);

/**
 * @brief Update existing user
 * 
 * Updates user information including profile data, role assignments,
 * and account status. Supports partial updates and field-level validation.
 * 
 * @param ctx API context with database and RBAC system
 * @param request HTTP request with user ID and update data
 * @return HTTP response with updated user information
 * 
 * @note Users can update their own profile, admins can update any user
 * @note Password changes require current password validation
 */
http_response_t* api_handle_user_update(api_context_t* ctx, http_request_t* request);

/**
 * @brief Delete user account
 * 
 * Permanently removes user account and associated data. Includes
 * cascade deletion of sessions, permissions, and user-owned resources.
 * 
 * @param ctx API context with database and RBAC system
 * @param request HTTP request with user ID in path
 * @return HTTP response confirming deletion
 * 
 * @note Requires admin privileges
 * @note Cannot delete currently authenticated user
 * @note Includes safety checks for critical system users
 */
http_response_t* api_handle_user_delete(api_context_t* ctx, http_request_t* request);

/** @} */ /* End of RBAC_Users group */

/**
 * @defgroup RBAC_Roles Role Management
 * @brief Role administration and permission management
 * @{
 */

/**
 * @brief Get list of all roles
 * 
 * Retrieves all roles in the system with associated permissions
 * and user count information. Supports filtering and pagination.
 * 
 * @param ctx API context with database and RBAC system
 * @param request HTTP request (query parameters for filtering)
 * @return HTTP response with roles array
 * 
 * @note Available to all authenticated users for role selection
 * @note Response format: {"roles": [...], "count": N}
 */
http_response_t* api_handle_roles_list(api_context_t* ctx, http_request_t* request);

/**
 * @brief Get specific role by ID
 * 
 * Retrieves detailed role information including permissions,
 * assigned users, and role hierarchy relationships.
 * 
 * @param ctx API context with database and RBAC system
 * @param request HTTP request with role ID in path
 * @return HTTP response with role details
 * 
 * @note Available to all authenticated users
 * @note Includes comprehensive permission mapping
 */
http_response_t* api_handle_role_get(api_context_t* ctx, http_request_t* request);

/**
 * @brief Create new role
 * 
 * Creates a new role with specified name, description, and
 * permission set. Validates role name uniqueness and permission validity.
 * 
 * @param ctx API context with database and RBAC system
 * @param request HTTP request with role creation data
 * @return HTTP response with created role information
 * 
 * @note Requires admin privileges
 * @note Permission validation against system capabilities
 * @note Automatic audit trail creation
 */
http_response_t* api_handle_role_create(api_context_t* ctx, http_request_t* request);

/**
 * @brief Update existing role
 * 
 * Updates role information including name, description, permissions,
 * and user assignments. Supports incremental permission updates.
 * 
 * @param ctx API context with database and RBAC system
 * @param request HTTP request with role ID and update data
 * @return HTTP response with updated role information
 * 
 * @note Requires admin privileges
 * @note Cannot modify system-critical roles
 * @note Automatic permission inheritance calculation
 */
http_response_t* api_handle_role_update(api_context_t* ctx, http_request_t* request);

/**
 * @brief Delete role
 * 
 * Permanently removes role and updates all assigned users.
 * Includes validation to prevent deletion of critical system roles.
 * 
 * @param ctx API context with database and RBAC system
 * @param request HTTP request with role ID in path
 * @return HTTP response confirming deletion
 * 
 * @note Requires admin privileges
 * @note Cannot delete roles with active user assignments (safety check)
 * @note Automatic user permission recalculation
 */
http_response_t* api_handle_role_delete(api_context_t* ctx, http_request_t* request);

/** @} */ /* End of RBAC_Roles group */

/** @} */ /* End of RBAC_API group */

#endif /* API_RBAC_H */