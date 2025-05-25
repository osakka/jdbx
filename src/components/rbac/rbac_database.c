#include "rbac/rbac.h"
#include "rbac/rbac_db.h"
#include "database/database.h"
#include "utils/logger.h"
#include "utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* RBAC collection names */
#define RBAC_USERS_COLLECTION "_users"
#define RBAC_ROLES_COLLECTION "_roles"
#define RBAC_PERMISSIONS_COLLECTION "_permissions"
#define RBAC_COLLECTIONS_COLLECTION "_collections"
#define RBAC_SESSIONS_COLLECTION "_sessions"
#define RBAC_PERMISSION_CACHE_COLLECTION "_permission_cache"

/* Permission cache TTL in seconds (5 minutes) */
#define PERMISSION_CACHE_TTL 300

/* Initialize RBAC database collections */
static int init_rbac_collections(struct database* db) {
    LOG_TRACE("RBAC_DB: Initializing RBAC collections");
    
    const char* collections[] = {
        RBAC_USERS_COLLECTION,
        RBAC_ROLES_COLLECTION,
        RBAC_PERMISSIONS_COLLECTION,
        RBAC_COLLECTIONS_COLLECTION,
        RBAC_SESSIONS_COLLECTION,
        RBAC_PERMISSION_CACHE_COLLECTION
    };
    
    for (size_t i = 0; i < sizeof(collections) / sizeof(collections[0]); i++) {
        if (!db_collection_exists(db, collections[i])) {
            LOG_TRACE("RBAC_DB: Creating collection: %s", collections[i]);
            if (!db_create_collection(db, collections[i])) {
                LOG_ERROR("RBAC_DB: Failed to create collection: %s", collections[i]);
                return 0;
            }
        } else {
            LOG_TRACE("RBAC_DB: Collection already exists: %s", collections[i]);
        }
    }
    
    LOG_TRACE("RBAC_DB: All RBAC collections initialized");
    return 1;
}

/* Forward declarations */
static int create_default_admin_role(struct database* db);
static int create_default_admin_user(struct database* db);

/* Initialize RBAC system with database backend */
rbac_system_t* rbac_database_init(struct database* db, const char* jwt_secret) {
    LOG_TRACE("RBAC_DB: Initializing database-backed RBAC system");
    LOG_TRACE("RBAC_DB: Database pointer: %p", db);
    LOG_TRACE("RBAC_DB: JWT secret: %s", jwt_secret ? "[PROVIDED]" : "[NULL]");
    
    if (!db) {
        LOG_ERROR("RBAC_DB: Database is NULL");
        return NULL;
    }
    
    /* Initialize RBAC collections */
    if (!init_rbac_collections(db)) {
        LOG_ERROR("RBAC_DB: Failed to initialize RBAC collections");
        return NULL;
    }
    
    /* Create RBAC system structure */
    rbac_system_t* rbac = (rbac_system_t*)malloc(sizeof(rbac_system_t));
    if (!rbac) {
        LOG_ERROR("RBAC_DB: Failed to allocate memory for RBAC system");
        return NULL;
    }
    
    /* Initialize with empty in-memory structures (we'll use database directly) */
    rbac->users = json_create_object();
    rbac->roles = json_create_object();
    rbac->db = db;
    rbac->jwt_secret = jwt_secret ? strdup(jwt_secret) : strdup("change-this-secret-in-production");
    
    if (!rbac->users || !rbac->roles || !rbac->jwt_secret) {
        LOG_ERROR("RBAC_DB: Failed to initialize RBAC fields");
        if (rbac->users) json_free(rbac->users);
        if (rbac->roles) json_free(rbac->roles);
        if (rbac->jwt_secret) free(rbac->jwt_secret);
        free(rbac);
        return NULL;
    }
    
    /* Create default admin role if it doesn't exist */
    if (!create_default_admin_role(db)) {
        LOG_ERROR("RBAC_DB: Failed to create default admin role");
        json_free(rbac->users);
        json_free(rbac->roles);
        free(rbac);
        return NULL;
    }
    
    /* Create default admin user if it doesn't exist */
    if (!create_default_admin_user(db)) {
        LOG_ERROR("RBAC_DB: Failed to create default admin user");
        json_free(rbac->users);
        json_free(rbac->roles);
        free(rbac);
        return NULL;
    }
    
    LOG_TRACE("RBAC_DB: Database-backed RBAC system initialized successfully");
    return rbac;
}

