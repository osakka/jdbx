/**
 * @file api_auth.c
 * @brief Authentication and session management API handlers implementation
 * 
 * Consolidates all authentication-related API endpoints including login,
 * logout, registration, session management, token operations, and password
 * management. Uses unified documents architecture and checkpoint-based
 * memory management.
 */

#include "api/api_auth.h"
#include "api/api.h"
#include "database/document_storage.h"
#include "database/virtual_layer.h"
#include "core/server.h"
#include "database/database.h"
#include "rbac/rbac.h"
#include "rbac/rbac_db.h"
#include "rbac/rbac_database.h"
#include "rbac/jwt.h"
#include "rbac/jwt_cache.h"
#include "utils/config_loader.h"
#include "utils/json.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <pthread.h>

/* Global bootstrap synchronization mutex */
static pthread_mutex_t g_bootstrap_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_bootstrap_completed = 0;

/**
 * Handle user login
 */
http_response_t* api_handle_login(api_context_t* ctx, http_request_t* request) {
  LOG_DEBUG("Starting login handler.");
  
  if (!ctx || !request || !request->body) {
    LOG_ERROR("Invalid request parameters.");
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  LOG_DEBUG("Parsing request body: %s", request->body);
  
  /* Parse request body */
  json_value_t* body = json_parse(request->body);
  LOG_DEBUG("JSON parsing completed.");
  if (!body || body->type != JSON_OBJECT) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request body\"}", "application/json");
  }
  
  /* Extract username, password, and optional library */
  json_value_t* username_val = json_object_get(body, "username");
  json_value_t* password_val = json_object_get(body, "password");
  json_value_t* library_val = json_object_get(body, "library");
  
  if (!username_val || username_val->type != JSON_STRING || 
    !password_val || password_val->type != JSON_STRING) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Username and password required\"}", "application/json");
  }
  
  const char* username = username_val->value.string;
  const char* password = password_val->value.string;
  const char* library = NULL;
  
  /* Check if library is specified in request or username contains @ */
  char username_copy[256] = {0};
  if (library_val && library_val->type == JSON_STRING) {
    library = library_val->value.string;
  } else if (strchr(username, '@')) {
    /* Parse username@library format - need to copy since we'll modify it */
    strncpy(username_copy, username, sizeof(username_copy) - 1);
    char* at_pos = strchr(username_copy, '@');
    if (at_pos) {
      *at_pos = '\0';
      username = username_copy;
      library = at_pos + 1;
    }
  }
  
  TRACE_AUTH("Extracted credentials for username: %s", username);
  
  /* Check if we need to perform deferred bootstrap */
  if (ctx->db && ctx->db->is_bootstrap_mode) {
    const char* deferred_bootstrap = getenv("JDBX_DEFERRED_BOOTSTRAP");
    if (deferred_bootstrap && strcmp(deferred_bootstrap, "1") == 0) {
      
      /* Use mutex to ensure bootstrap only happens once across all threads */
      pthread_mutex_lock(&g_bootstrap_mutex);
      
      /* Double-check pattern: another thread might have completed bootstrap */
      if (g_bootstrap_completed) {
        pthread_mutex_unlock(&g_bootstrap_mutex);
        LOG_DEBUG("Bootstrap already completed by another thread");
      } else {
        LOG_INFO("Performing deferred bootstrap - creating admin user");
        
        /* Create admin role and user now that server is fully initialized */
        char* admin_role_id = NULL;
        if (create_default_admin_role(ctx->db, &admin_role_id)) {
          if (admin_role_id) {
            if (create_default_admin_user(ctx->db, admin_role_id)) {
              LOG_INFO("Admin user created successfully, now creating library documents");
              
              /* Create library documents for system and default libraries */
              /* These were created as physical structures but need metadata documents */
              
              /* Create system library document */
              json_value_t* system_lib_doc = json_create_object();
              json_object_set(system_lib_doc, "type", json_create_string("library"));
              json_object_set(system_lib_doc, "name", json_create_string("system"));
              json_object_set(system_lib_doc, "template", json_create_string("system"));
              json_object_set(system_lib_doc, "owner", json_create_string("system-admin"));
              json_object_set(system_lib_doc, "description", json_create_string("System library for internal operations"));
              
              /* Add timestamps - use Unix timestamp integers for consistency */
              time_t now = time(NULL);
              json_object_set(system_lib_doc, "created_at", json_create_integer(now));
              json_object_set(system_lib_doc, "updated_at", json_create_integer(now));
              
              /* Insert system library document */
              json_value_t* system_lib_result = db_insert_document(ctx->db, STORAGE_LIBRARY, STORAGE_COLLECTION, system_lib_doc);
              if (system_lib_result) {
                json_value_t* system_lib_id_val = json_object_get(system_lib_result, "uuid");
                if (system_lib_id_val && system_lib_id_val->type == JSON_STRING) {
                  LOG_INFO("Created system library document: %s", system_lib_id_val->value.string);
                }
                /* CHECKPOINT: json_free(system_lib_result); */
              } else {
                LOG_WARNING("Failed to create system library document");
              }
              /* CHECKPOINT: json_free(system_lib_doc); */
              
              /* Create default library document */
              json_value_t* default_lib_doc = json_create_object();
              json_object_set(default_lib_doc, "type", json_create_string("library"));
              json_object_set(default_lib_doc, "name", json_create_string("default"));
              json_object_set(default_lib_doc, "template", json_create_string("standard"));
              json_object_set(default_lib_doc, "owner", json_create_string("system-admin"));
              json_object_set(default_lib_doc, "description", json_create_string("Default library for general use"));
              json_object_set(default_lib_doc, "created_at", json_create_integer(now));
              json_object_set(default_lib_doc, "updated_at", json_create_integer(now));
              
              /* Insert default library document */
              json_value_t* default_lib_result = db_insert_document(ctx->db, STORAGE_LIBRARY, STORAGE_COLLECTION, default_lib_doc);
              if (default_lib_result) {
                json_value_t* default_lib_id_val = json_object_get(default_lib_result, "uuid");
                if (default_lib_id_val && default_lib_id_val->type == JSON_STRING) {
                  LOG_INFO("Created default library document: %s", default_lib_id_val->value.string);
                }
                /* CHECKPOINT: json_free(default_lib_result); */
              } else {
                LOG_WARNING("Failed to create default library document");
              }
              /* CHECKPOINT: json_free(default_lib_doc); */
              
              /* Create system actors (system-admin, system-metrics, etc.) */
              extern int create_system_actors(database_t* db);
              create_system_actors(ctx->db);
              
              LOG_INFO("Deferred bootstrap completed successfully with library documents and system actors");
              /* Disable bootstrap mode */
              ctx->db->is_bootstrap_mode = 0;
              unsetenv("JDBX_DEFERRED_BOOTSTRAP");
              
              /* Mark bootstrap as completed */
              g_bootstrap_completed = 1;
              
              /* Load secure admin credentials for comparison */
              char* admin_user = NULL;
              char* admin_pass = NULL; 
              char* admin_email = NULL;
              config_load_bootstrap_admin_credentials(&admin_user, &admin_pass, &admin_email);
              
              /* If this request used the configured admin credentials, return successful login */
              int is_admin_login = (admin_user && admin_pass && 
                                   strcmp(username, admin_user) == 0 && 
                                   strcmp(password, admin_pass) == 0);
              
              /* Clean up credentials immediately for security */
              BUFFER_FREE(admin_user);
              BUFFER_FREE(admin_pass);
              BUFFER_FREE(admin_email);
              
              if (is_admin_login) {
                pthread_mutex_unlock(&g_bootstrap_mutex);
                
                /* Get the admin user ID that was just created */
                json_value_t* bootstrap_query = json_create_object();
                json_object_set(bootstrap_query, "type", json_create_string(DOC_TYPE_NAME_USER));
                json_object_set(bootstrap_query, "library", json_create_string("system"));
                json_object_set(bootstrap_query, "collection", json_create_string("users"));
                json_object_set(bootstrap_query, "username", json_create_string("admin"));
                
                json_value_t* bootstrap_results = db_query_documents(ctx->db, STORAGE_LIBRARY, STORAGE_COLLECTION, bootstrap_query);
                /* CHECKPOINT: json_free(bootstrap_query); */
                
                if (bootstrap_results) {
                  json_value_t* documents = json_object_get(bootstrap_results, "documents");
                  if (documents && documents->type == JSON_ARRAY && json_array_size(documents) > 0) {
                    json_value_t* admin_user = json_array_get(documents, 0);
                    json_value_t* user_id_val = json_object_get(admin_user, "uuid");
                    
                    if (user_id_val && user_id_val->type == JSON_STRING) {
                      const char* admin_user_id = user_id_val->value.string;
                      
                      /* Create JWT token pair for bootstrap admin login */
                      json_value_t* bootstrap_response_obj = NULL;
                      char* bootstrap_response_str = jwt_create_token_pair(ctx->jwt_secret, admin_user_id, "admin", &bootstrap_response_obj);
                      
                      if (bootstrap_response_str) {
                        /* CHECKPOINT: json_free(bootstrap_results); */
                        /* CHECKPOINT: json_free(body); */
                        
                        http_response_t* response = create_http_response(HTTP_OK, bootstrap_response_str, "application/json");
                        BUFFER_FREE(bootstrap_response_str);
                        /* CHECKPOINT: json_free(bootstrap_response_obj); */
                        
                        LOG_INFO("Bootstrap admin login successful - returning immediate response");
                        return response;
                      }
                    }
                  }
                  /* CHECKPOINT: json_free(bootstrap_results); */
                }
              }
            }
          }
          BUFFER_FREE(admin_role_id);
        }
        
        /* Release bootstrap mutex */
        pthread_mutex_unlock(&g_bootstrap_mutex);
      }
    }
  }
  
  /* Determine target library - default to system if not specified */
  if (!library) {
    library = "system";
  }
  
  LOG_DEBUG("Attempting login for user: %s in library: %s", username, library);
  
  /* VIRTUAL OPERATION: Query for user using clear virtual function */
  json_value_t* username_filter = json_create_object();
  json_object_set(username_filter, "username", json_create_string(username));
  
  LOG_DEBUG("Virtual query for user '%s' in library='%s'", username, RBAC_SYSTEM_LIBRARY);
  
  if (!ctx->db) {
    LOG_ERROR("Database context is NULL!");
    /* CHECKPOINT: json_free(username_filter); */
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Database not initialized\"}", "application/json");
  }
  
  /* Use clear virtual function for user query */
  json_value_t* query_results = virtual_query_users(ctx->db, RBAC_SYSTEM_LIBRARY, username_filter);
  
  /* CHECKPOINT: json_free(username_filter); */
    
  if (!query_results) {
    /* CHECKPOINT: json_free(username_filter); */
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to query user\"}", "application/json");
  }
  
  /* Extract documents array from response object */
  json_value_t* results = json_object_get(query_results, "documents");
  if (!results || results->type != JSON_ARRAY || json_array_size(results) == 0) {
    /* CHECKPOINT: json_free(username_filter); */
    /* CHECKPOINT: json_free(query_results); */
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Invalid credentials\"}", "application/json");
  }
  
  json_value_t* user_doc = json_array_get(results, 0);
  json_value_t* id_val = json_object_get(user_doc, "uuid");
  json_value_t* password_hash_val = json_object_get(user_doc, "password_hash");
  
  if (!id_val || id_val->type != JSON_STRING || 
      !password_hash_val || password_hash_val->type != JSON_STRING) {
    /* CHECKPOINT: json_free(username_filter); */
    /* CHECKPOINT: json_free(query_results); */
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"User data corrupted\"}", "application/json");
  }
  
  const char* user_id = id_val->value.string;
  const char* stored_hash = password_hash_val->value.string;
  
  /* Verify password */
  extern int verify_password(const char* password, const char* hash);
  
  if (!verify_password(password, stored_hash)) {
    /* CHECKPOINT: json_free(username_filter); */
    /* CHECKPOINT: json_free(query_results); */
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Invalid credentials\"}", "application/json");
  }
  
  /* Store library in user's session for context */
  json_value_t* user_library = json_object_get(user_doc, "library");
  const char* user_lib = (user_library && user_library->type == JSON_STRING) ? 
                         user_library->value.string : library;
    
  /* Create proper JWT token pair */
  LOG_DEBUG("Creating JWT token pair for user: %s", username);
  json_value_t* response_obj = NULL;
  char* response_str = jwt_create_token_pair(ctx->jwt_secret, user_id, username, &response_obj);
  LOG_DEBUG("jwt_create_token_pair returned, checking result.");
  
  if (!response_str || !response_obj) {
    LOG_ERROR("Failed to create token response.");
    /* CHECKPOINT: json_free(query_results); */
    /* CHECKPOINT: json_free(body); */
    /* CHECKPOINT: if (response_obj) json_free(response_obj); */
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to create response\"}", "application/json");
  }
  
  LOG_DEBUG("Token response created successfully.");
  
  /* Add library context to response */
  json_object_set(response_obj, "library", json_create_string(user_lib));
    
    /* Create session record for the access token */
    if (ctx->db && response_obj) {
      json_value_t* token_val = json_object_get(response_obj, "token");
      if (token_val && token_val->type == JSON_STRING) {
        const char* access_token = token_val->value.string;
        
        /* First, invalidate any existing active sessions for this user */
        LOG_DEBUG("Checking for existing sessions for user: %s", user_id);
        json_value_t* session_query = json_create_object();
        json_object_set(session_query, "type", json_create_string(DOC_TYPE_NAME_SESSION));
        json_object_set(session_query, "library", json_create_string(RBAC_SYSTEM_LIBRARY));
        json_object_set(session_query, "collection", json_create_string(RBAC_SESSIONS_COLLECTION_NAME));
        json_object_set(session_query, "user_id", json_create_string(user_id));
        json_object_set(session_query, "active", json_create_boolean(1));
        
        /* Query system/sessions collection in JDBX architecture */
        json_value_t* existing_sessions_response = db_query_documents(ctx->db, STORAGE_LIBRARY, STORAGE_COLLECTION, session_query);
        /* CHECKPOINT: json_free(session_query); */
        
        if (existing_sessions_response) {
          /* Extract documents array from response object */
          json_value_t* existing_sessions = json_object_get(existing_sessions_response, "documents");
          if (existing_sessions && existing_sessions->type == JSON_ARRAY) {
            int session_count = json_array_size(existing_sessions);
            LOG_DEBUG("Found %d existing active sessions for user", session_count);
            
            /* Invalidate each existing session */
            for (size_t i = 0; i < existing_sessions->value.array.size; i++) {
              json_value_t* session = json_array_get(existing_sessions, i);
              json_value_t* session_id_val = json_object_get(session, "uuid");
              if (session_id_val && session_id_val->type == JSON_STRING) {
                const char* old_session_id = session_id_val->value.string;
                LOG_INFO("Invalidating old session: %s", old_session_id);
                rbac_db_invalidate_session(ctx->db, old_session_id);
              }
            }
          }
          /* CHECKPOINT: json_free(existing_sessions_response); */
        }
        
        /* Extract client info from request */
        const char* ip_address = request->remote_addr;
        const char* user_agent = request->user_agent;
        
        /* Create new session with 30 minute expiration */
        time_t expires_at = time(NULL) + (30 * 60);
        char* session_id = rbac_db_create_session(ctx->db, user_id, access_token, 
                            expires_at, ip_address, user_agent);
        
        if (session_id) {
          LOG_INFO("New session created with ID: %s", session_id);
          /* CHECKPOINT: buffer_pool_free(session_id); - Let checkpoint handle cleanup */
          BUFFER_FREE(session_id);
        } else {
          LOG_ERROR("Failed to create session for user: %s", username);
        }
      }
    }
    
    /* Re-serialize response with library added */
    BUFFER_FREE(response_str);
    response_str = json_stringify(response_obj);
    
    /* Clean up and return response */
    /* CHECKPOINT: json_free(username_filter); */
    /* CHECKPOINT: json_free(query_results); */
    /* CHECKPOINT: json_free(body); */
    
    http_response_t* response = create_http_response(HTTP_OK, response_str, "application/json");
    LOG_DEBUG("Returning successful login response with library context: %s", user_lib);
    
    /* Clean up response string */
    BUFFER_FREE(response_str);
    
    return response;
}

