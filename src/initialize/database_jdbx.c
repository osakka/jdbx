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
    
    /* Get database file basename */
    const char* db_file = config->db_file;
    if (!db_file || strlen(db_file) == 0) {
        db_file = getenv("JSONDB_DB_FILE");
        if (!db_file) {
            db_file = "var/jsondb";
        }
    }
    
    /* Generate actual JDBX file path */
    char* jdbx_path = jdbx_generate_db_path(db_file);
    if (!jdbx_path) {
        INIT_LOG_FAILURE("DATABASE", "Failed to generate JDBX file path");
        return INIT_DATABASE_ERROR;
    }
    
    INIT_LOG_PROGRESS("DATABASE", "Initializing JDBX database at %s", jdbx_path);
    
    /* Initialize database (will create/open JDBX file) */
    database_t* db = db_init(jdbx_path);
    
    /* Clean up generated path */
    free(jdbx_path);
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