/* Create default admin role */
int create_default_admin_role(struct database* db) {
    LOG_TRACE("RBAC_DB: Creating default admin role");
    
    /* Check if admin role already exists */
    json_value_t* query = json_create_object();
    json_object_set(query, "name", json_create_string("admin"));
    
    json_value_t* existing = db_query_documents(db, RBAC_ROLES_COLLECTION, query);
    json_free(query);
    
    if (existing && json_object_get(existing, "count") && 
        json_object_get(existing, "count")->value.number > 0) {
        LOG_TRACE("RBAC_DB: Admin role already exists");
        json_free(existing);
        return 1;
    }
    if (existing) json_free(existing);
    
    /* Create admin role document */
    json_value_t* admin_role = json_create_object();
    json_object_set(admin_role, "name", json_create_string("admin"));
    json_object_set(admin_role, "description", json_create_string("System administrator with full access"));
    
    /* Set permissions - admin has all permissions on everything */
    json_value_t* permissions = json_create_object();
    json_value_t* collections = json_create_object();
    json_value_t* all_perms = json_create_array();
    
    json_array_append(all_perms, json_create_string("CREATE"));
    json_array_append(all_perms, json_create_string("READ"));
    json_array_append(all_perms, json_create_string("UPDATE"));
    json_array_append(all_perms, json_create_string("DELETE"));
    json_array_append(all_perms, json_create_string("ADMIN"));
    
    json_object_set(collections, "*", all_perms);
    json_object_set(permissions, "collections", collections);
    
    json_value_t* system_perms = json_create_array();
    json_array_append(system_perms, json_create_string("*"));
    json_object_set(permissions, "system", system_perms);
    
    json_object_set(admin_role, "permissions", permissions);
    
    /* Add timestamps */
    char timestamp[64];
    time_t now = time(NULL);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    json_object_set(admin_role, "created_at", json_create_string(timestamp));
    json_object_set(admin_role, "updated_at", json_create_string(timestamp));
    
    /* Insert role */
    json_value_t* result = db_insert_document(db, RBAC_ROLES_COLLECTION, admin_role);
    json_free(admin_role);
    
    if (!result) {
        LOG_ERROR("RBAC_DB: Failed to create admin role");
        return 0;
    }
    
    const char* role_id = json_get_string(json_object_get(result, "_id"));
    json_free(result);
    
    LOG_TRACE("RBAC_DB: Admin role created with ID: %s", role_id ? role_id : "(null)");
    return 1;
}

