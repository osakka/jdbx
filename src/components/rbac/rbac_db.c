#include "rbac/rbac_db.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Generate a simple UUID */
static char* generate_uuid() {
    char* uuid = (char*)malloc(37);  /* 36 chars + null terminator */
    if (!uuid) return NULL;
    
    /* Format: 8-4-4-4-12 hexadecimal digits */
    srand((unsigned int)time(NULL) + rand());
    sprintf(uuid, "%08x-%04x-%04x-%04x-%04x%08x",
            rand() & 0xFFFFFFFF,
            rand() & 0xFFFF,
            ((rand() & 0xFFFF) & 0x0FFF) | 0x4000,  /* Version 4 */
            ((rand() & 0xFFFF) & 0x3FFF) | 0x8000,  /* Variant 1 */
            rand() & 0xFFFF,
            rand() & 0xFFFFFFFF);
    
    return uuid;
}

/* Initialize RBAC collections in the database */
rbac_db_status_t rbac_db_init_collections(database_t* db) {
    rbac_db_status_t status = {0};
    
    if (!db) {
        status.error_message = strdup("Database is NULL");
        return status;
    }
    
    /* Create _system collection if it doesn't exist */
    if (!db_get_collection(db, RBAC_CONFIG_COLLECTION)) {
        if (!db_create_collection(db, RBAC_CONFIG_COLLECTION)) {
            status.error_message = strdup("Failed to create _system collection");
            return status;
        }
        status.collections_created++;
    }
    
    /* Create _users collection if it doesn't exist */
    if (!db_get_collection(db, RBAC_USERS_COLLECTION)) {
        if (!db_create_collection(db, RBAC_USERS_COLLECTION)) {
            status.error_message = strdup("Failed to create _users collection");
            return status;
        }
        status.collections_created++;
        
        /* Create indexes for _users collection */
        if (!db_create_index(db, RBAC_USERS_COLLECTION, "idx_user_id", "id", INDEX_TYPE_UNIQUE)) {
            status.error_message = strdup("Failed to create user ID index");
            return status;
        }
        status.indexes_created++;
        
        if (!db_create_index(db, RBAC_USERS_COLLECTION, "idx_username", "username", INDEX_TYPE_UNIQUE)) {
            status.error_message = strdup("Failed to create username index");
            return status;
        }
        status.indexes_created++;
    }
    
    /* Create _roles collection if it doesn't exist */
    if (!db_get_collection(db, RBAC_ROLES_COLLECTION)) {
        if (!db_create_collection(db, RBAC_ROLES_COLLECTION)) {
            status.error_message = strdup("Failed to create _roles collection");
            return status;
        }
        status.collections_created++;
        
        /* Create indexes for _roles collection */
        if (!db_create_index(db, RBAC_ROLES_COLLECTION, "idx_role_id", "id", INDEX_TYPE_UNIQUE)) {
            status.error_message = strdup("Failed to create role ID index");
            return status;
        }
        status.indexes_created++;
        
        if (!db_create_index(db, RBAC_ROLES_COLLECTION, "idx_role_name", "name", INDEX_TYPE_UNIQUE)) {
            status.error_message = strdup("Failed to create role name index");
            return status;
        }
        status.indexes_created++;
    }
    
    status.success = 1;
    return status;
}

/* Check if RBAC exists in database */
int rbac_db_exists(database_t* db) {
    if (!db) {
        return 0;
    }
    
    /* Check if all required collections exist */
    if (!db_get_collection(db, RBAC_CONFIG_COLLECTION) ||
        !db_get_collection(db, RBAC_USERS_COLLECTION) ||
        !db_get_collection(db, RBAC_ROLES_COLLECTION)) {
        return 0;
    }
    
    /* Check if users collection has at least one user */
    json_value_t* query = json_create_object();
    json_value_t* result = db_query_documents(db, RBAC_USERS_COLLECTION, query);
    json_free(query);
    
    if (!result || result->type != JSON_ARRAY || result->value.array.size == 0) {
        if (result) json_free(result);
        return 0;
    }
    
    json_free(result);
    return 1;
}

