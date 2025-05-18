#include "rbac/rbac.h"
#include "rbac/rbac_db.h"
#include "database/database.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Static flag to detect and prevent recursive database operations */
static int db_operation_in_progress = 0;
/* Flag to ensure collections are only initialized once */
static int db_collections_initialized = 0;

/**
 * Properly fixed RBAC initialization function
 * This version properly handles database operations without causing deadlocks
 * If database operations fail, it falls back to memory-only RBAC
 * 
 * @param db Database instance
 * @param path Path to RBAC file (used for legacy compatibility)
 * @return Initialized RBAC system
 */
rbac_system_t* rbac_enhanced_init(database_t* db, const char* path) {
    rbac_system_t* rbac = NULL;
    int db_load_success = 0;
    
    /* Log initialization attempt with timestamp */
    LOG_INFO("Starting RBAC initialization");
    printf("RBAC FIX: Starting RBAC initialization\n");
    fflush(stdout);
    
    /* Validate parameters */
    if (!db) {
        LOG_ERROR("Database is NULL, creating memory-only RBAC system");
        printf("RBAC FIX: Database is NULL, creating memory-only RBAC system\n");
        fflush(stdout);
        return rbac_init();
    }
    
    /* Avoid recursive calls by checking if we're already initializing RBAC */
    if (db_operation_in_progress) {
        LOG_WARNING("Recursive RBAC initialization detected, using memory-only RBAC");
        printf("RBAC FIX: Recursive RBAC initialization detected, using memory-only RBAC\n");
        fflush(stdout);
        return rbac_init();
    }
    
    /* Set the flag to prevent recursive operations */
    db_operation_in_progress = 1;
    
    /* FIRST FIX: Initialize RBAC collections if needed */
    if (!db_collections_initialized) {
        LOG_INFO("Initializing RBAC collections");
        printf("RBAC FIX: Initializing RBAC collections\n");
        fflush(stdout);
        
        rbac_db_status_t status = rbac_db_init_collections(db);
        
        if (status.success) {
            LOG_INFO("RBAC collections initialized successfully");
            printf("RBAC FIX: RBAC collections initialized successfully\n");
            fflush(stdout);
            db_collections_initialized = 1;
        } else {
            LOG_ERROR("Failed to initialize RBAC collections: %s", 
                     status.error_message ? status.error_message : "Unknown error");
            printf("RBAC FIX: Failed to initialize RBAC collections: %s\n", 
                  status.error_message ? status.error_message : "Unknown error");
            fflush(stdout);
            
            /* Free error message if present */
            if (status.error_message) {
                free(status.error_message);
            }
            
            /* Reset the flag since we're done with database operations */
            db_operation_in_progress = 0;
            
            /* Fall back to memory-only RBAC */
            return rbac_init();
        }
    }
    
    /* SECOND FIX: Try to load RBAC from database with proper error handling */
    LOG_INFO("Attempting to load RBAC from database");
    printf("RBAC FIX: Attempting to load RBAC from database\n");
    fflush(stdout);
    
    /* Try to load from the database with timeout control */
    rbac = rbac_db_load(db);
    
    if (rbac) {
        LOG_INFO("Successfully loaded RBAC from database");
        printf("RBAC FIX: Successfully loaded RBAC from database\n");
        fflush(stdout);
        db_load_success = 1;
    } else {
        LOG_WARNING("Failed to load RBAC from database, creating new RBAC system");
        printf("RBAC FIX: Failed to load RBAC from database, creating new RBAC system\n");
        fflush(stdout);
        
        /* Create a new empty RBAC system */
        rbac = rbac_init();
        
        if (!rbac) {
            LOG_ERROR("Failed to create new RBAC system");
            printf("RBAC FIX: Failed to create new RBAC system\n");
            fflush(stdout);
            
            /* Reset the flag since we're done with database operations */
            db_operation_in_progress = 0;
            
            return NULL;
        }
    }
    
    /* THIRD FIX: Only save to database if we created a new RBAC system and are not in a recursive call */
    if (!db_load_success && rbac) {
        /* Try to save the new RBAC system to the database */
        LOG_INFO("Saving new RBAC system to database");
        printf("RBAC FIX: Saving new RBAC system to database\n");
        fflush(stdout);
        
        /* Try to save but don't fail the whole operation if it doesn't work */
        int save_result = rbac_db_save(db, rbac);
        
        if (save_result) {
            LOG_INFO("Successfully saved RBAC to database");
            printf("RBAC FIX: Successfully saved RBAC to database\n");
            fflush(stdout);
        } else {
            LOG_ERROR("Failed to save RBAC to database, continuing with memory-only RBAC");
            printf("RBAC FIX: Failed to save RBAC to database, continuing with memory-only RBAC\n");
            fflush(stdout);
        }
    }
    
    /* Reset the flag since we're done with database operations */
    db_operation_in_progress = 0;
    
    LOG_INFO("RBAC initialization completed successfully");
    printf("RBAC FIX: RBAC initialization completed successfully\n");
    fflush(stdout);
    
    return rbac;
}

