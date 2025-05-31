#ifndef RBAC_PERMISSIONS_H
#define RBAC_PERMISSIONS_H

#include "rbac/rbac.h"
#include "database/database.h"

/**
 * Database-only RBAC initialization
 * 
 * This function will:
 * 1. Check if RBAC exists in the database
 * 2. If yes, load RBAC from the database
 * 3. If not, initialize a new RBAC system and save to database
 * 
 * @param db Database instance
 * @param path Path to RBAC file (ignored, no longer used)
 * @return Initialized RBAC system or NULL on failure
 */
rbac_system_t* rbac_permissions_init(database_t* db, const char* path);

/**
 * Database-only RBAC save function
 * 
 * @param db Database instance
 * @param rbac RBAC system to save
 * @param path Path to RBAC file (ignored, no longer used)
 * @return 1 on success, 0 on failure
 */
int rbac_permissions_save(database_t* db, rbac_system_t* rbac, const char* path);

#endif /* RBAC_PERMISSIONS_H */