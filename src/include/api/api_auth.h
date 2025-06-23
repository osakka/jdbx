/**
 * @file api_auth.h
 * @brief Authentication and session management API handlers
 * 
 * This module consolidates all authentication-related API endpoints including:
 * - User authentication (login, logout, register)
 * - Session management (list, terminate, get current)
 * - Token operations (refresh, validation)
 * - Library context switching
 * - Password management
 * 
 * All handlers follow the unified documents architecture and use the
 * checkpoint-based memory management system.
 */

#ifndef API_AUTH_H
#define API_AUTH_H

#include "api/api.h"

/**
 * @defgroup auth_core Core Authentication
 * @{
 */

/**
 * Handle user login
 * 
 * Authenticates user credentials and generates JWT token.
 * Supports library-scoped authentication with username@library format.
 * 
 * Request body:
 * ```json
 * {
 *   "username": "string",
 *   "password": "string",
 *   "library": "string" (optional)
 * }
 * ```
 * 
 * Response:
 * ```json
 * {
 *   "token": "jwt_token_string",
 *   "expires_in": 3600,
 *   "user": {
 *     "id": "user_uuid",
 *     "username": "string",
 *     "library": "string"
 *   }
 * }
 * ```
 * 
 * @param ctx API context with database and RBAC
 * @param request HTTP request with credentials
 * @return HTTP response with JWT token or error
 */
http_response_t* api_handle_login(api_context_t* ctx, http_request_t* request);

/**
 * Handle user registration
 * 
 * Creates new user account with specified credentials.
 * Automatically assigns default roles based on configuration.
 * 
 * Request body:
 * ```json
 * {
 *   "username": "string",
 *   "password": "string",
 *   "email": "string" (optional),
 *   "library": "string" (optional)
 * }
 * ```
 * 
 * @param ctx API context with database and RBAC
 * @param request HTTP request with registration data
 * @return HTTP response with success or error
 */
http_response_t* api_handle_register(api_context_t* ctx, http_request_t* request);

/**
 * Handle token refresh
 * 
 * Generates new JWT token from valid existing token.
 * Extends session without requiring re-authentication.
 * 
 * Request headers:
 * - Authorization: Bearer <existing_token>
 * 
 * @param ctx API context with database and RBAC
 * @param request HTTP request with current token
 * @return HTTP response with new JWT token or error
 */
http_response_t* api_handle_token_refresh(api_context_t* ctx, http_request_t* request);

/**
 * Handle user logout
 * 
 * Terminates current session and invalidates token.
 * 
 * Request headers:
 * - Authorization: Bearer <token>
 * 
 * @param ctx API context with database and RBAC
 * @param request HTTP request with token
 * @return HTTP response with success or error
 */
http_response_t* api_handle_logout(api_context_t* ctx, http_request_t* request);

/** @} */

/**
 * @defgroup session_mgmt Session Management
 * @{
 */

/**
 * Get current session information
 * 
 * Returns details about the authenticated user's current session.
 * 
 * @param ctx API context with database and RBAC
 * @param request HTTP request with authentication
 * @return HTTP response with session details
 */
http_response_t* api_handle_get_current_session(api_context_t* ctx, http_request_t* request);

/**
 * Get all sessions for current user
 * 
 * Lists all active sessions for the authenticated user.
 * 
 * @param ctx API context with database and RBAC
 * @param request HTTP request with authentication
 * @return HTTP response with session list
 */
http_response_t* api_handle_get_sessions(api_context_t* ctx, http_request_t* request);

/**
 * Get all active sessions (admin only)
 * 
 * Lists all active sessions across all users.
 * Requires administrative privileges.
 * 
 * @param ctx API context with database and RBAC
 * @param request HTTP request with admin authentication
 * @return HTTP response with all active sessions
 */
http_response_t* api_handle_get_active_sessions(api_context_t* ctx, http_request_t* request);

/**
 * Terminate specific session (POST method)
 * 
 * Terminates a session by ID. Users can terminate their own sessions,
 * admins can terminate any session.
 * 
 * URL: /api/sessions/{session_id}
 * 
 * @param ctx API context with database and RBAC
 * @param request HTTP request with session ID in path
 * @return HTTP response with success or error
 */
http_response_t* api_handle_session_terminate(api_context_t* ctx, http_request_t* request);

/**
 * Terminate specific session (DELETE method)
 * 
 * Alternative endpoint for session termination using DELETE method.
 * 
 * URL: /api/sessions/{session_id}
 * 
 * @param ctx API context with database and RBAC
 * @param request HTTP request with session ID in path
 * @return HTTP response with success or error
 */
http_response_t* api_handle_terminate_session(api_context_t* ctx, http_request_t* request);

/** @} */

/**
 * @defgroup library_ctx Library Context
 * @{
 */

/**
 * Get current library context
 * 
 * Returns the library context for the authenticated user.
 * 
 * @param ctx API context with database and RBAC
 * @param request HTTP request with authentication
 * @return HTTP response with library context
 */
http_response_t* api_handle_get_library_context(api_context_t* ctx, http_request_t* request);

/**
 * Switch library context
 * 
 * Changes the active library for the current session.
 * 
 * Request body:
 * ```json
 * {
 *   "library": "string"
 * }
 * ```
 * 
 * @param ctx API context with database and RBAC
 * @param request HTTP request with new library
 * @return HTTP response with updated context
 */
http_response_t* api_handle_switch_library(api_context_t* ctx, http_request_t* request);

/** @} */

/**
 * @defgroup password_mgmt Password Management
 * @{
 */

/**
 * Change user password
 * 
 * Updates password for authenticated user.
 * 
 * Request body:
 * ```json
 * {
 *   "current_password": "string",
 *   "new_password": "string"
 * }
 * ```
 * 
 * @param ctx API context with database and RBAC
 * @param request HTTP request with password data
 * @return HTTP response with success or error
 */
http_response_t* api_handle_change_password(api_context_t* ctx, http_request_t* request);

/** @} */

#endif /* API_AUTH_H */