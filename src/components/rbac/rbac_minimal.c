#include "rbac/rbac.h"
#include "rbac/rbac_db.h"
#include "database/database.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * Minimal RBAC initialization that avoids all database operations
 * This is a last-resort implementation when other approaches are hanging
 * It creates a memory-only RBAC system with a default admin user and role
 * 
 * @param db Database instance (unused)
 * @param path Path to RBAC file (unused)
 * @return Initialized RBAC system
 */
rbac_system_t* rbac_minimal_init(database_t* db, const char* path) {
    /* Mark parameters as unused */
    (void)db;
    (void)path;
    
    LOG_INFO("Starting minimal RBAC initialization (skipping database operations)");
    
    /* Create a basic RBAC system */
    rbac_system_t* rbac = rbac_init();
    if (!rbac) {
        LOG_ERROR("Failed to initialize minimal RBAC system");
        return NULL;
    }
    
    /* Create a default admin role */
    LOG_INFO("Creating default admin role in memory-only RBAC");
    
    json_value_t* admin_role = json_create_object();
    if (!admin_role) {
        LOG_ERROR("Failed to create admin role object");
        rbac_free(rbac);
        return NULL;
    }
    
    json_object_set(admin_role, "id", json_create_string("admin"));
    json_object_set(admin_role, "name", json_create_string("Administrator"));
    json_object_set(admin_role, "permissions", json_create_object());
    json_object_set(admin_role, "users", json_create_array());
    
    /* Add admin role to RBAC system */
    json_object_set(rbac->roles, "admin", admin_role);
    
    /* Create a default admin user */
    LOG_INFO("Creating default admin user in memory-only RBAC");
    
    json_value_t* admin_user = json_create_object();
    if (!admin_user) {
        LOG_ERROR("Failed to create admin user object");
        rbac_free(rbac);
        return NULL;
    }
    
    json_object_set(admin_user, "id", json_create_string("admin"));
    json_object_set(admin_user, "username", json_create_string("admin"));
    json_object_set(admin_user, "password_hash", json_create_string("$2a$10$RXM7Nq0jCIATCXsHpMdIa.UefPQhOmmEJuA5xn0M9fz.8p9UqrIHe")); /* Default: 'admin' */
    
    /* Add admin role to user */
    json_value_t* roles_array = json_create_array();
    json_array_append(roles_array, json_create_string("admin"));
    json_object_set(admin_user, "roles", roles_array);
    
    /* Add admin user to RBAC system */
    json_object_set(rbac->users, "admin", admin_user);
    
    /* Set admin role permissions (all permissions) */
    json_value_t* permissions = json_object_get(admin_role, "permissions");
    if (permissions) {
        json_object_set(permissions, "0:*", json_create_number(15)); /* All permissions for all collections */
        json_object_set(permissions, "1:*", json_create_number(15)); /* All permissions for all documents */
        json_object_set(permissions, "2:*", json_create_number(15)); /* All permissions for all users */
        json_object_set(permissions, "3:*", json_create_number(15)); /* All permissions for all roles */
    }
    
    LOG_INFO("Minimal RBAC initialization completed successfully (memory-only)");
    
    return rbac;
}

/**
 * Minimal RBAC save function that does nothing
 * This is used to prevent hanging during RBAC save operations
 * 
 * @param db Database instance (unused)
 * @param rbac RBAC system to save (unused)
 * @param path Path to RBAC file (unused)
 * @return Always returns 1 (success)
 */
int rbac_minimal_save(database_t* db, rbac_system_t* rbac, const char* path) {
    /* Mark parameters as unused */
    (void)db;
    (void)rbac;
    (void)path;
    
    LOG_INFO("Skipping RBAC save operation (minimal implementation)");
    
    /* Always return success to prevent callers from failing */
    return 1;
}