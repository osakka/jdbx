#ifndef RBAC_API_H
#define RBAC_API_H

#include "api/api.h"
#include "rbac/rbac.h"
#include "database/database.h"

/**
 * Register RBAC API routes
 * 
 * @param api_routes API routes array
 * @param num_routes Number of routes
 * @param db Database instance
 * @param rbac RBAC system
 * @param rbac_file_path Path to RBAC file
 * @return Updated number of routes
 */
int rbac_api_register_routes(api_route_t* api_routes, int num_routes, database_t* db, 
                            rbac_system_t* rbac, const char* rbac_file_path);

#endif /* RBAC_API_H */