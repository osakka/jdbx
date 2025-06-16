#ifndef RBAC_H
#define RBAC_H

#include "utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

/* RBAC resource types */
typedef enum {
    RBAC_UNKNOWN = -1,
    RBAC_DATABASE,
    RBAC_COLLECTION,
    RBAC_DOCUMENT,
    RBAC_USER,
    RBAC_ROLE,
    RBAC_PERMISSION
} rbac_resource_type_t;

/* RBAC permission types */
typedef enum {
    RBAC_READ = 1,        /* 0001 */
    RBAC_WRITE = 2,       /* 0010 */
    RBAC_DELETE = 4,      /* 0100 */
    RBAC_ADMIN = 8,       /* 1000 */
    RBAC_EXECUTE = 16     /* 10000 - Permission to execute functions */
} rbac_permission_t;

/* Multi-role and multi-owner permission structure */
typedef struct rbac_permissions {
    char** owner_ids;            /* Multiple document owners */
    size_t owner_count;          /* Number of owners */
    uint8_t owner_perms;         /* Owner permissions */
    uint8_t world_perms;         /* World/public permissions */
    
    /* Role-based permissions */
    struct {
        char* role_id;           /* Role/group name */
        uint8_t permissions;     /* Permissions for this role */
    } *role_perms;
    size_t role_count;
    
    /* Field-level rules */
    struct {
        char* pattern;           /* Regex pattern for field names */
        uint8_t owner_perms;     /* Permissions for owner */
        uint8_t world_perms;     /* Permissions for world */
        struct {
            char* role_id;
            uint8_t permissions;
        } *role_perms;
        size_t role_count;
    } *field_rules;
    size_t field_rule_count;
} rbac_permissions_t;

/* RBAC role/group - supports nesting */
typedef struct {
    char* id;
    char* name;
    json_value_t* permissions;   /* JSON object mapping resource to permission */
    json_value_t* parent_roles;  /* JSON array of parent role IDs */
    json_value_t* child_roles;   /* JSON array of child role IDs */
} rbac_role_t;

/* RBAC user */
typedef struct {
    char* id;
    char* username;
    char* password_hash;
    json_value_t* roles; /* JSON array of role IDs */
} rbac_user_t;

/* Forward declaration */
struct database;

/* RBAC system - Single Source of Truth: Database Only */
typedef struct {
    struct database* db; /* Database for RBAC storage - SINGLE SOURCE OF TRUTH */
    char* jwt_secret;    /* JWT secret for token generation */
} rbac_system_t;

/* RBAC function prototypes */
rbac_system_t* rbac_init();
void rbac_free(rbac_system_t* rbac);
int rbac_save(rbac_system_t* rbac, const char* path);
rbac_system_t* rbac_load(const char* path);

/* User operations */
rbac_user_t* rbac_create_user(rbac_system_t* rbac, const char* username, const char* password);
int rbac_delete_user(rbac_system_t* rbac, const char* user_id);
rbac_user_t* rbac_get_user(rbac_system_t* rbac, const char* user_id);
rbac_user_t* rbac_get_user_by_username(rbac_system_t* rbac, const char* username);
int rbac_authenticate_user(rbac_system_t* rbac, const char* username, const char* password);
void rbac_free_user(rbac_user_t* user);
char* hash_password(const char* password);
int verify_password(const char* password, const char* password_hash);

/* Role operations */
rbac_role_t* rbac_create_role(rbac_system_t* rbac, const char* name);
int rbac_delete_role(rbac_system_t* rbac, const char* role_id);
rbac_role_t* rbac_get_role(rbac_system_t* rbac, const char* role_id);
void rbac_free_role(rbac_role_t* role);
int rbac_add_user_to_role(rbac_system_t* rbac, const char* user_id, const char* role_id);
int rbac_remove_user_from_role(rbac_system_t* rbac, const char* user_id, const char* role_id);

/* Permission operations */
int rbac_grant_permission(rbac_system_t* rbac, const char* role_id, rbac_resource_type_t resource_type, 
                          const char* resource_id, rbac_permission_t permission);
int rbac_revoke_permission(rbac_system_t* rbac, const char* role_id, rbac_resource_type_t resource_type, 
                           const char* resource_id, rbac_permission_t permission);
int rbac_check_permission(rbac_system_t* rbac, const char* user_id, rbac_resource_type_t resource_type, 
                          const char* resource_id, rbac_permission_t permission);

#endif /* RBAC_H */