/* Convert user document to rbac_user_t */
static rbac_user_t* user_doc_to_rbac_user(json_value_t* user_doc) {
    if (!user_doc || user_doc->type != JSON_OBJECT) {
        return NULL;
    }
    
    /* Get user fields */
    json_value_t* id_val = json_object_get(user_doc, "id");
    json_value_t* username_val = json_object_get(user_doc, "username");
    json_value_t* password_hash_val = json_object_get(user_doc, "password_hash");
    json_value_t* roles_val = json_object_get(user_doc, "roles");
    
    if (!id_val || id_val->type != JSON_STRING ||
        !username_val || username_val->type != JSON_STRING ||
        !password_hash_val || password_hash_val->type != JSON_STRING ||
        !roles_val || roles_val->type != JSON_ARRAY) {
        return NULL;
    }
    
    /* Create user structure */
    rbac_user_t* user = (rbac_user_t*)malloc(sizeof(rbac_user_t));
    if (!user) {
        return NULL;
    }
    
    /* Set user fields */
    user->id = strdup(id_val->value.string);
    user->username = strdup(username_val->value.string);
    user->password_hash = strdup(password_hash_val->value.string);
    
    /* Copy roles array */
    user->roles = json_create_array();
    for (size_t i = 0; i < roles_val->value.array.size; i++) {
        json_value_t* role_id = roles_val->value.array.items[i];
        if (role_id->type == JSON_STRING) {
            json_array_append(user->roles, json_create_string(role_id->value.string));
        }
    }
    
    return user;
}

/* Convert role document to rbac_role_t */
static rbac_role_t* role_doc_to_rbac_role(json_value_t* role_doc) {
    if (!role_doc || role_doc->type != JSON_OBJECT) {
        return NULL;
    }
    
    /* Get role fields */
    json_value_t* id_val = json_object_get(role_doc, "id");
    json_value_t* name_val = json_object_get(role_doc, "name");
    json_value_t* permissions_val = json_object_get(role_doc, "permissions");
    
    if (!id_val || id_val->type != JSON_STRING ||
        !name_val || name_val->type != JSON_STRING ||
        !permissions_val || permissions_val->type != JSON_OBJECT) {
        return NULL;
    }
    
    /* Create role structure */
    rbac_role_t* role = (rbac_role_t*)malloc(sizeof(rbac_role_t));
    if (!role) {
        return NULL;
    }
    
    /* Set role fields */
    role->id = strdup(id_val->value.string);
    role->name = strdup(name_val->value.string);
    
    /* Deep copy permissions */
    char* permissions_str = json_stringify(permissions_val);
    role->permissions = json_parse(permissions_str);
    free(permissions_str);
    
    return role;
}

/* Convert rbac_user_t to user document */
static json_value_t* rbac_user_to_doc(rbac_user_t* user) {
    if (!user) {
        return NULL;
    }
    
    /* Create user document */
    json_value_t* user_doc = json_create_object();
    if (!user_doc) {
        return NULL;
    }
    
    /* Set user fields */
    json_object_set(user_doc, "id", json_create_string(user->id));
    json_object_set(user_doc, "username", json_create_string(user->username));
    json_object_set(user_doc, "password_hash", json_create_string(user->password_hash));
    
    /* Copy roles array */
    json_value_t* roles = json_create_array();
    for (size_t i = 0; i < user->roles->value.array.size; i++) {
        json_value_t* role_id = user->roles->value.array.items[i];
        if (role_id->type == JSON_STRING) {
            json_array_append(roles, json_create_string(role_id->value.string));
        }
    }
    json_object_set(user_doc, "roles", roles);
    
    return user_doc;
}

/* Convert rbac_role_t to role document */
static json_value_t* rbac_role_to_doc(rbac_role_t* role) {
    if (!role) {
        return NULL;
    }
    
    /* Create role document */
    json_value_t* role_doc = json_create_object();
    if (!role_doc) {
        return NULL;
    }
    
    /* Set role fields */
    json_object_set(role_doc, "id", json_create_string(role->id));
    json_object_set(role_doc, "name", json_create_string(role->name));
    
    /* Deep copy permissions */
    char* permissions_str = json_stringify(role->permissions);
    json_value_t* permissions = json_parse(permissions_str);
    free(permissions_str);
    json_object_set(role_doc, "permissions", permissions);
    
    /* Add empty users array */
    json_object_set(role_doc, "users", json_create_array());
    
    return role_doc;
}

