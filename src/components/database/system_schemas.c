#include "database/system_schemas.h"
#include "database/database.h"
#include "utils/json.h"
#include "utils/logger.h"
#include <string.h>
#include <stdlib.h>

/* Check if a collection is a system collection */
int db_is_system_collection(const char* collection_name) {
    if (!collection_name) return 0;
    
    const char* system_collections[] = {
        SYSTEM_USERS_COLLECTION,
        SYSTEM_ROLES_COLLECTION,
        SYSTEM_PERMISSIONS_COLLECTION,
        SYSTEM_SESSIONS_COLLECTION,
        SYSTEM_METRICS_COLLECTION,
        "_collections",
        "_permission_cache",
        "_system"
    };
    
    for (size_t i = 0; i < sizeof(system_collections) / sizeof(system_collections[0]); i++) {
        if (strcmp(collection_name, system_collections[i]) == 0) {
            return 1;
        }
    }
    
    return 0;
}

/* Create users collection schema */
static schema_t* create_users_schema() {
    json_value_t* schema_json = json_create_object();
    if (!schema_json) return NULL;
    
    json_object_set(schema_json, "type", json_create_string("object"));
    
    /* Properties */
    json_value_t* properties = json_create_object();
    
    /* _id property */
    json_value_t* id_prop = json_create_object();
    json_object_set(id_prop, "type", json_create_string("string"));
    json_object_set(properties, "_id", id_prop);
    
    /* username property */
    json_value_t* username_prop = json_create_object();
    json_object_set(username_prop, "type", json_create_string("string"));
    json_object_set(username_prop, "minLength", json_create_number(3));
    json_object_set(username_prop, "maxLength", json_create_number(50));
    json_object_set(username_prop, "pattern", json_create_string("^[a-zA-Z0-9_-]+$"));
    json_object_set(properties, "username", username_prop);
    
    /* password property */
    json_value_t* password_prop = json_create_object();
    json_object_set(password_prop, "type", json_create_string("string"));
    json_object_set(password_prop, "minLength", json_create_number(8));
    json_object_set(properties, "password", password_prop);
    
    /* email property */
    json_value_t* email_prop = json_create_object();
    json_object_set(email_prop, "type", json_create_string("string"));
    json_object_set(email_prop, "pattern", json_create_string("^[^@]+@[^@]+\\\\.[^@]+$"));
    json_object_set(properties, "email", email_prop);
    
    /* roles property */
    json_value_t* roles_prop = json_create_object();
    json_object_set(roles_prop, "type", json_create_string("array"));
    json_value_t* roles_items = json_create_object();
    json_object_set(roles_items, "type", json_create_string("string"));
    json_object_set(roles_prop, "items", roles_items);
    json_object_set(properties, "roles", roles_prop);
    
    /* created_at property */
    json_value_t* created_prop = json_create_object();
    json_object_set(created_prop, "type", json_create_string("number"));
    json_object_set(properties, "created_at", created_prop);
    
    /* last_login property */
    json_value_t* login_prop = json_create_object();
    json_object_set(login_prop, "type", json_create_string("number"));
    json_object_set(properties, "last_login", login_prop);
    
    json_object_set(schema_json, "properties", properties);
    
    /* Required fields */
    json_value_t* required = json_create_array();
    json_array_append(required, json_create_string("username"));
    json_array_append(required, json_create_string("password"));
    json_object_set(schema_json, "required", required);
    
    /* No additional properties */
    json_object_set(schema_json, "additionalProperties", json_create_boolean(0));
    
    /* Create schema from JSON */
    schema_t* schema = db_schema_from_json(schema_json);
    json_free(schema_json);
    
    return schema;
}

