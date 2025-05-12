#ifndef RBAC_H
#define RBAC_H

#include "utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* RBAC resource types */
typedef enum {
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
    RBAC_ADMIN = 8        /* 1000 */
} rbac_permission_t;

/* RBAC role */
typedef struct {
    char* id;
    char* name;
    json_value_t* permissions; /* JSON object mapping resource to permission */
} rbac_role_t;

/* RBAC user */
typedef struct {
    char* id;
    char* username;
    char* password_hash;
    json_value_t* roles; /* JSON array of role IDs */
} rbac_user_t;

/* RBAC system */
typedef struct {
    json_value_t* users; /* JSON object of users */
    json_value_t* roles; /* JSON object of roles */
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