/**
 * Handle user registration
 */
http_response_t* api_handle_register(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request || !request->body) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }

  /* Parse request body */
  json_value_t* body = json_parse(request->body);
  if (!body || body->type != JSON_OBJECT) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request body\"}", "application/json");
  }

  /* Extract registration fields */
  json_value_t* username_val = json_object_get(body, "username");
  json_value_t* password_val = json_object_get(body, "password");
  json_value_t* email_val = json_object_get(body, "email");
  json_value_t* library_val = json_object_get(body, "library");

  if (!username_val || username_val->type != JSON_STRING || 
      !password_val || password_val->type != JSON_STRING) {
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Username and password required\"}", "application/json");
  }

  const char* username = username_val->value.string;
  const char* password = password_val->value.string;
  const char* email = email_val && email_val->type == JSON_STRING ? 
                      email_val->value.string : NULL;
  const char* library = library_val && library_val->type == JSON_STRING ? 
                        library_val->value.string : "default";

  /* Validate username */
  if (strlen(username) < 3 || strlen(username) > 50) {
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Username must be between 3 and 50 characters\"}", "application/json");
  }

  /* Validate password */
  if (strlen(password) < 8) {
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Password must be at least 8 characters\"}", "application/json");
  }

  /* Check if user already exists */
  json_value_t* username_filter = json_create_object();
  json_object_set(username_filter, "username", json_create_string(username));
  
  json_value_t* existing_users = virtual_query_users(ctx->db, RBAC_SYSTEM_LIBRARY, username_filter);
  /* CHECKPOINT: json_free(username_filter); */
  
  if (existing_users) {
    json_value_t* documents = json_object_get(existing_users, "documents");
    if (documents && documents->type == JSON_ARRAY && json_array_size(documents) > 0) {
      /* CHECKPOINT: json_free(existing_users); */
      /* CHECKPOINT: json_free(body); */
      return create_http_response(HTTP_CONFLICT, 
                   "{\"error\":\"Username already exists\"}", "application/json");
    }
    /* CHECKPOINT: json_free(existing_users); */
  }

  /* Get default role for new users */
  const char* default_role = "user"; /* Could be made configurable */
  
  /* Create new user */
  json_value_t* user_result = virtual_create_user(ctx->db, username, password, RBAC_SYSTEM_LIBRARY);
  if (!user_result) {
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to create user\"}", "application/json");
  }

  /* Extract user ID from result */
  json_value_t* user_id_val = json_object_get(user_result, "uuid");
  if (!user_id_val || user_id_val->type != JSON_STRING) {
    /* CHECKPOINT: json_free(user_result); */
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to get user ID\"}", "application/json");
  }

  const char* user_id = user_id_val->value.string;

  /* Add email if provided */
  if (email) {
    json_value_t* update = json_create_object();
    json_object_set(update, "email", json_create_string(email));
    virtual_update(ctx->db, user_id, update);
    /* CHECKPOINT: json_free(update); */
  }

  /* Add user to default role */
  if (!rbac_db_add_user_to_role(ctx->db, user_id, default_role)) {
    LOG_WARNING("Failed to add user to default role");
  }

  /* Create response */
  json_value_t* response = json_create_object();
  json_object_set(response, "message", json_create_string("User registered successfully"));
  json_object_set(response, "user_id", json_create_string(user_id));
  json_object_set(response, "username", json_create_string(username));
  json_object_set(response, "library", json_create_string(library));
  
  char* response_str = json_stringify(response);
  /* CHECKPOINT: json_free(response); */
  /* CHECKPOINT: json_free(user_result); */
  /* CHECKPOINT: json_free(body); */

  http_response_t* http_response = create_http_response(HTTP_CREATED, response_str, "application/json");
  BUFFER_FREE(response_str);
  
  return http_response;
}