/* Load RBAC system from database */
rbac_system_t* rbac_db_load(database_t* db) {
    if (!db) {
        return NULL;
    }
    
    /* Check if RBAC collections exist */
    if (!rbac_db_exists(db)) {
        LOG_WARNING("RBAC collections not found in database");
        return NULL;
    }
    
    /* Create RBAC system */
    rbac_system_t* rbac = (rbac_system_t*)malloc(sizeof(rbac_system_t));
    if (!rbac) {
        return NULL;
    }
    
    /* Initialize users and roles containers */
    rbac->users = json_create_object();
    rbac->roles = json_create_object();
    
    if (!rbac->users || !rbac->roles) {
        if (rbac->users) json_free(rbac->users);
        if (rbac->roles) json_free(rbac->roles);
        free(rbac);
        return NULL;
    }
    
    /* Load all users */
    json_value_t* all_users_query = json_create_object();
    json_value_t* users_result = db_query_documents(db, RBAC_USERS_COLLECTION, all_users_query);
    json_free(all_users_query);
    
    if (!users_result || users_result->type != JSON_ARRAY) {
        if (users_result) json_free(users_result);
        rbac_free(rbac);
        return NULL;
    }
    
    /* Add each user to users object */
    for (size_t i = 0; i < users_result->value.array.size; i++) {
        json_value_t* user_doc = users_result->value.array.items[i];
        if (user_doc->type == JSON_OBJECT) {
            json_value_t* id_val = json_object_get(user_doc, "id");
            if (id_val && id_val->type == JSON_STRING) {
                /* Deep copy the user document */
                char* user_str = json_stringify(user_doc);
                json_value_t* user_copy = json_parse(user_str);
                free(user_str);
                
                if (user_copy) {
                    json_object_set(rbac->users, id_val->value.string, user_copy);
                }
            }
        }
    }
    
    json_free(users_result);
    
    /* Load all roles */
    json_value_t* all_roles_query = json_create_object();
    json_value_t* roles_result = db_query_documents(db, RBAC_ROLES_COLLECTION, all_roles_query);
    json_free(all_roles_query);
    
    if (!roles_result || roles_result->type != JSON_ARRAY) {
        if (roles_result) json_free(roles_result);
        rbac_free(rbac);
        return NULL;
    }
    
    /* Add each role to roles object */
    for (size_t i = 0; i < roles_result->value.array.size; i++) {
        json_value_t* role_doc = roles_result->value.array.items[i];
        if (role_doc->type == JSON_OBJECT) {
            json_value_t* id_val = json_object_get(role_doc, "id");
            if (id_val && id_val->type == JSON_STRING) {
                /* Deep copy the role document */
                char* role_str = json_stringify(role_doc);
                json_value_t* role_copy = json_parse(role_str);
                free(role_str);
                
                if (role_copy) {
                    json_object_set(rbac->roles, id_val->value.string, role_copy);
                }
            }
        }
    }
    
    json_free(roles_result);
    
    return rbac;
}

/* Save RBAC system to database */
int rbac_db_save(database_t* db, rbac_system_t* rbac) {
    if (!db || !rbac) {
        return 0;
    }
    
    /* Initialize RBAC collections if they don't exist */
    rbac_db_status_t status = rbac_db_init_collections(db);
    if (!status.success) {
        LOG_ERROR("Failed to initialize RBAC collections: %s", status.error_message);
        if (status.error_message) free(status.error_message);
        return 0;
    }
    
    /* Clear existing users and roles */
    json_value_t* clear_query = json_create_object();
    
    /* Get all users */
    json_value_t* existing_users = db_query_documents(db, RBAC_USERS_COLLECTION, clear_query);
    if (existing_users && existing_users->type == JSON_ARRAY) {
        /* Delete each user */
        for (size_t i = 0; i < existing_users->value.array.size; i++) {
            json_value_t* user = existing_users->value.array.items[i];
            if (user->type == JSON_OBJECT) {
                json_value_t* id_val = json_object_get(user, "id");
                if (id_val && id_val->type == JSON_STRING) {
                    db_delete_document(db, RBAC_USERS_COLLECTION, id_val->value.string);
                }
            }
        }
        json_free(existing_users);
    }
    
    /* Get all roles */
    json_value_t* existing_roles = db_query_documents(db, RBAC_ROLES_COLLECTION, clear_query);
    if (existing_roles && existing_roles->type == JSON_ARRAY) {
        /* Delete each role */
        for (size_t i = 0; i < existing_roles->value.array.size; i++) {
            json_value_t* role = existing_roles->value.array.items[i];
            if (role->type == JSON_OBJECT) {
                json_value_t* id_val = json_object_get(role, "id");
                if (id_val && id_val->type == JSON_STRING) {
                    db_delete_document(db, RBAC_ROLES_COLLECTION, id_val->value.string);
                }
            }
        }
        json_free(existing_roles);
    }
    
    json_free(clear_query);
    
    /* Save users */
    for (size_t i = 0; i < rbac->users->value.object.size; i++) {
        const char* user_id = rbac->users->value.object.entries[i].key;
        json_value_t* user = rbac->users->value.object.entries[i].value;
        
        if (user->type == JSON_OBJECT) {
            /* Insert user into database */
            json_value_t* user_copy = json_clone(user);
            json_value_t* result = db_insert_document(db, RBAC_USERS_COLLECTION, user_copy);
            
            if (!result) {
                LOG_ERROR("Failed to insert user %s into database", user_id);
                return 0;
            }
            
            json_free(result);
        }
    }
    
    /* Save roles */
    for (size_t i = 0; i < rbac->roles->value.object.size; i++) {
        const char* role_id = rbac->roles->value.object.entries[i].key;
        json_value_t* role = rbac->roles->value.object.entries[i].value;
        
        if (role->type == JSON_OBJECT) {
            /* Insert role into database */
            json_value_t* role_copy = json_clone(role);
            json_value_t* result = db_insert_document(db, RBAC_ROLES_COLLECTION, role_copy);
            
            if (!result) {
                LOG_ERROR("Failed to insert role %s into database", role_id);
                return 0;
            }
            
            json_free(result);
        }
    }
    
    return 1;
}

