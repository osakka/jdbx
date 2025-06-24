#ifndef JDBX_INIT_H
#define JDBX_INIT_H

#include "utils/logger.h"
#include "utils/config_loader.h"
#include "database/database.h"
#include "rbac/rbac.h"
#include "rbac/rbac_permissions.h"
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
    
    /* Special status codes for command line handling */
    INIT_SHOW_HELP = 2,      /* Help was requested, show help and exit */
    INIT_SHOW_VERSION = 3,   /* Version was requested, show version and exit */
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

/* Initialize metrics system */
init_status_t init_metrics(server_config_t* config);

/* Run server main loop */
init_status_t run_server(server_config_t* config);

/* Cleanup resources */
void init_cleanup(void);

/* Cleanup metrics */
void cleanup_metrics(void);

/* Process type constants */
#define PROCESS_TYPE_MAIN          0  /* Initial process */
#define PROCESS_TYPE_DAEMON_PARENT 1  /* Parent that will exit after forking daemon */
#define PROCESS_TYPE_DAEMON_CHILD  2  /* Daemon child that will fork again */
#define PROCESS_TYPE_SERVER        3  /* Final server process */

/* Process type management */
void init_set_process_type(int type);
int init_get_process_type(void);

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
        LOG_INFO(message, ##__VA_ARGS__); \
    } else { \
        EARLY_LOG_INFO(component, message, ##__VA_ARGS__); \
    }

/* Standard log message for initialization failure */
#define INIT_LOG_FAILURE(component, message, ...) \
    if (g_logger) { \
        LOG_ERROR(message, ##__VA_ARGS__); \
    } else { \
        EARLY_LOG_ERROR(component, message, ##__VA_ARGS__); \
    }

/* Standard log message for initialization progress */
#define INIT_LOG_PROGRESS(component, message, ...) \
    if (g_logger) { \
        LOG_INFO(message, ##__VA_ARGS__); \
    } else { \
        EARLY_LOG_INFO(component, message, ##__VA_ARGS__); \
    }

/* Standard log message for initialization warnings */
#define INIT_LOG_WARNING(component, message, ...) \
    if (g_logger) { \
        LOG_WARNING(message, ##__VA_ARGS__); \
    } else { \
        EARLY_LOG_WARNING(component, message, ##__VA_ARGS__); \
    }

/* Standard log message for initialization debug information */
#define INIT_LOG_DEBUG(component, message, ...) \
    if (g_logger) { \
        LOG_DEBUG(message, ##__VA_ARGS__); \
    } else { \
        EARLY_LOG_DEBUG(component, message, ##__VA_ARGS__); \
    }

/* Global flag for verbose mode (used when logger is not yet initialized) */
extern int g_verbose_mode;

/* Helper functions */
init_status_t create_required_directories(server_config_t* config);
log_level_t parse_log_level(const char* level_str);
pid_t read_pid_file(void);

#endif /* JDBX_INIT_H */