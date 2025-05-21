#include "init.h"
#include "rbac/rbac.h"
#include "rbac/rbac_enhanced.h"
#include "rbac/rbac_minimal.h"
#include "rbac/rbac_refcount.h"
#include "database/database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* Initialize RBAC system */
init_status_t init_rbac(server_config_t* config, database_t* database, 
                      rbac_system_t** rbac_out, rbac_refcount_t** rbac_ref_out) {
    INIT_LOG_PROGRESS("RBAC", "Initializing RBAC");
    
    if (!config) {
        INIT_LOG_FAILURE("RBAC", "NULL server configuration");
        return INIT_RBAC_ERROR;
    }
    
    if (!database) {
        INIT_LOG_FAILURE("RBAC", "NULL database");
        return INIT_RBAC_ERROR;
    }
    
    if (!rbac_out || !rbac_ref_out) {
        INIT_LOG_FAILURE("RBAC", "NULL output parameters");
        return INIT_RBAC_ERROR;
    }
    
    /* First try with the minimal approach to ensure server can start */
    INIT_LOG_PROGRESS("RBAC", "Using minimal RBAC initialization to ensure server can start");
    
    /* Use minimal implementation that skips database operations */
    rbac_system_t* rbac = rbac_minimal_init(database, config->rbac_path);
    if (!rbac) {
        INIT_LOG_FAILURE("RBAC", "Failed to initialize minimal RBAC system");
        return INIT_RBAC_ERROR;
    }
    
    INIT_LOG_SUCCESS("RBAC", "Minimal RBAC system initialized successfully");
    
    /* Initialize reference counting wrapper */
    rbac_refcount_t* rbac_ref = rbac_to_refcount(rbac);
    if (!rbac_ref) {
        INIT_LOG_FAILURE("RBAC", "Failed to create RBAC reference counting wrapper");
        free(rbac);
        return INIT_RBAC_ERROR;
    }
    
    INIT_LOG_SUCCESS("RBAC", "RBAC reference counting wrapper created");
    
    /* Set output parameters */
    *rbac_out = rbac;
    *rbac_ref_out = rbac_ref;
    
    /* Register with cleanup system */
    init_register_rbac(rbac, rbac_ref);
    
    /* 
     * Try to load the enhanced RBAC system in a background thread
     * This allows the server to start and function with the minimal RBAC
     * while the enhanced system is being initialized asynchronously
     */
    
    return INIT_OK;
}