/* Create a user in the database */
rbac_user_t* rbac_db_create_user(database_t* db, const char* username, const char* password) {
    if (!db || !username || !password) {
        return NULL;
    }
    
    /* Check if username already exists */
    json_value_t* query = json_create_object();
    json_object_set(query, "username", json_create_string(username));
    json_value_t* result = db_query_documents(db, RBAC_USERS_COLLECTION, query);
    json_free(query);
    
    if (result && result->type == JSON_ARRAY && result->value.array.size > 0) {
        LOG_ERROR("Username %s already exists", username);
        json_free(result);
        return NULL;
    }
    
    if (result) json_free(result);
    
    /* Create user ID */
    char* id = generate_uuid();
    if (!id) {
        return NULL;
    }
    
    /* Hash password */
    char* password_hash = hash_password(password);
    if (!password_hash) {
        free(id);
        return NULL;
    }
    
    /* Create user document */
    json_value_t* user_doc = json_create_object();
    if (!user_doc) {
        free(id);
        free(password_hash);
        return NULL;
    }
    
    /* Set user properties */
    json_object_set(user_doc, "id", json_create_string(id));
    json_object_set(user_doc, "username", json_create_string(username));
    json_object_set(user_doc, "password_hash", json_create_string(password_hash));
    json_object_set(user_doc, "roles", json_create_array());
    
    /* Insert user document */
    json_value_t* insert_result = db_insert_document(db, RBAC_USERS_COLLECTION, user_doc);
    if (!insert_result) {
        LOG_ERROR("Failed to insert user document into database");
        free(id);
        free(password_hash);
        return NULL;
    }
    
    json_free(insert_result);
    
    /* Create user structure */
    rbac_user_t* user = (rbac_user_t*)malloc(sizeof(rbac_user_t));
    if (!user) {
        free(id);
        free(password_hash);
        return NULL;
    }
    
    /* Set user fields */
    user->id = id;
    user->username = strdup(username);
    user->password_hash = password_hash;
    user->roles = json_create_array();
    
    return user;
}

/* Delete a user from the database */
int rbac_db_delete_user(database_t* db, const char* user_id) {
    if (!db || !user_id) {
        return 0;
    }
    
    /* Get user document */
    json_value_t* user_doc = db_get_document(db, RBAC_USERS_COLLECTION, user_id);
    if (!user_doc) {
        LOG_ERROR("User %s not found", user_id);
        return 0;
    }
    
    /* Get user roles */
    json_value_t* roles_val = json_object_get(user_doc, "roles");
    if (roles_val && roles_val->type == JSON_ARRAY) {
        /* Remove user from all roles */
        for (size_t i = 0; i < roles_val->value.array.size; i++) {
            json_value_t* role_id_val = roles_val->value.array.items[i];
            if (role_id_val->type == JSON_STRING) {
                const char* role_id = role_id_val->value.string;
                
                /* Get role document */
                json_value_t* role_doc = db_get_document(db, RBAC_ROLES_COLLECTION, role_id);
                if (role_doc && role_doc->type == JSON_OBJECT) {
                    /* Get role users */
                    json_value_t* users_val = json_object_get(role_doc, "users");
                    if (users_val && users_val->type == JSON_ARRAY) {
                        /* Remove user from role users */
                        json_value_t* updated_users = json_create_array();
                        for (size_t j = 0; j < users_val->value.array.size; j++) {
                            json_value_t* id_val = users_val->value.array.items[j];
                            if (id_val->type == JSON_STRING && strcmp(id_val->value.string, user_id) != 0) {
                                json_array_append(updated_users, json_create_string(id_val->value.string));
                            }
                        }
                        
                        /* Update role document */
                        json_object_set(role_doc, "users", updated_users);
                        json_value_t* update_result = db_update_document(db, RBAC_ROLES_COLLECTION, role_id, role_doc);
                        if (update_result) {
                            json_free(update_result);
                        }
                    }
                    
                    json_free(role_doc);
                }
            }
        }
    }
    
    json_free(user_doc);
    
    /* Delete user document */
    return db_delete_document(db, RBAC_USERS_COLLECTION, user_id);
}

