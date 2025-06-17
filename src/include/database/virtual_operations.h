#ifndef VIRTUAL_OPERATIONS_H
#define VIRTUAL_OPERATIONS_H

#include "utils/json.h"
#include "database/database.h"

/**
 * VIRTUAL LAYER OPERATIONS
 * 
 * These functions implement business logic on top of the storage layer.
 * They understand concepts like "users", "roles", etc. and automatically
 * handle the type discrimination needed for unified storage.
 * 
 * NAMING CONVENTION:
 * - All virtual layer functions start with "virtual_"
 * - This makes it immediately obvious which layer you're using
 * 
 * NEVER mix virtual_*() calls with direct storage operations!
 */

/* User operations - automatically sets type="user" */
json_value_t* virtual_create_user(database_t* db, const char* username, 
                                  const char* password, const char* library);
json_value_t* virtual_query_users(database_t* db, const char* library, 
                                  json_value_t* additional_filters);
json_value_t* virtual_get_user(database_t* db, const char* username, 
                               const char* library);
int virtual_update_user(database_t* db, const char* username, 
                       const char* library, json_value_t* updates);
int virtual_delete_user(database_t* db, const char* username, 
                       const char* library);

/* Role operations - automatically sets type="role" */
json_value_t* virtual_create_role(database_t* db, const char* role_name, 
                                  json_value_t* permissions, const char* library);
json_value_t* virtual_query_roles(database_t* db, const char* library, 
                                  json_value_t* additional_filters);
json_value_t* virtual_get_role(database_t* db, const char* role_name, 
                               const char* library);

/* Session operations - automatically sets type="session" */
json_value_t* virtual_create_session(database_t* db, const char* user_id, 
                                     const char* token, const char* library);
json_value_t* virtual_query_sessions(database_t* db, const char* library, 
                                     json_value_t* additional_filters);
int virtual_delete_session(database_t* db, const char* token);

/* Collection metadata operations - automatically sets type="collection" */
json_value_t* virtual_create_collection_metadata(database_t* db, 
                                                const char* collection_name,
                                                const char* library);
json_value_t* virtual_query_collections(database_t* db, const char* library);

/**
 * Implementation helper macro - ensures consistent type setting
 */
#define VIRTUAL_ENSURE_TYPE(query, type_value) do { \
    if (!json_object_get(query, "type")) { \
        json_object_set(query, "type", json_create_string(type_value)); \
    } \
} while(0)

#endif /* VIRTUAL_OPERATIONS_H */