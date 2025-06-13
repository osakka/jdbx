#ifndef RBAC_V2_H
#define RBAC_V2_H

#include "database/database.h"
#include "rbac/rbac.h"
#include "utils/json.h"

/* Initialize RBAC v2 system */
int rbac_v2_init(database_t* db);

/* Permission checking */
int rbac_v2_check_permission(database_t* db, const char* user_id, 
                            const char* resource_type, const char* resource_id,
                            rbac_permission_t permission);

/* Role management */
int rbac_v2_grant_role(database_t* db, const char* user_id, const char* role_id,
                      const char* granted_by, json_value_t* conditions);
int rbac_v2_revoke_role(database_t* db, const char* user_id, const char* role_id,
                       const char* revoked_by);

/* Query functions */
json_value_t* rbac_v2_get_user_roles(database_t* db, const char* user_id);
json_value_t* rbac_v2_get_role_users(database_t* db, const char* role_id);

/* Create RBAC system wrapper for compatibility */
rbac_system_t* rbac_v2_create_wrapper(database_t* db, const char* jwt_secret);

#endif /* RBAC_V2_H */