/* Get a user from the database by ID */
rbac_user_t* rbac_db_get_user(database_t* db, const char* user_id) {
    if (!db || !user_id) {
        return NULL;
    }
    
    /* Get user document */
    json_value_t* user_doc = db_get_document(db, RBAC_USERS_COLLECTION, user_id);
    if (!user_doc) {
        return NULL;
    }
    
    /* Convert to rbac_user_t */
    rbac_user_t* user = user_doc_to_rbac_user(user_doc);
    json_free(user_doc);
    
    return user;
}

/* Get a user from the database by username */
rbac_user_t* rbac_db_get_user_by_username(database_t* db, const char* username) {
    if (!db || !username) {
        return NULL;
    }
    
    /* Create query */
    json_value_t* query = json_create_object();
    json_object_set(query, "username", json_create_string(username));
    
    /* Query user document */
    json_value_t* result = db_query_documents(db, RBAC_USERS_COLLECTION, query);
    json_free(query);
    
    if (!result || result->type != JSON_ARRAY || result->value.array.size == 0) {
        if (result) json_free(result);
        return NULL;
    }
    
    /* Get first matching user */
    json_value_t* user_doc = result->value.array.items[0];
    
    /* Convert to rbac_user_t */
    rbac_user_t* user = user_doc_to_rbac_user(user_doc);
    json_free(result);
    
    return user;
}

/* Authenticate a user in the database */
int rbac_db_authenticate_user(database_t* db, const char* username, const char* password) {
    if (!db || !username || !password) {
        return 0;
    }
    
    /* Get user by username */
    rbac_user_t* user = rbac_db_get_user_by_username(db, username);
    if (!user) {
        return 0;
    }
    
    /* Verify password against hash */
    int result = rbac_authenticate_user(NULL, username, password);
    
    /* Free user */
    rbac_free_user(user);
    
    return result;
}

/* Create a role in the database */
rbac_role_t* rbac_db_create_role(database_t* db, const char* name) {
    if (!db || !name) {
        return NULL;
    }
    
    /* Check if role with same name already exists */
    json_value_t* query = json_create_object();
    json_object_set(query, "name", json_create_string(name));
    json_value_t* result = db_query_documents(db, RBAC_ROLES_COLLECTION, query);
    json_free(query);
    
    if (result && result->type == JSON_ARRAY && result->value.array.size > 0) {
        LOG_ERROR("Role with name %s already exists", name);
        json_free(result);
        return NULL;
    }
    
    if (result) json_free(result);
    
    /* Create role ID */
    char* id = generate_uuid();
    if (!id) {
        return NULL;
    }
    
    /* Create role document */
    json_value_t* role_doc = json_create_object();
    if (!role_doc) {
        free(id);
        return NULL;
    }
    
    /* Set role properties */
    json_object_set(role_doc, "id", json_create_string(id));
    json_object_set(role_doc, "name", json_create_string(name));
    json_object_set(role_doc, "permissions", json_create_object());
    json_object_set(role_doc, "users", json_create_array());
    
    /* Insert role document */
    json_value_t* insert_result = db_insert_document(db, RBAC_ROLES_COLLECTION, role_doc);
    if (!insert_result) {
        LOG_ERROR("Failed to insert role document into database");
        free(id);
        return NULL;
    }
    
    json_free(insert_result);
    
    /* Create role structure */
    rbac_role_t* role = (rbac_role_t*)malloc(sizeof(rbac_role_t));
    if (!role) {
        free(id);
        return NULL;
    }
    
    /* Set role fields */
    role->id = id;
    role->name = strdup(name);
    role->permissions = json_create_object();
    
    return role;
}

/* Delete a role from the database */
int rbac_db_delete_role(database_t* db, const char* role_id) {
    if (!db || !role_id) {
        return 0;
    }
    
    /* Get role document */
    json_value_t* role_doc = db_get_document(db, RBAC_ROLES_COLLECTION, role_id);
    if (!role_doc) {
        LOG_ERROR("Role %s not found", role_id);
        return 0;
    }
    
    /* Get role users */
    json_value_t* users_val = json_object_get(role_doc, "users");
    if (users_val && users_val->type == JSON_ARRAY) {
        /* Remove role from all users */
        for (size_t i = 0; i < users_val->value.array.size; i++) {
            json_value_t* user_id_val = users_val->value.array.items[i];
            if (user_id_val->type == JSON_STRING) {
                const char* user_id = user_id_val->value.string;
                
                /* Get user document */
                json_value_t* user_doc = db_get_document(db, RBAC_USERS_COLLECTION, user_id);
                if (user_doc && user_doc->type == JSON_OBJECT) {
                    /* Get user roles */
                    json_value_t* roles_val = json_object_get(user_doc, "roles");
                    if (roles_val && roles_val->type == JSON_ARRAY) {
                        /* Remove role from user roles */
                        json_value_t* updated_roles = json_create_array();
                        for (size_t j = 0; j < roles_val->value.array.size; j++) {
                            json_value_t* id_val = roles_val->value.array.items[j];
                            if (id_val->type == JSON_STRING && strcmp(id_val->value.string, role_id) != 0) {
                                json_array_append(updated_roles, json_create_string(id_val->value.string));
                            }
                        }
                        
                        /* Update user document */
                        json_object_set(user_doc, "roles", updated_roles);
                        json_value_t* update_result = db_update_document(db, RBAC_USERS_COLLECTION, user_id, user_doc);
                        if (update_result) {
                            json_free(update_result);
                        }
                    }
                    
                    json_free(user_doc);
                }
            }
        }
    }
    
    json_free(role_doc);
    
    /* Delete role document */
    return db_delete_document(db, RBAC_ROLES_COLLECTION, role_id);
}