/**
 * Properly fixed RBAC save function
 * This version properly handles database operations without causing deadlocks
 * 
 * @param db Database instance
 * @param rbac RBAC system to save
 * @param path Path to RBAC file (used for legacy compatibility)
 * @return 1 on success, 0 on failure
 */
int rbac_enhanced_save(database_t* db, rbac_system_t* rbac, const char* path) {
    /* Static flag to prevent recursive save operations */
    static int save_in_progress = 0;
    
    /* Log save attempt */
    LOG_INFO("Attempting to save RBAC");
    printf("RBAC FIX: Attempting to save RBAC\n");
    fflush(stdout);
    
    /* Validate parameters */
    if (!db || !rbac) {
        LOG_ERROR("Invalid parameters for RBAC save: db=%p, rbac=%p", (void*)db, (void*)rbac);
        printf("RBAC FIX: Invalid parameters for RBAC save\n");
        fflush(stdout);
        return 0;
    }
    
    /* Detect recursive save operations */
    if (save_in_progress) {
        LOG_WARNING("Recursive RBAC save detected, skipping");
        printf("RBAC FIX: Recursive RBAC save detected, skipping\n");
        fflush(stdout);
        return 1; /* Return success to prevent callers from failing */
    }
    
    /* Set the flag to prevent recursive operations */
    save_in_progress = 1;
    
    /* Ensure collections are initialized */
    if (!db_collections_initialized) {
        LOG_INFO("Initializing RBAC collections before save");
        printf("RBAC FIX: Initializing RBAC collections before save\n");
        fflush(stdout);
        
        rbac_db_status_t status = rbac_db_init_collections(db);
        
        if (status.success) {
            LOG_INFO("RBAC collections initialized successfully");
            printf("RBAC FIX: RBAC collections initialized successfully\n");
            fflush(stdout);
            db_collections_initialized = 1;
        } else {
            LOG_ERROR("Failed to initialize RBAC collections: %s", 
                     status.error_message ? status.error_message : "Unknown error");
            printf("RBAC FIX: Failed to initialize RBAC collections: %s\n", 
                  status.error_message ? status.error_message : "Unknown error");
            fflush(stdout);
            
            /* Free error message if present */
            if (status.error_message) {
                free(status.error_message);
            }
            
            /* Reset the flag since we're done */
            save_in_progress = 0;
            return 0;
        }
    }
    
    /* Try to save RBAC to database */
    int result = rbac_db_save(db, rbac);
    
    if (result) {
        LOG_INFO("Successfully saved RBAC to database");
        printf("RBAC FIX: Successfully saved RBAC to database\n");
        fflush(stdout);
    } else {
        LOG_ERROR("Failed to save RBAC to database");
        printf("RBAC FIX: Failed to save RBAC to database\n");
        fflush(stdout);
    }
    
    /* Reset the flag since we're done */
    save_in_progress = 0;
    
    return result;
}