/* Create roles collection schema */
static schema_t* create_roles_schema() {
    json_value_t* schema_json = json_create_object();
    if (!schema_json) return NULL;
    
    json_object_set(schema_json, "type", json_create_string("object"));
    
    /* Properties */
    json_value_t* properties = json_create_object();
    
    /* _id property */
    json_value_t* id_prop = json_create_object();
    json_object_set(id_prop, "type", json_create_string("string"));
    json_object_set(properties, "_id", id_prop);
    
    /* name property */
    json_value_t* name_prop = json_create_object();
    json_object_set(name_prop, "type", json_create_string("string"));
    json_object_set(name_prop, "minLength", json_create_number(3));
    json_object_set(name_prop, "maxLength", json_create_number(50));
    json_object_set(properties, "name", name_prop);
    
    /* description property */
    json_value_t* desc_prop = json_create_object();
    json_object_set(desc_prop, "type", json_create_string("string"));
    json_object_set(properties, "description", desc_prop);
    
    /* permissions property (object with pattern properties) */
    json_value_t* perms_prop = json_create_object();
    json_object_set(perms_prop, "type", json_create_string("object"));
    
    json_value_t* pattern_props = json_create_object();
    json_value_t* perm_array = json_create_object();
    json_object_set(perm_array, "type", json_create_string("array"));
    
    json_value_t* perm_items = json_create_object();
    json_object_set(perm_items, "type", json_create_string("string"));
    json_value_t* perm_enum = json_create_array();
    json_array_append(perm_enum, json_create_string("CREATE"));
    json_array_append(perm_enum, json_create_string("READ"));
    json_array_append(perm_enum, json_create_string("UPDATE"));
    json_array_append(perm_enum, json_create_string("DELETE"));
    json_array_append(perm_enum, json_create_string("ADMIN"));
    json_object_set(perm_items, "enum", perm_enum);
    
    json_object_set(perm_array, "items", perm_items);
    json_object_set(pattern_props, ".*", perm_array);
    json_object_set(perms_prop, "patternProperties", pattern_props);
    json_object_set(properties, "permissions", perms_prop);
    
    /* system_permissions property */
    json_value_t* sys_perms_prop = json_create_object();
    json_object_set(sys_perms_prop, "type", json_create_string("array"));
    json_value_t* sys_items = json_create_object();
    json_object_set(sys_items, "type", json_create_string("string"));
    json_object_set(sys_perms_prop, "items", sys_items);
    json_object_set(properties, "system_permissions", sys_perms_prop);
    
    json_object_set(schema_json, "properties", properties);
    
    /* Required fields */
    json_value_t* required = json_create_array();
    json_array_append(required, json_create_string("name"));
    json_array_append(required, json_create_string("permissions"));
    json_object_set(schema_json, "required", required);
    
    /* No additional properties */
    json_object_set(schema_json, "additionalProperties", json_create_boolean(0));
    
    /* Create schema from JSON */
    schema_t* schema = db_schema_from_json(schema_json);
    json_free(schema_json);
    
    return schema;
}

/* Initialize system schemas */
int db_init_system_schemas(database_t* db) {
    if (!db) return 0;
    
    LOG_INFO("Initializing system schemas for bootstrap");
    
    /* Users collection schema */
    schema_t* users_schema = create_users_schema();
    if (users_schema) {
        if (db_attach_schema(db, SYSTEM_USERS_COLLECTION, users_schema)) {
            LOG_INFO("Attached schema to %s collection", SYSTEM_USERS_COLLECTION);
        } else {
            LOG_ERROR("Failed to attach schema to %s collection", SYSTEM_USERS_COLLECTION);
            free(users_schema);
            return 0;
        }
    }
    
    /* Roles collection schema */
    schema_t* roles_schema = create_roles_schema();
    if (roles_schema) {
        if (db_attach_schema(db, SYSTEM_ROLES_COLLECTION, roles_schema)) {
            LOG_INFO("Attached schema to %s collection", SYSTEM_ROLES_COLLECTION);
        } else {
            LOG_ERROR("Failed to attach schema to %s collection", SYSTEM_ROLES_COLLECTION);
            free(roles_schema);
            return 0;
        }
    }
    
    /* Note: Other system collections have more flexible schemas, so we don't 
     * enforce strict validation on them during bootstrap */
    
    return 1;
}

/* Check if database needs bootstrap */
int db_needs_bootstrap(database_t* db) {
    if (!db) return 0;
    
    /* Check if we have any user data */
    db_collection_t* users_coll = db_get_collection(db, SYSTEM_USERS_COLLECTION);
    if (!users_coll || !users_coll->documents || json_array_size(users_coll->documents) == 0) {
        LOG_INFO("Database needs bootstrap - no users found");
        return 1;
    }
    
    return 0;
}

/* Complete bootstrap mode */
void db_complete_bootstrap(database_t* db) {
    if (!db) return;
    
    db->is_bootstrap_mode = 0;
    LOG_INFO("Bootstrap mode completed - normal operation resumed");
}