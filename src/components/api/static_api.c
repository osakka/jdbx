/**
 * @file static_api.c
 * @brief Static admin authentication API endpoints for JDBX
 * 
 * Provides static admin authentication endpoints including:
 * - Admin login with credential validation
 * - Admin test endpoints for debugging
 * - JWT token generation and management
 * 
 * Architecture: Implements simplified admin authentication using:
 * - Hardcoded admin credentials for demo purposes
 * - JWT token generation for session management
 * - HTTP cookie-based authentication persistence
 * 
 * Authentication Flow:
 * 1. Client submits username/password via JSON POST
 * 2. Server validates credentials against hardcoded admin account
 * 3. On success, generates JWT token and sets HTTP-only cookie
 * 4. Client receives token in both response body and cookie
 * 
 * Security: Uses static admin credentials (admin/admin) - suitable for
 * development and demo environments only. Production deployments should
 * integrate with the full RBAC system.
 * 
 * @note Demo authentication - hardcoded admin:admin credentials
 * @performance O(1) authentication with JWT token generation overhead
 * @threadsafe Thread-safe token generation and response handling
 * @memory Uses checkpoint-based allocation for request processing
 */

#include "api/api.h"
#include "core/server.h"
#include "database/database.h"
#include "rbac/rbac.h"
#include "rbac/jwt.h"
#include "utils/buffer_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * Handle admin test API request
 * 
 * Provides a simple test endpoint for verifying admin API functionality
 * and server responsiveness. Returns server information and debug data
 * for development and debugging purposes.
 * 
 * @param ctx API context (unused but required for API interface consistency)
 * @param request HTTP request (unused but required for API interface consistency)
 * @return JSON response with test data including server version and timestamp
 * 
 * @note No authentication required - public test endpoint
 * @performance O(1) operation with minimal server impact
 * @threadsafe Safe for concurrent access
 * @memory Response allocated using checkpoint memory - automatically freed
 * 
 * @example
 * GET /api/admin/test
 * Response: {
 *   "success": true,
 *   "message": "Admin test route is working!",
 *   "time": 1640995200,
 *   "server": {"version": "1.0.0", "api_path": "/api/admin/test"}
 * }
 */
http_response_t* api_handle_admin_test(api_context_t* ctx, http_request_t* request) {
  (void)ctx; /* Avoid unused parameter warning */
  (void)request; /* Avoid unused parameter warning */
  printf("Admin test endpoint called\n");

  /* Create detailed response with debug info */
  json_value_t* test_obj = json_create_object();
  json_object_set(test_obj, "success", json_create_boolean(1));
  json_object_set(test_obj, "message", json_create_string("Admin test route is working!"));
  json_object_set(test_obj, "time", json_create_number((double)time(NULL)));

  /* Add server info */
  json_value_t* server_info = json_create_object();
  json_object_set(server_info, "version", json_create_string("1.0.0"));
  json_object_set(server_info, "api_path", json_create_string("/api/admin/test"));

  /* Add the server info to the response */
  json_object_set(test_obj, "server", server_info);

  /* Convert to string */
  char* response_str = json_stringify(test_obj);
  /* CHECKPOINT: json_free(test_obj); */

  /* Create response */
  http_response_t* response = create_http_response(HTTP_OK, response_str, "application/json");

  /* CORS headers will be applied by the server framework */
  /* We don't need to add them here anymore */

  printf("Test endpoint returning response\n");
  return response;
}

/**
 * Handle admin login API request
 * 
 * Processes admin authentication requests using hardcoded credentials.
 * Validates username/password, generates JWT tokens, and sets authentication
 * cookies for successful logins. Implements the complete authentication flow.
 * 
 * @param ctx API context containing request routing and validation info
 * @param request HTTP request with JSON body containing username and password
 * @return JSON response with authentication result and JWT token or error
 * 
 * @note Hardcoded admin:admin credentials - for demo/development only
 * @performance O(1) authentication with JWT generation overhead
 * @threadsafe Thread-safe credential validation and token generation
 * @memory Request body parsed using checkpoint memory, token freed after use
 * 
 * @example
 * POST /api/admin/login
 * Body: {"username": "admin", "password": "admin"}
 * Response: {
 *   "success": true,
 *   "message": "Login successful",
 *   "username": "admin",
 *   "token": "eyJ0eXAiOiJKV1QiLCJhbGc..."
 * }
 * Headers: Set-Cookie: auth_token=...; HttpOnly; Secure
 */
