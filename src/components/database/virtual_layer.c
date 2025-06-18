/**
 * Virtual Layer Implementation
 * 
 * This layer provides business logic operations on top of the storage layer.
 * It handles all the complexity of document types, UUID generation, and
 * field standardization so that application code doesn't have to.
 * 
 * CRITICAL: This is the ONLY layer that application code should use!
 */

#include "database/virtual_layer.h"
#include "database/document_storage.h"
#include "database/database.h"
#include "rbac/rbac_unified_documents.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include "utils/json.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* REMOVED: UUID generation and add_standard_fields - handled by generic virtual_insert in database.c (single source of truth) */

/* ===== USER MANAGEMENT ===== */

/**
 * Create a new user
 */
json_value_t* virtual_create_user(database_t* db, const char* username, 
                                 const char* password_hash, const char* library) {
    if (!db || !username || !password_hash) {
        LOG_ERROR("Invalid parameters for user creation");
        return NULL;
    }
    
    /* Check if user already exists */
    json_value_t* existing = virtual_get_user_by_name(db, username, library);
    if (existing) {
        LOG_ERROR("User already exists: %s in library %s", username, library);
        /* CHECKPOINT: json_free(existing); */
        return NULL;
    }
    
    /* Create user document */
    json_value_t* user_doc = json_create_object();
    if (!user_doc) {
        LOG_ERROR("Failed to create user document");
        return NULL;
    }
    
    /* Add user-specific fields */
    json_object_set(user_doc, "username", json_create_string(username));
    json_object_set(user_doc, "password_hash", json_create_string(password_hash));
    json_object_set(user_doc, "roles", json_create_array());
    json_object_set(user_doc, "active", json_create_boolean(1));
    
    /* Use generic virtual_insert from database.c - single source of truth */
    json_value_t* result = virtual_insert(db, DOC_TYPE_NAME_USER, library, "users", user_doc, username);
    /* CHECKPOINT: json_free(user_doc); */
    
    return result;
}

/**
 * Get user by UUID
 */
json_value_t* virtual_get_user_by_uuid(database_t* db, const char* uuid) {
    if (!db || !uuid) {
        return NULL;
    }
    
    /* Use generic virtual_get from database.c - single source of truth */
    return virtual_get(db, uuid);
}

/**
 * Get user by username
 */
json_value_t* virtual_get_user_by_name(database_t* db, const char* username, 
                                       const char* library) {
    if (!db || !username) {
        return NULL;
    }
    
    /* Build query */
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string(DOC_TYPE_NAME_USER));
    json_object_set(query, "library", json_create_string(library ? library : "default"));
    json_object_set(query, "username", json_create_string(username));
    
    /* Query storage */
    json_value_t* result = storage_query_documents(db, query);
    /* CHECKPOINT: json_free(query); */
    
    if (!result) {
        return NULL;
    }
    
    /* Extract first matching document */
    json_value_t* documents = json_object_get(result, "documents");
    json_value_t* user = NULL;
    
    if (documents && documents->type == JSON_ARRAY && documents->value.array.size > 0) {
        user = json_clone(documents->value.array.items[0]);
    }
    
    /* CHECKPOINT: json_free(result); */
    return user;
}

/**
 * Update user
 */
json_value_t* virtual_update_user(database_t* db, const char* uuid, 
                                 json_value_t* updates) {
    if (!db || !uuid || !updates) {
        return NULL;
    }
    
    /* Get existing user */
    json_value_t* user = virtual_get_user_by_uuid(db, uuid);
    if (!user) {
        LOG_ERROR("User not found: %s", uuid);
        return NULL;
    }
    
    /* Apply specific updates - only allow certain fields */
    json_value_t* password_hash = json_object_get(updates, "password_hash");
    if (password_hash) {
        json_object_set(user, "password_hash", json_clone(password_hash));
    }
    
    json_value_t* active = json_object_get(updates, "active");
    if (active) {
        json_object_set(user, "active", json_clone(active));
    }
    
    json_value_t* roles = json_object_get(updates, "roles");
    if (roles) {
        json_object_set(user, "roles", json_clone(roles));
    }
    
    /* Update modified timestamp */
    char timestamp[64];
    time_t now = time(NULL);
    struct tm* tm_info = gmtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", tm_info);
    json_object_set(user, "modified_at", json_create_string(timestamp));
    
    /* Update in storage */
    json_value_t* result = storage_update_document(db, uuid, user);
    /* CHECKPOINT: json_free(user); */
    
    return result;
}

/**
 * Delete user
 */
int virtual_delete_user(database_t* db, const char* uuid) {
    if (!db || !uuid) {
        return 0;
    }
    
    /* TODO: Remove user from all roles before deletion */
    
    /* Use generic virtual_delete from database.c - single source of truth */
    return virtual_delete(db, uuid);
}

/* REMOVED: virtual_query_users_new - using virtual_query_users from database.c (single source of truth) */

/* ===== ROLE MANAGEMENT ===== */

/**
 * Create a new role
 */