/**
 * Handle token refresh
 */
http_response_t* api_handle_token_refresh(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }

  /* Extract current token */
  char* token = api_extract_token(request);
  if (!token) {
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"No authorization token\"}", "application/json");
  }

  /* Validate current token */
  jwt_token_t* jwt = jwt_decode(token);
  if (!jwt || !jwt->payload) {
    BUFFER_FREE(token);
    if (jwt) jwt_free(jwt);
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Invalid token\"}", "application/json");
  }

  /* Check if token is expired */
  time_t now = time(NULL);
  if (jwt->payload->exp && jwt->payload->exp < now) {
    BUFFER_FREE(token);
    jwt_free(jwt);
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Token expired\"}", "application/json");
  }

  /* Extract user info from token */
  const char* user_id = jwt->payload->sub;
  const char* username = NULL;
  
  if (jwt->payload->claims) {
    json_value_t* username_val = json_object_get(jwt->payload->claims, "username");
    if (username_val && username_val->type == JSON_STRING) {
      username = username_val->value.string;
    }
  }

  if (!user_id || !username) {
    BUFFER_FREE(token);
    jwt_free(jwt);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Invalid token claims\"}", "application/json");
  }

  /* Create new token pair */
  json_value_t* response_obj = NULL;
  char* response_str = jwt_create_token_pair(ctx->jwt_secret, user_id, username, &response_obj);
  
  if (!response_str || !response_obj) {
    BUFFER_FREE(token);
    jwt_free(jwt);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to create new token\"}", "application/json");
  }

  /* Create new session for refreshed token */
  json_value_t* new_token_val = json_object_get(response_obj, "token");
  if (new_token_val && new_token_val->type == JSON_STRING) {
    const char* new_token = new_token_val->value.string;
    
    /* Invalidate old session */
    json_value_t* session = virtual_get_session_by_token(ctx->db, token);
    if (session) {
      json_value_t* session_id_val = json_object_get(session, "uuid");
      if (session_id_val && session_id_val->type == JSON_STRING) {
        rbac_db_invalidate_session(ctx->db, session_id_val->value.string);
      }
      /* CHECKPOINT: json_free(session); */
    }
    
    /* Create new session */
    time_t expires_at = time(NULL) + (30 * 60);
    char* session_id = rbac_db_create_session(ctx->db, user_id, new_token, 
                        expires_at, request->remote_addr, request->user_agent);
    if (session_id) {
      BUFFER_FREE(session_id);
    }
  }

  BUFFER_FREE(token);
  jwt_free(jwt);
  /* CHECKPOINT: json_free(response_obj); */

  http_response_t* response = create_http_response(HTTP_OK, response_str, "application/json");
  BUFFER_FREE(response_str);
  
  return response;
}

