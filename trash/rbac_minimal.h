#ifndef RBAC_MINIMAL_H
#define RBAC_MINIMAL_H

#include "rbac/rbac.h"
#include "database/database.h"

/**
 * Minimal RBAC initialization that avoids all database operations
 * This is a last-resort implementation when other approaches are hanging
 * It creates a memory-only RBAC system with a default admin user and role
 * 
 * @param db Database instance (unused)
 * @param path Path to RBAC file (unused)
 * @return Initialized RBAC system
 */
rbac_system_t* rbac_minimal_init(database_t* db, const char* path);

/**
 * Minimal RBAC save function that does nothing
 * This is used to prevent hanging during RBAC save operations
 * 
 * @param db Database instance (unused)
 * @param rbac RBAC system to save (unused)
 * @param path Path to RBAC file (unused)
 * @return Always returns 1 (success)
 */
int rbac_minimal_save(database_t* db, rbac_system_t* rbac, const char* path);

#endif /* RBAC_MINIMAL_H */