json_value_t* virtual_create_role(database_t* db, const char* name, 
                                 const char* library) {
    if (!db || !name) {
        LOG_ERROR("Invalid parameters for role creation");
        return NULL;
    }
    
    /* Check if role already exists */
    json_value_t* existing = virtual_get_role_by_name(db, name, library);
    if (existing) {
        LOG_ERROR("Role already exists: %s in library %s", name, library);
        /* CHECKPOINT: json_free(existing); */
        return NULL;
    }
    
    /* Create role document */
    json_value_t* role_doc = json_create_object();
    if (!role_doc) {
        LOG_ERROR("Failed to create role document");
        return NULL;
    }
    
    /* Add role-specific fields */
    json_object_set(role_doc, "name", json_create_string(name));
    json_object_set(role_doc, "permissions", json_create_object());
    json_object_set(role_doc, "users", json_create_array());
    
    /* Use generic virtual_insert from database.c - single source of truth */
    json_value_t* result = virtual_insert(db, DOC_TYPE_NAME_ROLE, library, "roles", role_doc, "system");
    /* CHECKPOINT: json_free(role_doc); */
    
    return result;
}

/**
 * Get role by UUID
 */
json_value_t* virtual_get_role_by_uuid(database_t* db, const char* uuid) {
    if (!db || !uuid) {
        return NULL;
    }
    
    /* Use generic virtual_get from database.c - single source of truth */
    return virtual_get(db, uuid);
}

/**
 * Get role by name
 */
json_value_t* virtual_get_role_by_name(database_t* db, const char* name, 
                                       const char* library) {
    if (!db || !name) {
        return NULL;
    }
    
    /* Build query filters */
    json_value_t* filters = json_create_object();
    json_object_set(filters, "name", json_create_string(name));
    
    /* Use generic virtual_query from database.c - single source of truth */
    json_value_t* result = virtual_query(db, DOC_TYPE_NAME_ROLE, library ? library : "default", "roles", filters);
    /* CHECKPOINT: json_free(filters); */
    
    if (!result) {
        return NULL;
    }
    
    /* Extract first matching document */
    json_value_t* documents = json_object_get(result, "documents");
    json_value_t* role = NULL;
    
    if (documents && documents->type == JSON_ARRAY && documents->value.array.size > 0) {
        role = json_clone(documents->value.array.items[0]);
    }
    
    /* CHECKPOINT: json_free(result); */
    return role;
}

/* ===== SESSION MANAGEMENT ===== */

/**
 * Create a new session
 */
json_value_t* virtual_create_session(database_t* db, const char* user_uuid, 
                                    const char* token, time_t expires_at,
                                    const char* ip_address, const char* user_agent) {
    if (!db || !user_uuid || !token) {
        LOG_ERROR("Invalid parameters for session creation");
        return NULL;
    }
    
    /* Get user to add username to session */
    json_value_t* user = virtual_get_user_by_uuid(db, user_uuid);
    if (!user) {
        LOG_ERROR("User not found for session: %s", user_uuid);
        return NULL;
    }
    
    json_value_t* username_val = json_object_get(user, "username");
    const char* username = username_val && username_val->type == JSON_STRING ? 
                          username_val->value.string : "unknown";
    
    /* Create session document */
    json_value_t* session_doc = json_create_object();
    if (!session_doc) {
        /* CHECKPOINT: json_free(user); */
        LOG_ERROR("Failed to create session document");
        return NULL;
    }
    
    /* Add session-specific fields */
    json_object_set(session_doc, "user_id", json_create_string(user_uuid));
    json_object_set(session_doc, "username", json_create_string(username));
    json_object_set(session_doc, "token", json_create_string(token));
    json_object_set(session_doc, "active", json_create_boolean(1));
    
    /* Add IP and user agent if provided */
    if (ip_address) {
        json_object_set(session_doc, "ip_address", json_create_string(ip_address));
    }
    if (user_agent) {
        json_object_set(session_doc, "user_agent", json_create_string(user_agent));
    }
    
    /* Add expiration */
    char expire_time[64];
    struct tm* tm_info = gmtime(&expires_at);
    strftime(expire_time, sizeof(expire_time), "%Y-%m-%dT%H:%M:%SZ", tm_info);
    json_object_set(session_doc, "expires_at", json_create_string(expire_time));
    
    /* CHECKPOINT: json_free(user); */
    
    /* Use generic virtual_insert from database.c - single source of truth */
    json_value_t* result = virtual_insert(db, DOC_TYPE_NAME_SESSION, "system", "sessions", session_doc, username);
    /* CHECKPOINT: json_free(session_doc); */
    
    return result;
}

/**
 * Get session by token
 */
json_value_t* virtual_get_session_by_token(database_t* db, const char* token) {
    if (!db || !token) {
        return NULL;
    }
    
    /* Build query filters */
    json_value_t* filters = json_create_object();
    json_object_set(filters, "token", json_create_string(token));
    json_object_set(filters, "active", json_create_boolean(1));
    
    /* Use generic virtual_query from database.c - single source of truth */
    json_value_t* result = virtual_query(db, DOC_TYPE_NAME_SESSION, "system", "sessions", filters);
    /* CHECKPOINT: json_free(filters); */
    
    if (!result) {
        return NULL;
    }
    
    /* Extract first matching document */
    json_value_t* documents = json_object_get(result, "documents");
    json_value_t* session = NULL;
    
    if (documents && documents->type == JSON_ARRAY && documents->value.array.size > 0) {
        session = json_clone(documents->value.array.items[0]);
    }
    
    /* CHECKPOINT: json_free(result); */
    return session;
}