/* Create default admin user */
int create_default_admin_user(struct database* db) {
    LOG_TRACE("RBAC_DB: Creating default admin user");
    
    /* Check if admin user already exists */
    json_value_t* query = json_create_object();
    json_object_set(query, "username", json_create_string("admin"));
    
    json_value_t* existing = db_query_documents(db, RBAC_USERS_COLLECTION, query);
    json_free(query);
    
    if (existing && json_object_get(existing, "count") && 
        json_object_get(existing, "count")->value.number > 0) {
        LOG_TRACE("RBAC_DB: Admin user already exists");
        json_free(existing);
        return 1;
    }
    if (existing) json_free(existing);
    
    /* Get admin role ID */
    query = json_create_object();
    json_object_set(query, "name", json_create_string("admin"));
    
    json_value_t* roles = db_query_documents(db, RBAC_ROLES_COLLECTION, query);
    json_free(query);
    
    if (!roles || !json_object_get(roles, "documents")) {
        LOG_ERROR("RBAC_DB: Admin role not found");
        if (roles) json_free(roles);
        return 0;
    }
    
    json_value_t* docs = json_object_get(roles, "documents");
    if (docs->value.array.size == 0) {
        LOG_ERROR("RBAC_DB: Admin role not found in results");
        json_free(roles);
        return 0;
    }
    
    json_value_t* admin_role = docs->value.array.items[0];
    json_value_t* role_id_val = json_object_get(admin_role, "_id");
    if (!role_id_val) {
        LOG_ERROR("RBAC_DB: Admin role missing _id");
        json_free(roles);
        return 0;
    }
    
    const char* admin_role_id = role_id_val->value.string;
    
    /* Create admin user document */
    json_value_t* admin_user = json_create_object();
    json_object_set(admin_user, "username", json_create_string("admin"));
    json_object_set(admin_user, "email", json_create_string("admin@localhost"));
    
    /* Hash the default password "admin" */
    char* password_hash = hash_password("admin");
    if (!password_hash) {
        LOG_ERROR("RBAC_DB: Failed to hash password");
        json_free(admin_user);
        json_free(roles);
        return 0;
    }
    
    json_object_set(admin_user, "password_hash", json_create_string(password_hash));
    free(password_hash);
    
    /* Add admin role */
    json_value_t* user_roles = json_create_array();
    json_array_append(user_roles, json_create_string(admin_role_id));
    json_object_set(admin_user, "roles", user_roles);
    
    /* Add metadata */
    json_object_set(admin_user, "active", json_create_boolean(1));
    
    char timestamp[64];
    time_t now = time(NULL);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    json_object_set(admin_user, "created_at", json_create_string(timestamp));
    json_object_set(admin_user, "updated_at", json_create_string(timestamp));
    
    /* Insert user */
    json_value_t* user_result = db_insert_document(db, RBAC_USERS_COLLECTION, admin_user);
    json_free(admin_user);
    json_free(roles);
    
    if (!user_result) {
        LOG_ERROR("RBAC_DB: Failed to create admin user");
        return 0;
    }
    
    const char* user_id = json_get_string(json_object_get(user_result, "_id"));
    LOG_TRACE("RBAC_DB: Admin user created with ID: %s", user_id ? user_id : "(null)");
    json_free(user_result);
    return 1;
}

/* Get user by username from database */
rbac_user_t* rbac_database_get_user_by_username(struct database* db, const char* username) {
    LOG_TRACE("RBAC_DB: Getting user by username: %s", username);
    
    if (!db || !username) {
        LOG_ERROR("RBAC_DB: Invalid parameters");
        return NULL;
    }
    
    /* Query for user */
    json_value_t* query = json_create_object();
    json_object_set(query, "username", json_create_string(username));
    
    json_value_t* result = db_query_documents(db, RBAC_USERS_COLLECTION, query);
    json_free(query);
    
    if (!result) {
        LOG_ERROR("RBAC_DB: Query failed");
        return NULL;
    }
    
    /* Check if user found */
    json_value_t* docs = json_object_get(result, "documents");
    if (!docs || docs->value.array.size == 0) {
        LOG_TRACE("RBAC_DB: User not found: %s", username);
        json_free(result);
        return NULL;
    }
    
    /* Get first user document */
    json_value_t* user_doc = docs->value.array.items[0];
    
    /* Create rbac_user_t structure */
    rbac_user_t* user = (rbac_user_t*)malloc(sizeof(rbac_user_t));
    if (!user) {
        LOG_ERROR("RBAC_DB: Failed to allocate memory for user");
        json_free(result);
        return NULL;
    }
    
    /* Extract user data */
    json_value_t* id_val = json_object_get(user_doc, "_id");
    json_value_t* username_val = json_object_get(user_doc, "username");
    json_value_t* password_hash_val = json_object_get(user_doc, "password_hash");
    json_value_t* roles_val = json_object_get(user_doc, "roles");
    
    user->id = strdup(id_val ? id_val->value.string : "");
    user->username = strdup(username_val ? username_val->value.string : username);
    user->password_hash = strdup(password_hash_val ? password_hash_val->value.string : "");
    
    /* Copy roles array */
    if (roles_val && roles_val->type == JSON_ARRAY) {
        user->roles = json_clone(roles_val);
    } else {
        user->roles = json_create_array();
    }
    
    json_free(result);
    
    LOG_TRACE("RBAC_DB: User found with ID: %s", user->id);
    return user;
}