/* Get a role from the database by ID */
rbac_role_t* rbac_db_get_role(database_t* db, const char* role_id) {
    if (!db || !role_id) {
        return NULL;
    }
    
    /* Get role document */
    json_value_t* role_doc = db_get_document(db, RBAC_ROLES_COLLECTION, role_id);
    if (!role_doc) {
        return NULL;
    }
    
    /* Convert to rbac_role_t */
    rbac_role_t* role = role_doc_to_rbac_role(role_doc);
    json_free(role_doc);
    
    return role;
}

/* Add a user to a role in the database */
int rbac_db_add_user_to_role(database_t* db, const char* user_id, const char* role_id) {
    if (!db || !user_id || !role_id) {
        return 0;
    }
    
    /* Get user document */
    json_value_t* user_doc = db_get_document(db, RBAC_USERS_COLLECTION, user_id);
    if (!user_doc) {
        LOG_ERROR("User %s not found", user_id);
        return 0;
    }
    
    /* Get role document */
    json_value_t* role_doc = db_get_document(db, RBAC_ROLES_COLLECTION, role_id);
    if (!role_doc) {
        LOG_ERROR("Role %s not found", role_id);
        json_free(user_doc);
        return 0;
    }
    
    /* Get user roles */
    json_value_t* user_roles = json_object_get(user_doc, "roles");
    if (!user_roles || user_roles->type != JSON_ARRAY) {
        json_free(user_doc);
        json_free(role_doc);
        return 0;
    }
    
    /* Get role users */
    json_value_t* role_users = json_object_get(role_doc, "users");
    if (!role_users || role_users->type != JSON_ARRAY) {
        json_free(user_doc);
        json_free(role_doc);
        return 0;
    }
    
    /* Check if user already in role */
    int user_has_role = 0;
    for (size_t i = 0; i < user_roles->value.array.size; i++) {
        json_value_t* id = user_roles->value.array.items[i];
        if (id->type == JSON_STRING && strcmp(id->value.string, role_id) == 0) {
            user_has_role = 1;
            break;
        }
    }
    
    /* Check if role already has user */
    int role_has_user = 0;
    for (size_t i = 0; i < role_users->value.array.size; i++) {
        json_value_t* id = role_users->value.array.items[i];
        if (id->type == JSON_STRING && strcmp(id->value.string, user_id) == 0) {
            role_has_user = 1;
            break;
        }
    }
    
    /* Add role to user roles if needed */
    if (!user_has_role) {
        json_array_append(user_roles, json_create_string(role_id));
        json_value_t* update_result = db_update_document(db, RBAC_USERS_COLLECTION, user_id, user_doc);
        if (update_result) {
            json_free(update_result);
        } else {
            LOG_ERROR("Failed to update user document");
            json_free(user_doc);
            json_free(role_doc);
            return 0;
        }
    }
    
    /* Add user to role users if needed */
    if (!role_has_user) {
        json_array_append(role_users, json_create_string(user_id));
        json_value_t* update_result = db_update_document(db, RBAC_ROLES_COLLECTION, role_id, role_doc);
        if (update_result) {
            json_free(update_result);
        } else {
            LOG_ERROR("Failed to update role document");
            json_free(user_doc);
            json_free(role_doc);
            return 0;
        }
    }
    
    json_free(user_doc);
    json_free(role_doc);
    
    return 1;
}