/**
 * Handle logout (invalidate session)
 */
http_response_t* api_handle_logout(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !ctx->db || !request) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Get token from authorization header */
  const char* auth_header = request->authorization;
  if (!auth_header) {
    return create_http_response(HTTP_UNAUTHORIZED,
                 "{\"error\":\"No authorization header\"}", "application/json");
  }
  
  /* Extract token */
  const char* token = auth_header;
  if (strncmp(auth_header, "Bearer ", 7) == 0) {
    token = auth_header + 7;
  }
  
  /* Find session by token using virtual layer - single source of truth */
  json_value_t* query = json_create_object();
  json_object_set(query, "token", json_create_string(token));
  json_object_set(query, "active", json_create_boolean(1));
  
  json_value_t* results = virtual_query(ctx->db, DOC_TYPE_NAME_SESSION, "system", "sessions", query);
  /* CHECKPOINT: json_free(query); */
  
  if (!results) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Failed to query sessions\"}", "application/json");
  }
  
  /* Get documents array */
  json_value_t* documents = json_object_get(results, "documents");
  if (!documents || documents->type != JSON_ARRAY || json_array_size(documents) == 0) {
    /* CHECKPOINT: json_free(results); */
    return create_http_response(HTTP_NOT_FOUND,
                 "{\"error\":\"Session not found\"}", "application/json");
  }
  
  /* Get the session (there should only be one with this token) */
  json_value_t* session = json_array_get(documents, 0);
  json_value_t* session_id_val = json_object_get(session, "uuid");
  json_value_t* user_id_val = json_object_get(session, "user_id");
  
  if (session_id_val && session_id_val->type == JSON_STRING) {
    /* Invalidate the specific token from JWT cache */
    jwt_cache_invalidate_token(token);
    LOG_INFO("Invalidated JWT cache for token");
    
    /* Also invalidate all cache entries for the user */
    if (user_id_val && user_id_val->type == JSON_STRING) {
      jwt_cache_invalidate_user(user_id_val->value.string);
      LOG_INFO("Invalidated all JWT cache entries for user: %s", user_id_val->value.string);
    }
    
    /* Invalidate ALL sessions with this token */
    if (rbac_db_invalidate_sessions_by_token(ctx->db, token)) {
      /* CHECKPOINT: json_free(results); */
      return create_http_response(HTTP_OK,
                   "{\"success\":true,\"message\":\"Logged out successfully\"}", 
                   "application/json");
    }
  }
  
  /* CHECKPOINT: json_free(results); */
  return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
               "{\"error\":\"Failed to invalidate session\"}", "application/json");
}

/**
 * Get all sessions for current user
 */