/* Create new user in database */
rbac_user_t* rbac_database_create_user(struct database* db, const char* username, const char* password) {
    LOG_TRACE("RBAC_DB: Creating user: %s", username);
    
    if (!db || !username || !password) {
        LOG_ERROR("RBAC_DB: Invalid parameters");
        return NULL;
    }
    
    /* Check if user already exists */
    rbac_user_t* existing = rbac_db_get_user_by_username(db, username);
    if (existing) {
        LOG_ERROR("RBAC_DB: User already exists: %s", username);
        rbac_free_user(existing);
        return NULL;
    }
    
    /* Hash password */
    char* password_hash = hash_password(password);
    if (!password_hash) {
        LOG_ERROR("RBAC_DB: Failed to hash password");
        return NULL;
    }
    
    /* Create user document */
    json_value_t* user_doc = json_create_object();
    json_object_set(user_doc, "username", json_create_string(username));
    json_object_set(user_doc, "password_hash", json_create_string(password_hash));
    json_object_set(user_doc, "roles", json_create_array());
    json_object_set(user_doc, "active", json_create_boolean(1));
    
    /* Add timestamps */
    char timestamp[64];
    time_t now = time(NULL);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    json_object_set(user_doc, "created_at", json_create_string(timestamp));
    json_object_set(user_doc, "updated_at", json_create_string(timestamp));
    
    /* Insert user */
    json_value_t* insert_result = db_insert_document(db, RBAC_USERS_COLLECTION, user_doc);
    json_free(user_doc);
    
    if (!insert_result) {
        LOG_ERROR("RBAC_DB: Failed to insert user");
        free(password_hash);
        return NULL;
    }
    
    /* Extract user ID from result */
    const char* user_id = json_get_string(json_object_get(insert_result, "_id"));
    json_free(insert_result);
    
    /* Create rbac_user_t structure */
    rbac_user_t* user = (rbac_user_t*)malloc(sizeof(rbac_user_t));
    if (!user) {
        LOG_ERROR("RBAC_DB: Failed to allocate memory for user");
        free(user_id);
        free(password_hash);
        return NULL;
    }
    
    user->id = user_id;
    user->username = strdup(username);
    user->password_hash = password_hash;
    user->roles = json_create_array();
    
    LOG_TRACE("RBAC_DB: User created with ID: %s", user_id);
    return user;
}

