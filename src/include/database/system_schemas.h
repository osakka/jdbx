#ifndef SYSTEM_SCHEMAS_H
#define SYSTEM_SCHEMAS_H

#include "database/database.h"

/* System collection names */
#define SYSTEM_USERS_COLLECTION "_users"
#define SYSTEM_ROLES_COLLECTION "_roles"  
#define SYSTEM_PERMISSIONS_COLLECTION "_permissions"
#define SYSTEM_SESSIONS_COLLECTION "_sessions"
#define SYSTEM_METRICS_COLLECTION "_metrics"
#define SYSTEM_CONFIG_COLLECTION "_system_config"

/* Initialize system schemas for bootstrap */
int db_init_system_schemas(database_t* db);

/* Check if a collection is a system collection */
int db_is_system_collection(const char* collection_name);

/* Check if database needs bootstrap */
int db_needs_bootstrap(database_t* db);

/* Complete bootstrap mode */
void db_complete_bootstrap(database_t* db);

#endif /* SYSTEM_SCHEMAS_H */