http_response_t* api_handle_get_sessions(api_context_t* ctx, http_request_t* request) {
  (void)request; /* Suppress unused parameter warning */
  if (!ctx || !ctx->db) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Database not initialized\"}", "application/json");
  }
  
  /* Query all sessions using virtual layer - single source of truth */
  json_value_t* query = json_create_object();
  json_object_set(query, "type", json_create_string(DOC_TYPE_NAME_SESSION));
  json_object_set(query, "library", json_create_string("system"));
  json_object_set(query, "collection", json_create_string("sessions"));
  json_value_t* results = virtual_query(ctx->db, DOC_TYPE_NAME_SESSION, "system", "sessions", query);
  /* CHECKPOINT: json_free(query); */
  
  if (!results) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Failed to query sessions\"}", "application/json");
  }
  
  /* Extract documents array */
  json_value_t* documents = json_object_get(results, "documents");
  if (!documents || documents->type != JSON_ARRAY) {
    /* CHECKPOINT: json_free(results); */
    
    /* Return empty array */
    json_value_t* empty_response = json_create_object();
    json_object_set(empty_response, "sessions", json_create_array());
    json_object_set(empty_response, "count", json_create_number(0));
    
    char* response_str = json_stringify(empty_response);
    /* CHECKPOINT: json_free(empty_response); */
    
    return create_http_response(HTTP_OK, response_str, "application/json");
  }
  
  /* Filter out corrupted sessions */
  json_value_t* valid_sessions = json_create_array();
  size_t doc_count = json_array_size(documents);
  
  for (size_t i = 0; i < doc_count; i++) {
    json_value_t* session = json_array_get(documents, i);
    if (!session) continue;
    
    /* Check for required fields */
    json_value_t* user_id = json_object_get(session, "user_id");
    json_value_t* username = json_object_get(session, "username");
    json_value_t* token = json_object_get(session, "token");
    json_value_t* created_at = json_object_get(session, "created_at");
    json_value_t* expires_at = json_object_get(session, "expires_at");
    
    /* Only include sessions with all required fields */
    if (user_id && user_id->type == JSON_STRING &&
        username && username->type == JSON_STRING &&
        token && token->type == JSON_STRING &&
        created_at && created_at->type == JSON_STRING &&
        expires_at && expires_at->type == JSON_STRING) {
      json_array_append(valid_sessions, json_clone(session));
    } else {
      /* Log corrupted session for debugging */
      json_value_t* session_id = json_object_get(session, "uuid");
      if (session_id && session_id->type == JSON_STRING) {
        LOG_WARNING("Skipping corrupted session: %s", session_id->value.string);
      }
    }
  }
  
  /* Create response with valid sessions */
  json_value_t* response_obj = json_create_object();
  json_object_set(response_obj, "sessions", valid_sessions);
  json_object_set(response_obj, "count", json_create_number(json_array_size(valid_sessions)));
  
  /* CHECKPOINT: json_free(results); */
  
  char* response_str = json_stringify(response_obj);
  /* CHECKPOINT: json_free(response_obj); */
  
  return create_http_response(HTTP_OK, response_str, "application/json");
}

/**
 * Get all active sessions (admin only)
 */
http_response_t* api_handle_get_active_sessions(api_context_t* ctx, http_request_t* request) {
  (void)request; /* Suppress unused parameter warning */
  if (!ctx || !ctx->db) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Database not initialized\"}", "application/json");
  }
  
  /* Query active sessions using virtual layer - single source of truth */
  json_value_t* query = json_create_object();
  json_object_set(query, "type", json_create_string(DOC_TYPE_NAME_SESSION));
  json_object_set(query, "library", json_create_string("system"));
  json_object_set(query, "collection", json_create_string("sessions"));
  json_object_set(query, "active", json_create_boolean(1));
  
  json_value_t* results = virtual_query(ctx->db, DOC_TYPE_NAME_SESSION, "system", "sessions", query);
  /* CHECKPOINT: json_free(query); */
  
  if (!results) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Failed to query sessions\"}", "application/json");
  }
  
  /* Extract documents array */
  json_value_t* documents = json_object_get(results, "documents");
  if (!documents || documents->type != JSON_ARRAY) {
    /* CHECKPOINT: json_free(results); */
    
    /* Return empty array */
    json_value_t* empty_response = json_create_object();
    json_object_set(empty_response, "sessions", json_create_array());
    json_object_set(empty_response, "count", json_create_number(0));
    
    char* response_str = json_stringify(empty_response);
    /* CHECKPOINT: json_free(empty_response); */
    
    return create_http_response(HTTP_OK, response_str, "application/json");
  }
  
  /* Filter out corrupted sessions */
  json_value_t* valid_sessions = json_create_array();
  size_t doc_count = json_array_size(documents);
  
  for (size_t i = 0; i < doc_count; i++) {
    json_value_t* session = json_array_get(documents, i);
    if (!session) continue;
    
    /* Check for required fields */
    json_value_t* user_id = json_object_get(session, "user_id");
    json_value_t* username = json_object_get(session, "username");
    json_value_t* token = json_object_get(session, "token");
    json_value_t* created_at = json_object_get(session, "created_at");
    json_value_t* expires_at = json_object_get(session, "expires_at");
    
    /* Only include sessions with all required fields */
    if (user_id && user_id->type == JSON_STRING &&
        username && username->type == JSON_STRING &&
        token && token->type == JSON_STRING &&
        created_at && created_at->type == JSON_STRING &&
        expires_at && expires_at->type == JSON_STRING) {
      json_array_append(valid_sessions, json_clone(session));
    } else {
      /* Log corrupted session for debugging */
      json_value_t* session_id = json_object_get(session, "uuid");
      if (session_id && session_id->type == JSON_STRING) {
        LOG_WARNING("Skipping corrupted active session: %s", session_id->value.string);
      }
    }
  }
  
  /* Create response with valid sessions */
  json_value_t* response_obj = json_create_object();
  json_object_set(response_obj, "sessions", valid_sessions);
  json_object_set(response_obj, "count", json_create_number(json_array_size(valid_sessions)));
  
  /* CHECKPOINT: json_free(results); */
  
  char* response_str = json_stringify(response_obj);
  /* CHECKPOINT: json_free(response_obj); */
  
  return create_http_response(HTTP_OK, response_str, "application/json");
}

/**
 * Get current session information
 */