/* Check user permissions on a resource */
int rbac_database_check_permission(struct database* db, const char* user_id, rbac_resource_type_t resource_type,
                           const char* resource_id, rbac_permission_t permission) {
    /* Convert resource type to string */
    const char* resource_type_str = "unknown";
    switch (resource_type) {
        case RBAC_DATABASE: resource_type_str = "database"; break;
        case RBAC_COLLECTION: resource_type_str = "collection"; break;
        case RBAC_DOCUMENT: resource_type_str = "document"; break;
        case RBAC_USER: resource_type_str = "user"; break;
        case RBAC_ROLE: resource_type_str = "role"; break;
        case RBAC_PERMISSION: resource_type_str = "permission"; break;
        default: break;
    }
    
    /* Convert permission to string */
    const char* permission_str = "unknown";
    if (permission & RBAC_READ) permission_str = "READ";
    else if (permission & RBAC_WRITE) permission_str = "WRITE";
    else if (permission & RBAC_DELETE) permission_str = "DELETE";
    else if (permission & RBAC_ADMIN) permission_str = "ADMIN";
    
    LOG_TRACE("RBAC_DB: Checking permission - user: %s, resource: %s:%s, permission: %s",
              user_id, resource_type_str, resource_id, permission_str);
    
    if (!db || !user_id || !resource_id) {
        LOG_ERROR("RBAC_DB: Invalid parameters");
        return 0;
    }
    
    /* First check permission cache */
    char cache_key[256];
    snprintf(cache_key, sizeof(cache_key), "%s:%s:%s", user_id, resource_type_str, resource_id);
    
    json_value_t* cache_query = json_create_object();
    json_object_set(cache_query, "_id", json_create_string(cache_key));
    
    json_value_t* cache_result = db_query_documents(db, RBAC_PERMISSION_CACHE_COLLECTION, cache_query);
    json_free(cache_query);
    
    if (cache_result) {
        json_value_t* docs = json_object_get(cache_result, "documents");
        if (docs && docs->value.array.size > 0) {
            json_value_t* cache_doc = docs->value.array.items[0];
            json_value_t* expires_at = json_object_get(cache_doc, "expires_at");
            
            /* Check if cache is still valid */
            if (expires_at) {
                time_t now = time(NULL);
                time_t expiry = (time_t)expires_at->value.number;
                
                if (now < expiry) {
                    LOG_TRACE("RBAC_DB: Using cached permissions");
                    json_value_t* perms = json_object_get(cache_doc, "permissions");
                    
                    if (perms && perms->type == JSON_ARRAY) {
                        for (size_t i = 0; i < perms->value.array.size; i++) {
                            json_value_t* perm = perms->value.array.items[i];
                            if (perm->type == JSON_STRING && 
                                strcmp(perm->value.string, permission_str) == 0) {
                                LOG_TRACE("RBAC_DB: Permission granted (cached)");
                                json_free(cache_result);
                                return 1;
                            }
                        }
                    }
                    
                    LOG_TRACE("RBAC_DB: Permission denied (cached)");
                    json_free(cache_result);
                    return 0;
                }
            }
        }
        json_free(cache_result);
    }
    
    /* Cache miss or expired - compute permissions */
    LOG_TRACE("RBAC_DB: Computing permissions from database");
    
    /* TODO: Implement full permission resolution algorithm */
    /* For now, just check if user has admin role */
    json_value_t* user_query = json_create_object();
    json_object_set(user_query, "id", json_create_string(user_id));
    
    LOG_TRACE("RBAC_DB: Querying user with id: %s", user_id);
    
    json_value_t* user_result = db_query_documents(db, RBAC_USERS_COLLECTION, user_query);
    json_free(user_query);
    
    if (!user_result) {
        LOG_ERROR("RBAC_DB: Failed to query user");
        return 0;
    }
    
    json_value_t* user_docs = json_object_get(user_result, "documents");
    if (!user_docs || user_docs->value.array.size == 0) {
        LOG_ERROR("RBAC_DB: User not found with id: %s", user_id);
        LOG_TRACE("RBAC_DB: Query result: %s", json_stringify(user_result));
        json_free(user_result);
        return 0;
    }
    
    LOG_TRACE("RBAC_DB: Found %zu users", user_docs->value.array.size);
    
    /* For now, grant all permissions to users with admin role */
    json_value_t* user_doc = user_docs->value.array.items[0];
    json_value_t* roles = json_object_get(user_doc, "roles");
    
    int has_permission = 0;
    if (roles && roles->type == JSON_ARRAY) {
        LOG_TRACE("RBAC_DB: User has %zu roles", roles->value.array.size);
        /* Check if user has admin role */
        for (size_t i = 0; i < roles->value.array.size; i++) {
            /* In a full implementation, we would look up each role and check permissions */
            /* For now, we just check if they have any role (simplified) */
            LOG_TRACE("RBAC_DB: Temporary - granting permission because user has roles");
            has_permission = 1;
            break;
        }
    } else {
        LOG_TRACE("RBAC_DB: User has no roles");
    }
    
    json_free(user_result);
    
    LOG_TRACE("RBAC_DB: Permission %s", has_permission ? "granted" : "denied");
    return has_permission;
}