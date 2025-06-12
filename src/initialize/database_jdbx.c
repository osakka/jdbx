#include "init.h"
#include "database/database.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Initialize JDBX-only database */
init_status_t init_database(server_config_t* config, database_t** database_out) {
    INIT_LOG_PROGRESS("DATABASE", "Initializing JDBX database");
    
    if (!config) {
        INIT_LOG_FAILURE("DATABASE", "NULL server configuration");
        return INIT_DATABASE_ERROR;
    }
    
    if (!database_out) {
        INIT_LOG_FAILURE("DATABASE", "NULL output parameter");
        return INIT_DATABASE_ERROR;
    }
    
    /* Get database directory */
    const char* db_dir = config->db_path;
    if (!db_dir || strlen(db_dir) == 0) {
        db_dir = "/opt/jsondb/build/var";
    }
    
    INIT_LOG_PROGRESS("DATABASE", "Initializing JDBX database at %s", db_dir);
    
    /* Initialize database (will create/open JDBX file) */
    database_t* db = db_init(db_dir);
    if (!db) {
        INIT_LOG_FAILURE("DATABASE", "Failed to initialize JDBX database");
        return INIT_DATABASE_ERROR;
    }
    
    /* Use the returned database handle */
    *database_out = db;
    
    /* Register with cleanup system */
    init_register_database(*database_out);
    
    INIT_LOG_SUCCESS("DATABASE", "JDBX database initialized successfully");
    
    return INIT_OK;
}