http_response_t* api_handle_get_current_session(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !request) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract token */
    char* token = api_extract_token(request);
    if (!token) {
        return create_http_response(HTTP_UNAUTHORIZED,
                     "{\"error\":\"No authorization token\"}", "application/json");
    }
    
    /* Decode JWT */
    jwt_token_t* jwt = jwt_decode(token);
    if (!jwt || !jwt->payload) {
        BUFFER_FREE(token);
        if (jwt) jwt_free(jwt);
        return create_http_response(HTTP_UNAUTHORIZED,
                     "{\"error\":\"Invalid token\"}", "application/json");
    }
    
    /* Extract user info from JWT payload */
    char username[256] = {0};
    char library[256] = "default";
    char user_uuid[256] = {0};
    char session_id[256] = {0};
    
    /* Get user ID (subject) */
    if (jwt->payload->sub) {
        strncpy(user_uuid, jwt->payload->sub, sizeof(user_uuid) - 1);
    }
    
    /* Get username from custom claims */
    if (jwt->payload->claims) {
        json_value_t* username_val = json_object_get(jwt->payload->claims, "username");
        if (username_val && username_val->type == JSON_STRING) {
            strncpy(username, username_val->value.string, sizeof(username) - 1);
        }
        
        json_value_t* library_val = json_object_get(jwt->payload->claims, "library");
        if (library_val && library_val->type == JSON_STRING) {
            strncpy(library, library_val->value.string, sizeof(library) - 1);
        }
        
        json_value_t* session_val = json_object_get(jwt->payload->claims, "session_id");
        if (session_val && session_val->type == JSON_STRING) {
            strncpy(session_id, session_val->value.string, sizeof(session_id) - 1);
        }
    }
    
    /* Query the session document */
    /* Use virtual layer to get session by token - single source of truth */
    json_value_t* session_doc = virtual_get_session_by_token(ctx->db, token);
    BUFFER_FREE(token);
    
    if (!session_doc) {
        jwt_free(jwt);
        return create_http_response(HTTP_NOT_FOUND,
                     "{\"error\":\"Session not found\"}", "application/json");
    }
    
    /* Build response with session info */
    json_value_t* response = json_create_object();
    json_object_set(response, "session_id", json_create_string(session_id));
    json_object_set(response, "username", json_create_string(username));
    json_object_set(response, "user_uuid", json_create_string(user_uuid));
    json_object_set(response, "library", json_create_string(library));
    
    /* Add session timestamps */
    json_value_t* created_at = json_object_get(session_doc, "created_at");
    json_value_t* expires_at = json_object_get(session_doc, "expires_at");
    if (created_at) {
        json_object_set(response, "created_at", json_clone(created_at));
    }
    if (expires_at) {
        json_object_set(response, "expires_at", json_clone(expires_at));
    }
    
    /* Add session metadata */
    json_value_t* ip_address = json_object_get(session_doc, "ip_address");
    json_value_t* user_agent = json_object_get(session_doc, "user_agent");
    if (ip_address) {
        json_object_set(response, "ip_address", json_clone(ip_address));
    }
    if (user_agent) {
        json_object_set(response, "user_agent", json_clone(user_agent));
    }
    
    /* Check if session is still active */
    json_value_t* active = json_object_get(session_doc, "active");
    if (active) {
        json_object_set(response, "active", json_clone(active));
    }
    
    /* CHECKPOINT: json_free(session_doc); */
    jwt_free(jwt);
    
    char* response_str = json_stringify(response);
    /* CHECKPOINT: json_free(response); */
    
    return create_http_response(HTTP_OK, response_str, "application/json");
}

/**
 * Get current library context
 */
http_response_t* api_handle_get_library_context(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !request) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract token */
    char* token = api_extract_token(request);
    if (!token) {
        return create_http_response(HTTP_UNAUTHORIZED,
                     "{\"error\":\"No authorization token\"}", "application/json");
    }
    
    /* Decode JWT */
    jwt_token_t* jwt = jwt_decode(token);
    if (!jwt || !jwt->payload) {
        BUFFER_FREE(token);
        if (jwt) jwt_free(jwt);
        return create_http_response(HTTP_UNAUTHORIZED,
                     "{\"error\":\"Invalid token\"}", "application/json");
    }
    
    /* Extract library from JWT claims */
    char library[256] = "default";
    if (jwt->payload->claims) {
        json_value_t* library_val = json_object_get(jwt->payload->claims, "library");
        if (library_val && library_val->type == JSON_STRING) {
            strncpy(library, library_val->value.string, sizeof(library) - 1);
        }
    }
    
    /* Query the library document */
    json_value_t* library_query = json_create_object();
    json_object_set(library_query, "type", json_create_string(DOC_TYPE_NAME_LIBRARY));
    json_object_set(library_query, "name", json_create_string(library));
    json_object_set(library_query, "library", json_create_string("system"));
    
    json_value_t* library_results = virtual_query(ctx->db, DOC_TYPE_NAME_LIBRARY, "system", VIRTUAL_COLLECTION_LIBRARIES, library_query);
    /* CHECKPOINT: json_free(library_query); */
    
    if (!library_results) {
        BUFFER_FREE(token);
        jwt_free(jwt);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                     "{\"error\":\"Failed to query library\"}", "application/json");
    }
    
    /* Extract documents */
    json_value_t* documents = json_object_get(library_results, "documents");
    if (!documents || documents->type != JSON_ARRAY || json_array_size(documents) == 0) {
        /* CHECKPOINT: json_free(library_results); */
        BUFFER_FREE(token);
        jwt_free(jwt);
        return create_http_response(HTTP_NOT_FOUND,
                     "{\"error\":\"Library not found\"}", "application/json");
    }
    
    /* Get first library document */
    json_value_t* library_doc = json_array_get(documents, 0);
    
    /* Build response */
    json_value_t* response = json_create_object();
    json_object_set(response, "library", json_create_string(library));
    
    /* Add library details */
    json_value_t* description = json_object_get(library_doc, "description");
    json_value_t* template = json_object_get(library_doc, "template");
    json_value_t* owner = json_object_get(library_doc, "owner");
    
    if (description) {
        json_object_set(response, "description", json_clone(description));
    }
    if (template) {
        json_object_set(response, "template", json_clone(template));
    }
    if (owner) {
        json_object_set(response, "owner", json_clone(owner));
    }
    
    /* Add library quota information if available */
    json_value_t* quota = json_object_get(library_doc, "quota");
    if (quota) {
        json_object_set(response, "quota", json_clone(quota));
    }
    
    /* CHECKPOINT: json_free(library_results); */
    BUFFER_FREE(token);
    jwt_free(jwt);
    
    char* response_str = json_stringify(response);
    /* CHECKPOINT: json_free(response); */
    
    return create_http_response(HTTP_OK, response_str, "application/json");
}

/**
 * Switch library context
 */
