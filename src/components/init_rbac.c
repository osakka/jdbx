#include "init.h"
#include "rbac/rbac.h"
#include "rbac/rbac_enhanced.h"
#include "rbac/rbac_refcount.h"
#include "database/database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Initialize RBAC system */
init_status_t init_rbac(server_config_t* config, database_t* database, 
                      rbac_system_t** rbac_out, rbac_refcount_t** rbac_ref_out) {
    INIT_LOG_PROGRESS("RBAC", "Initializing RBAC using enhanced system");
    
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
    
    /* Initialize the enhanced RBAC system */
    INIT_LOG_PROGRESS("RBAC", "Using database-first RBAC initialization approach");
    
    /* Use rbac_enhanced_init to check database first */
    rbac_system_t* rbac = rbac_enhanced_init(database, config->rbac_path);
    if (!rbac) {
        INIT_LOG_FAILURE("RBAC", "Failed to initialize RBAC system");
        return INIT_RBAC_ERROR;
    }
    
    INIT_LOG_SUCCESS("RBAC", "RBAC system initialized successfully");
    
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
    
    return INIT_OK;
}