#ifndef JSONDB_INIT_H
#define JSONDB_INIT_H

#include "utils/logger.h"
#include "utils/config_loader.h"
#include "database/database.h"
#include "rbac/rbac.h"
#include "rbac/rbac_enhanced.h"
#include "rbac/rbac_refcount.h"
#include "api/api.h"
#include "core/server.h"

/* Initialization status codes */
typedef enum init_status_e {
    INIT_OK = 0,              /* Initialization successful */
    INIT_ERROR = -1,          /* Generic initialization error */
    INIT_CONFIG_ERROR = -10,  /* Configuration error */
    INIT_LOGGER_ERROR = -20,  /* Logger initialization error */
    INIT_DATABASE_ERROR = -30, /* Database initialization error */
    INIT_DAEMON_ERROR = -40,  /* Daemon initialization error */
    INIT_SOCKET_ERROR = -50,  /* Socket initialization error */
    INIT_RBAC_ERROR = -60,    /* RBAC initialization error */
    INIT_API_ERROR = -70,     /* API initialization error */
    INIT_THREAD_ERROR = -80,  /* Thread initialization error */
    INIT_PERSISTENCE_ERROR = -90, /* Persistence thread initialization error */
    
    /* Special status codes for daemon process flow control */
    INIT_DAEMON_PARENT_EXIT = 1,  /* Parent process should exit gracefully */
} init_status_t;

/* Initialize logging system */
init_status_t init_logger(server_config_t* config);

/* Initialize and load configuration */
init_status_t init_config(int argc, char** argv, server_config_t** config);

/* Initialize database */
init_status_t init_database(server_config_t* config, database_t** database);

/* Initialize daemon process if in daemon mode */
init_status_t init_daemon(server_config_t* config);

/* Initialize server socket */
init_status_t init_socket(server_config_t* config);

/* Initialize RBAC system */
init_status_t init_rbac(server_config_t* config, database_t* database, 
                      rbac_system_t** rbac, rbac_refcount_t** rbac_ref);

/* Initialize API context and register routes */
init_status_t init_api(server_config_t* config, database_t* database, 
                     rbac_system_t* rbac, api_context_t** api_ctx);

/* Initialize thread pool */
init_status_t init_threads(server_config_t* config);

/* Initialize persistence thread (must be called after daemonization) */
init_status_t init_persistence_thread(database_t* database);

/* Run server main loop */
init_status_t run_server(server_config_t* config);

/* Cleanup resources */
void init_cleanup(void);

/* Registration functions for cleanup system */
void init_register_database(database_t* database);
void init_register_rbac(rbac_system_t* rbac, rbac_refcount_t* rbac_ref);
void init_register_api(api_context_t* api_ctx);
void init_register_cleanup(void);
void init_set_verbose(int verbose);
void init_register_config(server_config_t* config);

/* Standard log message for initialization success */
#define INIT_LOG_SUCCESS(component, message, ...) \
    if (g_logger) { \
        LOG_INFO("[INIT:%s] SUCCESS: " message, component, ##__VA_ARGS__); \
    } else { \
        printf("[INIT:%s] SUCCESS: " message "\n", component, ##__VA_ARGS__); \
    }

/* Standard log message for initialization failure */
#define INIT_LOG_FAILURE(component, message, ...) \
    if (g_logger) { \
        LOG_ERROR("[INIT:%s] FAILURE: " message, component, ##__VA_ARGS__); \
    } else { \
        fprintf(stderr, "[INIT:%s] FAILURE: " message "\n", component, ##__VA_ARGS__); \
    }

/* Standard log message for initialization progress */
#define INIT_LOG_PROGRESS(component, message, ...) \
    if (g_logger) { \
        LOG_INFO("[INIT:%s] " message, component, ##__VA_ARGS__); \
    } else { \
        printf("[INIT:%s] " message "\n", component, ##__VA_ARGS__); \
    }

/* Standard log message for initialization debug information */
#define INIT_LOG_DEBUG(component, message, ...) \
    if (g_logger) { \
        LOG_DEBUG("[INIT:%s] " message, component, ##__VA_ARGS__); \
    } else { \
        if (g_verbose_mode) { \
            printf("[INIT:%s] DEBUG: " message "\n", component, ##__VA_ARGS__); \
        } \
    }

/* Global flag for verbose mode (used when logger is not yet initialized) */
extern int g_verbose_mode;

/* Helper functions */
init_status_t create_required_directories(server_config_t* config);
log_level_t parse_log_level(const char* level_str);
pid_t read_pid_file(void);

#endif /* JSONDB_INIT_H */