http_response_t* api_handle_switch_library(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !request || !request->body) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Parse request body */
    json_value_t* body = json_parse(request->body);
    if (!body || body->type != JSON_OBJECT) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid JSON body\"}", "application/json");
    }
    
    /* Get library name from request */
    json_value_t* library_val = json_object_get(body, "library");
    if (!library_val || library_val->type != JSON_STRING) {
        /* CHECKPOINT: json_free(body); */
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Library name required\"}", "application/json");
    }
    
    const char* new_library = library_val->value.string;
    
    /* Extract token */
    char* token = api_extract_token(request);
    if (!token) {
        /* CHECKPOINT: json_free(body); */
        return create_http_response(HTTP_UNAUTHORIZED,
                     "{\"error\":\"No authorization token\"}", "application/json");
    }
    
    /* Decode JWT to get user info */
    jwt_token_t* jwt = jwt_decode(token);
    if (!jwt || !jwt->payload) {
        BUFFER_FREE(token);
        /* CHECKPOINT: json_free(body); */
        if (jwt) jwt_free(jwt);
        return create_http_response(HTTP_UNAUTHORIZED,
                     "{\"error\":\"Invalid token\"}", "application/json");
    }
    
    /* Extract user info */
    const char* user_id = jwt->payload->sub;
    const char* username = NULL;
    
    if (jwt->payload->claims) {
        json_value_t* username_val = json_object_get(jwt->payload->claims, "username");
        if (username_val && username_val->type == JSON_STRING) {
            username = username_val->value.string;
        }
    }
    
    if (!user_id || !username) {
        BUFFER_FREE(token);
        jwt_free(jwt);
        /* CHECKPOINT: json_free(body); */
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                     "{\"error\":\"Invalid token claims\"}", "application/json");
    }
    
    /* Verify library exists */
    json_value_t* library_query = json_create_object();
    json_object_set(library_query, "type", json_create_string(DOC_TYPE_NAME_LIBRARY));
    json_object_set(library_query, "name", json_create_string(new_library));
    json_object_set(library_query, "library", json_create_string("system"));
    
    json_value_t* library_results = virtual_query(ctx->db, DOC_TYPE_NAME_LIBRARY, "system", VIRTUAL_COLLECTION_LIBRARIES, library_query);
    /* CHECKPOINT: json_free(library_query); */
    
    if (!library_results) {
        BUFFER_FREE(token);
        jwt_free(jwt);
        /* CHECKPOINT: json_free(body); */
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                     "{\"error\":\"Failed to query library\"}", "application/json");
    }
    
    json_value_t* docs = json_object_get(library_results, "documents");
    if (!docs || docs->type != JSON_ARRAY || json_array_size(docs) == 0) {
        /* CHECKPOINT: json_free(library_results); */
        BUFFER_FREE(token);
        jwt_free(jwt);
        /* CHECKPOINT: json_free(body); */
        return create_http_response(HTTP_NOT_FOUND,
                     "{\"error\":\"Library not found\"}", "application/json");
    }
    
    /* CHECKPOINT: json_free(library_results); */
    
    /* Create new JWT with updated library context */
    json_value_t* response_obj = json_create_object();
    
    /* Copy existing claims and update library */
    json_value_t* claims = json_create_object();
    json_object_set(claims, "username", json_create_string(username));
    json_object_set(claims, "library", json_create_string(new_library));
    
    /* Generate new token with updated claims */
    time_t now = time(NULL);
    
    /* Create JWT token structure */
    jwt_token_t* new_jwt = jwt_create(ctx->jwt_secret);
    if (!new_jwt) {
        /* CHECKPOINT: json_free(claims); */
        /* CHECKPOINT: json_free(response_obj); */
        BUFFER_FREE(token);
        jwt_free(jwt);
        /* CHECKPOINT: json_free(body); */
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                     "{\"error\":\"Failed to create JWT\"}", "application/json");
    }
    
    /* Set JWT payload */
    new_jwt->payload->sub = BUFFER_STRDUP(user_id);
    new_jwt->payload->iat = now;
    new_jwt->payload->exp = now + 3600;
    new_jwt->payload->claims = claims;
    
    /* Encode the token */
    char* new_token = jwt_encode(new_jwt, ctx->jwt_secret);
    
    if (!new_token) {
        /* CHECKPOINT: json_free(claims); - Note: claims ownership transferred to JWT */
        /* CHECKPOINT: json_free(response_obj); */
        jwt_free(new_jwt);
        BUFFER_FREE(token);
        jwt_free(jwt);
        /* CHECKPOINT: json_free(body); */
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                     "{\"error\":\"Failed to create new token\"}", "application/json");
    }
    
    /* Update session with new token */
    json_value_t* session = virtual_get_session_by_token(ctx->db, token);
    if (session) {
        json_value_t* session_id_val = json_object_get(session, "uuid");
        if (session_id_val && session_id_val->type == JSON_STRING) {
            /* Update session with new token and library */
            json_object_set(session, "token", json_create_string(new_token));
            json_object_set(session, "library", json_create_string(new_library));
            
            /* Update timestamp */
            time_t now = time(NULL);
            char timestamp[64];
            struct tm* utc_tm = gmtime(&now);
            strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", utc_tm);
            json_object_set(session, "updated_at", json_create_string(timestamp));
            
            /* Save updated session */
            virtual_update(ctx->db, session_id_val->value.string, session);
        }
        /* CHECKPOINT: json_free(session); */
    }
    
    /* Build response */
    json_object_set(response_obj, "token", json_create_string(new_token));
    json_object_set(response_obj, "library", json_create_string(new_library));
    json_object_set(response_obj, "message", json_create_string("Library switched successfully"));
    
    BUFFER_FREE(new_token);
    /* CHECKPOINT: json_free(claims); - Note: claims ownership transferred to JWT */
    jwt_free(new_jwt);
    BUFFER_FREE(token);
    jwt_free(jwt);
    /* CHECKPOINT: json_free(body); */
    
    char* response_str = json_stringify(response_obj);
    /* CHECKPOINT: json_free(response_obj); */
    
    return create_http_response(HTTP_OK, response_str, "application/json");
}

/**
 * Change user password
 */
