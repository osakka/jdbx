#include "rbac/rbac.h"
#include "rbac/rbac_db.h"
#include "database/database.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Database-only RBAC initialization
 * 
 * This function will:
 * 1. Check if RBAC exists in the database
 * 2. If yes, load RBAC from the database
 * 3. If not, initialize a new RBAC system and save to database
 * 
 * @param db Database instance
 * @param path Path to RBAC file (deprecated, only used for migration during transition)
 * @return Initialized RBAC system or NULL on failure
 */
rbac_system_t* rbac_enhanced_init(database_t* db, const char* path) {
    rbac_system_t* rbac = NULL;
    
    if (!db) {
        LOG_ERROR("Database is NULL, cannot initialize RBAC");
        return NULL;
    }
    
    /* Initialize RBAC collections */
    rbac_db_status_t status = rbac_db_init_collections(db);
    if (!status.success) {
        LOG_ERROR("Failed to initialize RBAC collections: %s", status.error_message);
        if (status.error_message) free(status.error_message);
        return NULL;
    }
    
    /* Check if RBAC exists in database */
    if (rbac_db_exists(db)) {
        LOG_INFO("Loading RBAC from database");
        rbac = rbac_db_load(db);
        if (rbac) {
            return rbac;
        }
        
        LOG_ERROR("Failed to load RBAC from database");
    }
    
    /* One-time migration from file if path is provided */
    if (path) {
        LOG_INFO("Attempting one-time migration from file %s", path);
        rbac = rbac_load(path);
        if (rbac) {
            /* Migrate to database */
            LOG_INFO("Migrating RBAC from file to database");
            if (rbac_db_migrate_from_file(db, rbac)) {
                LOG_INFO("RBAC migration successful");
                return rbac;
            }
            
            LOG_ERROR("Failed to migrate RBAC to database");
            rbac_free(rbac);
            rbac = NULL;
        }
    }
    
    /* Initialize new RBAC system */
    LOG_INFO("Initializing new RBAC system");
    rbac = rbac_init();
    if (rbac) {
        /* Save to database */
        LOG_INFO("Saving new RBAC system to database");
        if (rbac_db_save(db, rbac)) {
            LOG_INFO("RBAC saved to database");
        } else {
            LOG_ERROR("Failed to save RBAC to database");
            rbac_free(rbac);
            return NULL;
        }
    }
    
    return rbac;
}

/**
 * Database-only RBAC save function
 * 
 * @param db Database instance
 * @param rbac RBAC system to save
 * @param path Path to RBAC file (deprecated, not used)
 * @return 1 on success, 0 on failure
 */
int rbac_enhanced_save(database_t* db, rbac_system_t* rbac, const char* path) {
    if (!db || !rbac) {
        return 0;
    }
    
    /* Save to database only */
    LOG_INFO("Saving RBAC to database");
    if (!rbac_db_save(db, rbac)) {
        LOG_ERROR("Failed to save RBAC to database");
        return 0;
    }
    
    return 1;
}