/* Remove a user from a role in the database */
int rbac_db_remove_user_from_role(database_t* db, const char* user_id, const char* role_id) {
    if (!db || !user_id || !role_id) {
        return 0;
    }
    
    /* Get user document */
    json_value_t* user_doc = db_get_document(db, RBAC_USERS_COLLECTION, user_id);
    if (!user_doc) {
        LOG_ERROR("User %s not found", user_id);
        return 0;
    }
    
    /* Get role document */
    json_value_t* role_doc = db_get_document(db, RBAC_ROLES_COLLECTION, role_id);
    if (!role_doc) {
        LOG_ERROR("Role %s not found", role_id);
        json_free(user_doc);
        return 0;
    }
    
    /* Get user roles */
    json_value_t* user_roles = json_object_get(user_doc, "roles");
    if (!user_roles || user_roles->type != JSON_ARRAY) {
        json_free(user_doc);
        json_free(role_doc);
        return 0;
    }
    
    /* Get role users */
    json_value_t* role_users = json_object_get(role_doc, "users");
    if (!role_users || role_users->type != JSON_ARRAY) {
        json_free(user_doc);
        json_free(role_doc);
        return 0;
    }
    
    /* Remove role from user roles */
    int user_role_found = 0;
    json_value_t* updated_roles = json_create_array();
    for (size_t i = 0; i < user_roles->value.array.size; i++) {
        json_value_t* id = user_roles->value.array.items[i];
        if (id->type == JSON_STRING && strcmp(id->value.string, role_id) != 0) {
            json_array_append(updated_roles, json_create_string(id->value.string));
        } else {
            user_role_found = 1;
        }
    }
    
    /* Update user document if role was found */
    if (user_role_found) {
        json_object_set(user_doc, "roles", updated_roles);
        json_value_t* update_result = db_update_document(db, RBAC_USERS_COLLECTION, user_id, user_doc);
        if (update_result) {
            json_free(update_result);
        } else {
            LOG_ERROR("Failed to update user document");
            json_free(user_doc);
            json_free(role_doc);
            return 0;
        }
    } else {
        json_free(updated_roles);
    }
    
    /* Remove user from role users */
    int role_user_found = 0;
    json_value_t* updated_users = json_create_array();
    for (size_t i = 0; i < role_users->value.array.size; i++) {
        json_value_t* id = role_users->value.array.items[i];
        if (id->type == JSON_STRING && strcmp(id->value.string, user_id) != 0) {
            json_array_append(updated_users, json_create_string(id->value.string));
        } else {
            role_user_found = 1;
        }
    }
    
    /* Update role document if user was found */
    if (role_user_found) {
        json_object_set(role_doc, "users", updated_users);
        json_value_t* update_result = db_update_document(db, RBAC_ROLES_COLLECTION, role_id, role_doc);
        if (update_result) {
            json_free(update_result);
        } else {
            LOG_ERROR("Failed to update role document");
            json_free(user_doc);
            json_free(role_doc);
            return 0;
        }
    } else {
        json_free(updated_users);
    }
    
    json_free(user_doc);
    json_free(role_doc);
    
    return user_role_found || role_user_found;
}

/* Get resource permission key */
static char* get_resource_permission_key(rbac_resource_type_t resource_type, const char* resource_id) {
    if (!resource_id) {
        return NULL;
    }
    
    /* Create key string */
    char* key = (char*)malloc(strlen(resource_id) + 32);
    if (!key) {
        return NULL;
    }
    
    /* Format key as "type:id" */
    sprintf(key, "%d:%s", resource_type, resource_id);
    
    return key;
}

/* Grant a permission to a role in the database */
int rbac_db_grant_permission(database_t* db, const char* role_id, rbac_resource_type_t resource_type,
                           const char* resource_id, rbac_permission_t permission) {
    if (!db || !role_id || !resource_id) {
        return 0;
    }
    
    /* Get role document */
    json_value_t* role_doc = db_get_document(db, RBAC_ROLES_COLLECTION, role_id);
    if (!role_doc) {
        LOG_ERROR("Role %s not found", role_id);
        return 0;
    }
    
    /* Get role permissions */
    json_value_t* permissions = json_object_get(role_doc, "permissions");
    if (!permissions || permissions->type != JSON_OBJECT) {
        json_free(role_doc);
        return 0;
    }
    
    /* Create resource permission key */
    char* key = get_resource_permission_key(resource_type, resource_id);
    if (!key) {
        json_free(role_doc);
        return 0;
    }
    
    /* Get current permission value */
    int current_permission = 0;
    json_value_t* current = json_object_get(permissions, key);
    if (current && current->type == JSON_NUMBER) {
        current_permission = (int)current->value.number;
    }
    
    /* Add permission */
    current_permission |= permission;
    
    /* Update permission */
    json_object_set(permissions, key, json_create_number(current_permission));
    
    /* Update role document */
    json_value_t* update_result = db_update_document(db, RBAC_ROLES_COLLECTION, role_id, role_doc);
    if (!update_result) {
        LOG_ERROR("Failed to update role document");
        free(key);
        json_free(role_doc);
        return 0;
    }
    
    json_free(update_result);
    free(key);
    json_free(role_doc);
    
    return 1;
}