http_response_t* api_handle_admin_login(api_context_t* ctx, http_request_t* request) {
  printf("Admin login called\n");

  if (!ctx || !request) {
    printf("Invalid context or request\n");
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid request\"}", "application/json");
  }

  /* Check for missing body */
  if (!request->body) {
    printf("Missing request body\n");
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Missing request body\"}", "application/json");
  }

  /* Log received body */
  printf("Received login request body: %s\n", request->body);

  /* Parse request body */
  json_value_t* body = json_parse(request->body);
  if (!body || body->type != JSON_OBJECT) {
    if (body) /* CHECKPOINT: json_free(body); */
    printf("Invalid JSON body\n");
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid request body\"}", "application/json");
  }

  /* Extract username and password */
  json_value_t* username_val = json_object_get(body, "username");
  json_value_t* password_val = json_object_get(body, "password");

  if (!username_val || username_val->type != JSON_STRING ||
    !password_val || password_val->type != JSON_STRING) {
    /* CHECKPOINT: json_free(body); */
    printf("Missing username or password fields\n");
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Username and password required\"}", "application/json");
  }

  const char* username = username_val->value.string;
  const char* password = password_val->value.string;

  printf("Login attempt: username=%s\n", username);

  /* For demo purposes, just check for hardcoded admin credentials */
  /* In a real application, you'd use the RBAC system with proper roles */
  if (strcmp(username, "admin") != 0 || strcmp(password, "admin") != 0) {
    /* CHECKPOINT: json_free(body); */
    printf("Invalid credentials\n");
    return create_http_response(HTTP_UNAUTHORIZED,
                 "{\"error\":\"Invalid credentials\"}", "application/json");
  }

  /* User authenticated, generate token */
  char* token = generate_static_auth_token(username);
  if (!token) {
    /* CHECKPOINT: json_free(body); */
    printf("Failed to generate token\n");
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Failed to generate token\"}", "application/json");
  }

  printf("Generated token for %s\n", username);

  /* Create success response */
  json_value_t* response_obj = json_create_object();
  json_object_set(response_obj, "success", json_create_boolean(1));
  json_object_set(response_obj, "message", json_create_string("Login successful"));
  json_object_set(response_obj, "username", json_create_string(username));
  json_object_set(response_obj, "token", json_create_string(token)); /* Add token to the response for client-side storage */

  /* Serialize response */
  char* response_str = json_stringify(response_obj);
  /* CHECKPOINT: json_free(response_obj); */
  /* CHECKPOINT: json_free(body); */

  printf("Login successful, sending response\n");

  /* Create HTTP response with auth cookie */
  http_response_t* response = create_http_response(HTTP_OK, response_str, "application/json");
  response = create_auth_response(response, token);

  /* CORS headers are now handled by apply_cors_headers in server.c */
  /* Let the central CORS handling function take care of it */
  
  /* Ensure success value is present for client */
  json_value_t* json_response = json_parse(response->body);
  if (json_response && json_response->type == JSON_OBJECT) {
    if (!json_object_has(json_response, "success")) {
      json_object_set(json_response, "success", json_create_boolean(1));
      
      /* Update response body */
      char* updated_body = json_stringify(json_response);
      if (updated_body) {
        BUFFER_FREE(response->body);
        response->body = updated_body;
        response->content_length = strlen(updated_body);
      }
    }
    /* CHECKPOINT: json_free(json_response); */
  }

  /* Free token */
  BUFFER_FREE(token);

  return response;
}