http_response_t* api_handle_change_password(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !request || !request->body) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Parse request body */
    json_value_t* body = json_parse(request->body);
    if (!body || body->type != JSON_OBJECT) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid JSON body\"}", "application/json");
    }
    
    /* Get password fields */
    json_value_t* current_pass_val = json_object_get(body, "current_password");
    json_value_t* new_pass_val = json_object_get(body, "new_password");
    
    if (!current_pass_val || current_pass_val->type != JSON_STRING ||
        !new_pass_val || new_pass_val->type != JSON_STRING) {
        /* CHECKPOINT: json_free(body); */
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Current and new passwords required\"}", "application/json");
    }
    
    const char* current_password = current_pass_val->value.string;
    const char* new_password = new_pass_val->value.string;
    
    /* Validate new password */
    if (strlen(new_password) < 8) {
        /* CHECKPOINT: json_free(body); */
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"New password must be at least 8 characters\"}", "application/json");
    }
    
    /* Extract token */
    char* token = api_extract_token(request);
    if (!token) {
        /* CHECKPOINT: json_free(body); */
        return create_http_response(HTTP_UNAUTHORIZED,
                     "{\"error\":\"No authorization token\"}", "application/json");
    }
    
    /* Decode JWT to get user info */
    jwt_token_t* jwt = jwt_decode(token);
    if (!jwt || !jwt->payload) {
        BUFFER_FREE(token);
        /* CHECKPOINT: json_free(body); */
        if (jwt) jwt_free(jwt);
        return create_http_response(HTTP_UNAUTHORIZED,
                     "{\"error\":\"Invalid token\"}", "application/json");
    }
    
    const char* user_id = jwt->payload->sub;
    BUFFER_FREE(token);
    
    if (!user_id) {
        jwt_free(jwt);
        /* CHECKPOINT: json_free(body); */
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                     "{\"error\":\"Invalid token claims\"}", "application/json");
    }
    
    /* Get user document */
    json_value_t* user_doc = virtual_get(ctx->db, user_id);
    if (!user_doc) {
        jwt_free(jwt);
        /* CHECKPOINT: json_free(body); */
        return create_http_response(HTTP_NOT_FOUND,
                     "{\"error\":\"User not found\"}", "application/json");
    }
    
    /* Verify current password */
    json_value_t* password_hash_val = json_object_get(user_doc, "password_hash");
    if (!password_hash_val || password_hash_val->type != JSON_STRING) {
        /* CHECKPOINT: json_free(user_doc); */
        jwt_free(jwt);
        /* CHECKPOINT: json_free(body); */
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                     "{\"error\":\"User data corrupted\"}", "application/json");
    }
    
    const char* stored_hash = password_hash_val->value.string;
    
    /* Verify current password */
    extern int verify_password(const char* password, const char* hash);
    
    if (!verify_password(current_password, stored_hash)) {
        /* CHECKPOINT: json_free(user_doc); */
        jwt_free(jwt);
        /* CHECKPOINT: json_free(body); */
        return create_http_response(HTTP_UNAUTHORIZED,
                     "{\"error\":\"Current password is incorrect\"}", "application/json");
    }
    
    /* Hash new password */
    extern char* hash_password(const char* password);
    char* new_hash = hash_password(new_password);
    
    if (!new_hash) {
        /* CHECKPOINT: json_free(user_doc); */
        jwt_free(jwt);
        /* CHECKPOINT: json_free(body); */
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                     "{\"error\":\"Failed to hash new password\"}", "application/json");
    }
    
    /* Update user document with new password */
    json_object_set(user_doc, "password_hash", json_create_string(new_hash));
    
    /* Update timestamp */
    time_t now = time(NULL);
    char timestamp[64];
    struct tm* utc_tm = gmtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", utc_tm);
    json_object_set(user_doc, "updated_at", json_create_string(timestamp));
    
    /* Save updated user */
    json_value_t* update_result = virtual_update(ctx->db, user_id, user_doc);
    
    BUFFER_FREE(new_hash);
    /* CHECKPOINT: json_free(user_doc); */
    jwt_free(jwt);
    /* CHECKPOINT: json_free(body); */
    
    if (!update_result) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                     "{\"error\":\"Failed to update password\"}", "application/json");
    }
    
    /* CHECKPOINT: json_free(update_result); */
    
    return create_http_response(HTTP_OK,
                 "{\"success\":true,\"message\":\"Password changed successfully\"}", 
                 "application/json");
}

/**
 * Terminate specific session (DELETE method)
 */
http_response_t* api_handle_terminate_session(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !request) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract session ID from path */
    const char* path = request->path;
    if (strncmp(path, "/api/sessions/", 14) != 0) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid path\"}", "application/json");
    }
    
    const char* session_id = path + 14;
    if (!session_id || strlen(session_id) == 0) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Session ID required\"}", "application/json");
    }
    
    /* Extract token to verify user */
    char* token = api_extract_token(request);
    if (!token) {
        return create_http_response(HTTP_UNAUTHORIZED,
                     "{\"error\":\"No authorization token\"}", "application/json");
    }
    
    /* Decode JWT to get user info */
    jwt_token_t* jwt = jwt_decode(token);
    if (!jwt || !jwt->payload) {
        BUFFER_FREE(token);
        if (jwt) jwt_free(jwt);
        return create_http_response(HTTP_UNAUTHORIZED,
                     "{\"error\":\"Invalid token\"}", "application/json");
    }
    
    const char* user_id = jwt->payload->sub;
    BUFFER_FREE(token);
    
    if (!user_id) {
        jwt_free(jwt);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                     "{\"error\":\"Invalid token claims\"}", "application/json");
    }
    
    /* Get session document */
    json_value_t* session = virtual_get(ctx->db, session_id);
    if (!session) {
        jwt_free(jwt);
        return create_http_response(HTTP_NOT_FOUND,
                     "{\"error\":\"Session not found\"}", "application/json");
    }
    
    /* Verify ownership or admin permission */
    json_value_t* session_user_id_val = json_object_get(session, "user_id");
    if (!session_user_id_val || session_user_id_val->type != JSON_STRING) {
        /* CHECKPOINT: json_free(session); */
        jwt_free(jwt);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                     "{\"error\":\"Invalid session data\"}", "application/json");
    }
    
    const char* session_user_id = session_user_id_val->value.string;
    
    /* Check if user owns the session or is admin */
    int is_admin = rbac_check_permission(ctx->rbac, user_id, RBAC_DOCUMENT, "*", RBAC_DELETE);
    
    if (strcmp(user_id, session_user_id) != 0 && !is_admin) {
        /* CHECKPOINT: json_free(session); */
        jwt_free(jwt);
        return create_http_response(HTTP_FORBIDDEN,
                     "{\"error\":\"Permission denied\"}", "application/json");
    }
    
    /* Get session token for cache invalidation */
    json_value_t* session_token_val = json_object_get(session, "token");
    if (session_token_val && session_token_val->type == JSON_STRING) {
        const char* session_token = session_token_val->value.string;
        
        /* Invalidate JWT cache */
        jwt_cache_invalidate_token(session_token);
        LOG_INFO("Invalidated JWT cache for terminated session token");
    }
    
    /* CHECKPOINT: json_free(session); */
    jwt_free(jwt);
    
    /* Invalidate the session */
    if (rbac_db_invalidate_session(ctx->db, session_id)) {
        return create_http_response(HTTP_OK,
                     "{\"success\":true,\"message\":\"Session terminated successfully\"}", 
                     "application/json");
    }
    
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Failed to terminate session\"}", "application/json");
}

/**
 * Terminate specific session (POST method) - wrapper
 */
http_response_t* api_handle_session_terminate(api_context_t* ctx, http_request_t* request) {
  /* This is just a wrapper that calls the DELETE method handler */
  return api_handle_terminate_session(ctx, request);
}