/* Revoke a permission from a role in the database */
int rbac_db_revoke_permission(database_t* db, const char* role_id, rbac_resource_type_t resource_type,
                            const char* resource_id, rbac_permission_t permission) {
    if (!db || !role_id || !resource_id) {
        return 0;
    }
    
    /* Get role document */
    json_value_t* role_doc = db_get_document(db, RBAC_ROLES_COLLECTION, role_id);
    if (!role_doc) {
        LOG_ERROR("Role %s not found", role_id);
        return 0;
    }
    
    /* Get role permissions */
    json_value_t* permissions = json_object_get(role_doc, "permissions");
    if (!permissions || permissions->type != JSON_OBJECT) {
        json_free(role_doc);
        return 0;
    }
    
    /* Create resource permission key */
    char* key = get_resource_permission_key(resource_type, resource_id);
    if (!key) {
        json_free(role_doc);
        return 0;
    }
    
    /* Get current permission value */
    int current_permission = 0;
    json_value_t* current = json_object_get(permissions, key);
    if (current && current->type == JSON_NUMBER) {
        current_permission = (int)current->value.number;
    } else {
        /* No permission set, nothing to revoke */
        free(key);
        json_free(role_doc);
        return 1;
    }
    
    /* Remove permission */
    current_permission &= ~permission;
    
    /* Update permission */
    if (current_permission == 0) {
        json_object_remove(permissions, key);
    } else {
        json_object_set(permissions, key, json_create_number(current_permission));
    }
    
    /* Update role document */
    json_value_t* update_result = db_update_document(db, RBAC_ROLES_COLLECTION, role_id, role_doc);
    if (!update_result) {
        LOG_ERROR("Failed to update role document");
        free(key);
        json_free(role_doc);
        return 0;
    }
    
    json_free(update_result);
    free(key);
    json_free(role_doc);
    
    return 1;
}

/* Check if a user has a permission for a resource in the database */
int rbac_db_check_permission(database_t* db, const char* user_id, rbac_resource_type_t resource_type,
                           const char* resource_id, rbac_permission_t permission) {
    if (!db || !user_id || !resource_id) {
        return 0;
    }
    
    /* Get user document */
    json_value_t* user_doc = db_get_document(db, RBAC_USERS_COLLECTION, user_id);
    if (!user_doc) {
        LOG_ERROR("User %s not found", user_id);
        return 0;
    }
    
    /* Get user roles */
    json_value_t* roles = json_object_get(user_doc, "roles");
    if (!roles || roles->type != JSON_ARRAY) {
        json_free(user_doc);
        return 0;
    }
    
    /* Create resource permission key */
    char* key = get_resource_permission_key(resource_type, resource_id);
    if (!key) {
        json_free(user_doc);
        return 0;
    }
    
    /* Create wildcard resource permission key */
    char* wildcard_key = get_resource_permission_key(resource_type, "*");
    if (!wildcard_key) {
        free(key);
        json_free(user_doc);
        return 0;
    }
    
    /* Check permission in each role */
    int has_permission = 0;
    for (size_t i = 0; i < roles->value.array.size; i++) {
        json_value_t* role_id_val = roles->value.array.items[i];
        if (role_id_val->type != JSON_STRING) {
            continue;
        }
        
        const char* role_id = role_id_val->value.string;
        
        /* Get role document */
        json_value_t* role_doc = db_get_document(db, RBAC_ROLES_COLLECTION, role_id);
        if (!role_doc || role_doc->type != JSON_OBJECT) {
            continue;
        }
        
        /* Get role permissions */
        json_value_t* permissions = json_object_get(role_doc, "permissions");
        if (!permissions || permissions->type != JSON_OBJECT) {
            json_free(role_doc);
            continue;
        }
        
        /* Check specific resource permission */
        json_value_t* perm_val = json_object_get(permissions, key);
        if (perm_val && perm_val->type == JSON_NUMBER) {
            int perm = (int)perm_val->value.number;
            if ((perm & permission) == permission) {
                has_permission = 1;
                json_free(role_doc);
                break;
            }
        }
        
        /* Check wildcard resource permission */
        perm_val = json_object_get(permissions, wildcard_key);
        if (perm_val && perm_val->type == JSON_NUMBER) {
            int perm = (int)perm_val->value.number;
            if ((perm & permission) == permission) {
                has_permission = 1;
                json_free(role_doc);
                break;
            }
        }
        
        json_free(role_doc);
    }
    
    free(key);
    free(wildcard_key);
    json_free(user_doc);
    
    return has_permission;
}

/**
 * DEPRECATED: This function is no longer used as we've removed file-based RBAC fallback
 * It is kept here for API compatibility only, but will be removed in a future release
 */
int rbac_db_migrate_from_file(database_t* db, rbac_system_t* rbac) {
    (void)db;
    (void)rbac;
    LOG_WARNING("rbac_db_migrate_from_file is deprecated and will be removed in a future release");
    return 0; /* Always fail - migration is no longer supported */
}