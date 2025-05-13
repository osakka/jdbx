#include "core/server.h"
#include "database/database.h"
#include "rbac/rbac.h"
#include "api/api.h"
#include "rbac/jwt.h"
#include "utils/metrics.h"

/* JavaScript dependencies */
#ifndef DISABLE_JS
#include "js/js_api.h"
#include "js/js_engine.h"
#else
/* Include stub typedefs for JavaScript when disabled */
typedef void js_engine_t;
/* Function declarations for JavaScript stubs are in api.h */
#endif

#include "rbac/rbac_refcount.h"
#include "utils/logger.h"
#include "utils/config_loader.h"
#include "utils/config_defaults.h"  /* For centralized defaults */
#include "utils/js_file_utils.h"
#include <stdlib.h> /* For atexit */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <libgen.h> /* For dirname() */
#include <dirent.h> /* For directory operations */
#include <limits.h> /* For PATH_MAX */
#include <getopt.h> /* For getopt_long */

// Global variables for cleanup handling
// g_server_config is now declared in config_loader.h/c
static database_t* g_database = NULL;
static rbac_refcount_t* g_rbac_ref = NULL;  /* Reference counted RBAC */
static rbac_system_t* g_rbac = NULL;       /* Regular RBAC for backwards compatibility */

/* Access to JS engine */
#ifndef DISABLE_JS
extern js_engine_t* g_js_engine;
#endif

#ifndef TOOLS_BUILD
api_context_t* g_api_ctx = NULL;
metrics_registry_t* g_metrics_registry = NULL;
#else
extern api_context_t* g_api_ctx;
extern metrics_registry_t* g_metrics_registry;
#endif

/* Runtime file paths - will be initialized from config during startup */
char pid_file_path[PATH_MAX];
char log_file_path[PATH_MAX];
char db_file_path[PATH_MAX];
char rbac_file_path[PATH_MAX];
log_level_t log_level = DEFAULT_LOG_LEVEL;

/* Function declarations for forward references */
pid_t read_pid_file();
int process_exists(pid_t pid);

/* Read PID from PID file */
pid_t read_pid_file() {
    if (pid_file_path[0] == '\0') {
        fprintf(stderr, "Error: PID file path not set\n");
        return -1;
    }
    
    FILE* pid_fp = fopen(pid_file_path, "r");
    if (!pid_fp) {
        /* PID file doesn't exist, meaning no running instance */
        return -1;
    }
    
    pid_t pid;
    if (fscanf(pid_fp, "%d", &pid) != 1) {
        fclose(pid_fp);
        fprintf(stderr, "Error: Failed to read PID from file '%s'\n", pid_file_path);
        return -1;
    }
    
    fclose(pid_fp);
    return pid;
}

/* Check if a process with the given PID exists */
int process_exists(pid_t pid) {
    if (pid <= 0) {
        return 0;  /* Invalid PID */
    }
    
    if (kill(pid, 0) == 0) {
        /* Process exists */
        return 1;
    } else if (errno == ESRCH) {
        /* Process does not exist */
        return 0;
    } else {
        /* Permission denied or other error */
        fprintf(stderr, "Error checking process existence: %s\n", strerror(errno));
        return 0;
    }
}

/* Print usage information and exit */
void print_usage(const char* program_name) {
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("JSON Database Server - A lightweight JSON document database\n\n");
    printf("Options:\n");
    printf("  -h, --help                    Display this help message and exit\n");
    printf("  -d, --daemon                  Run as a daemon (background mode)\n");
    printf("  -f, --foreground              Run in foreground mode\n");
    printf("  -t, --terminate               Terminate running server instance\n");
    printf("  -l, --log-level=LEVEL         Set log level (error, warn, info, debug, trace)\n");
    printf("  -b, --db-dir=DIRECTORY        Set database directory\n");
    printf("  -r, --rbac-file=FILE          Set RBAC file path\n");
    printf("  -p, --pid-file=FILE           Set PID file path\n");
    printf("  -o, --log-file=FILE           Set log file path\n");
    printf("  -c, --config=FILE             Load configuration from file\n");
    printf("  -v, --version                 Display version information and exit\n");
    printf("  -j, --js-file=FILE            Execute JavaScript file and exit\n");
    printf("\n");
}

/* Remove PID file - implementation of forward-declared function */
void remove_pid_file() {
    if (pid_file_path[0] != '\0') {
        unlink(pid_file_path);
    }
}

/* Convert string to log level */
log_level_t parse_log_level(const char* level_str) {
    if (!level_str) {
        return DEFAULT_LOG_LEVEL;
    }
    
    if (strcasecmp(level_str, "error") == 0) {
        return LOG_LEVEL_ERROR;
    } else if (strcasecmp(level_str, "warn") == 0 || strcasecmp(level_str, "warning") == 0) {
        return LOG_LEVEL_WARNING;
    } else if (strcasecmp(level_str, "info") == 0) {
        return LOG_LEVEL_INFO;
    } else if (strcasecmp(level_str, "debug") == 0) {
        return LOG_LEVEL_DEBUG;
    } else if (strcasecmp(level_str, "trace") == 0) {
        return LOG_LEVEL_TRACE;
    }
    
    fprintf(stderr, "Warning: Unknown log level '%s', using default (info)\n", level_str);
    return DEFAULT_LOG_LEVEL;
}

/* Cleanup resources safely */
void cleanup() {
    if (g_logger) {
        LOG_INFO("Performing cleanup before shutdown");
    }

    /* Save data first if needed */
    if (g_database) {
        if (g_logger) {
            LOG_INFO("Saving database state");
        }
        db_save(g_database);
    }

    if (g_rbac_ref) {
        if (g_logger) {
            LOG_INFO("Saving RBAC configuration");
        }
        rbac_refcount_save(g_rbac_ref, rbac_file_path);
    }

    /* Clean up in a safe order to avoid double-free issues */

    /* 1. First clean up JS engine which might use DB and RBAC */
#ifndef DISABLE_JS
    if (g_js_engine) {
        js_engine_t* engine = g_js_engine;
        g_js_engine = NULL;  /* Clear global reference */
        js_engine_free(engine);
    }
#endif

    /* 2. Free API context which might use DB and RBAC */
    if (g_api_ctx) {
        api_context_t* api = g_api_ctx;
        g_api_ctx = NULL;  /* Clear global reference */
        api_free_context(api);
    }

    /* 3. Free metrics */
    if (g_metrics_registry) {
        metrics_registry_t* metrics = g_metrics_registry;
        g_metrics_registry = NULL;  /* Clear global reference */
        metrics_registry_free(metrics);
    }

    /* 4. Free database */
    if (g_database) {
        database_t* db = g_database;
        g_database = NULL;  /* Clear global reference */
        db_close(db);
    }

    /* 5. Free server config */
    if (g_server_config) {
        server_config_t* server = g_server_config;
        g_server_config = NULL;  /* Clear global reference */

        /* Stop the server if it's still running */
        server_stop(server);

        /* Properly free CORS configuration */
        free_cors_config(&server->cors);

        free(server);
    }

    /* 6. First free the regular RBAC reference if it exists */
    if (g_rbac) {
        free(g_rbac); /* Just free the struct, not its contents (owned by g_rbac_ref) */
        g_rbac = NULL;
    }

    /* 7. Finally free RBAC system with reference counting */
    if (g_rbac_ref) {
        rbac_refcount_t* rbac = g_rbac_ref;
        g_rbac_ref = NULL;  /* Clear global reference */
        rbac_refcount_free(rbac);
    }

    /* 8. Remove PID file */
    remove_pid_file();

    /* 9. Close the logger */
    if (g_logger) {
        logger_close();
    }
}

int main(int argc, char** argv) {
    /* Initialize binary directory for path resolution */
    config_init_binary_dir();

    /* Initialize the server config with defaults from centralized configuration */
    server_config_t config = {0};
    g_server_config = &config;
    config_init_defaults(&config);
    
    /* Command line options */
    int show_help = 0;
    int run_daemon = 0;
    int run_foreground = 0;
    int terminate_server = 0;
    int show_version = 0;
    char* log_level_str = NULL;
    char* db_dir = NULL;
    char* pid_file = NULL;
    char* log_file = NULL;
    char* rbac_file = NULL;
    char* config_file = NULL;
    char* js_file = NULL;
    
    /* Define long options */
    static struct option long_options[] = {
        {"help",       no_argument,       0, 'h'},
        {"daemon",     no_argument,       0, 'd'},
        {"foreground", no_argument,       0, 'f'},
        {"terminate",  no_argument,       0, 't'},
        {"log-level",  required_argument, 0, 'l'},
        {"db-dir",     required_argument, 0, 'b'},
        {"rbac-file",  required_argument, 0, 'r'},
        {"pid-file",   required_argument, 0, 'p'},
        {"log-file",   required_argument, 0, 'o'},
        {"config",     required_argument, 0, 'c'},
        {"version",    no_argument,       0, 'v'},
        {"js-file",    required_argument, 0, 'j'},
        {0, 0, 0, 0}
    };
    
    /* Parse command line options */
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "hdftl:b:r:p:o:c:vj:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                show_help = 1;
                break;
            case 'd':
                run_daemon = 1;
                run_foreground = 0;
                break;
            case 'f':
                run_foreground = 1;
                run_daemon = 0;
                break;
            case 't':
                terminate_server = 1;
                break;
            case 'l':
                log_level_str = optarg;
                break;
            case 'b':
                db_dir = optarg;
                break;
            case 'r':
                rbac_file = optarg;
                break;
            case 'p':
                pid_file = optarg;
                break;
            case 'o':
                log_file = optarg;
                break;
            case 'c':
                config_file = optarg;
                break;
            case 'v':
                show_version = 1;
                break;
            case 'j':
                js_file = optarg;
                break;
            default:
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                return 1;
        }
    }
    
    /* Print banner */
    printf("JSON Database Server\n");
    printf("===================\n\n");
    
    /* Show help if requested or if no arguments provided */
    if (show_help || (argc == 1 && !run_daemon && !run_foreground && !terminate_server)) {
        print_usage(argv[0]);
        return 0;
    }
    
    /* Show version if requested */
    if (show_version) {
        printf("JSON Database Server v1.0.0\n");
        return 0;
    }
    
    /* Load configuration from file if specified */
    if (config_file) {
        if (!config_load(config_file, &config)) {
            fprintf(stderr, "Error: Failed to load configuration from '%s'\n", config_file);
            return 1;
        }
        printf("Configuration loaded from: %s\n", config_file);
    }
    
    /* Override configuration with command line arguments */
    if (run_daemon) {
        config.foreground_mode = 0;
    } else if (run_foreground) {
        config.foreground_mode = 1;
    }
    
    if (log_level_str) {
        config.log_level = parse_log_level(log_level_str);
    }
    
    if (db_dir) {
        /* Free previous value if allocated */
        if (config.db_path) {
            free(config.db_path);
        }
        config.db_path = strdup(db_dir);
    }
    
    if (rbac_file) {
        /* Free previous value if allocated */
        if (config.rbac_path) {
            free(config.rbac_path);
        }
        config.rbac_path = strdup(rbac_file);
    }
    
    if (pid_file) {
        /* Free previous value if allocated */
        if (config.pid_file) {
            free(config.pid_file);
        }
        config.pid_file = strdup(pid_file);
    }
    
    if (log_file) {
        /* Free previous value if allocated */
        if (config.log_file) {
            free(config.log_file);
        }
        config.log_file = strdup(log_file);
    }
    
    /* Copy paths to local variables for convenience */
    if (config.pid_file) {
        strncpy(pid_file_path, config.pid_file, PATH_MAX - 1);
        pid_file_path[PATH_MAX - 1] = '\0';
    }
    
    if (config.log_file) {
        strncpy(log_file_path, config.log_file, PATH_MAX - 1);
        log_file_path[PATH_MAX - 1] = '\0';
    }
    
    if (config.db_path) {
        strncpy(db_file_path, config.db_path, PATH_MAX - 1);
        db_file_path[PATH_MAX - 1] = '\0';
    }
    
    if (config.rbac_path) {
        strncpy(rbac_file_path, config.rbac_path, PATH_MAX - 1);
        rbac_file_path[PATH_MAX - 1] = '\0';
    }
    
    /* Set log level from config */
    log_level = config.log_level;
    
    /* Terminate server if requested */
    if (terminate_server) {
        pid_t pid = read_pid_file();
        if (pid > 0) {
            printf("Terminating server with PID %d...\n", pid);
            if (kill(pid, SIGTERM) == 0) {
                printf("Server terminated successfully\n");
                return 0;
            } else {
                fprintf(stderr, "Error: Failed to terminate server (errno=%d: %s)\n", 
                        errno, strerror(errno));
                return 1;
            }
        } else {
            fprintf(stderr, "Error: No running server found\n");
            return 1;
        }
    }

    /* Register cleanup handler */
    atexit(cleanup);

    /* Display active configuration */
    printf("Binary directory: %s\n", config_get_binary_dir());
    printf("Log level: %s\n", log_level_str ? log_level_str : "default");
    printf("Database path: %s\n", config.db_path ? config.db_path : "not set");
    printf("RBAC file: %s\n", config.rbac_path ? config.rbac_path : "not set");
    printf("PID file: %s\n", config.pid_file ? config.pid_file : "not set");
    printf("Log file: %s\n", config.log_file ? config.log_file : "not set");
    printf("Foreground mode: %s\n", config.foreground_mode ? "yes" : "no");
    
    /* If running in script mode, initialize JavaScript now if enabled */
    if (js_file) {
        /* Check if JavaScript is enabled in config */
        if (!config.js_enabled) {
            fprintf(stderr, "Error: JavaScript is disabled in configuration\n");
            return 1;
        }

#ifndef DISABLE_JS
        /* Initialize JavaScript engine for script execution */
        printf("Initializing JavaScript engine for script execution...\n");
        
        js_api_init(g_database);
        printf("JavaScript engine initialized\n");
        
        /* Execute JavaScript file using the JS engine */
        if (g_js_engine) {
            if (!js_execute_file(g_js_engine, js_file)) {
                fprintf(stderr, "Failed to execute JavaScript file\n");
                return 1;
            }
        } else {
            fprintf(stderr, "JavaScript engine not initialized, cannot execute file\n");
            return 1;
        }
        
        printf("JavaScript execution complete\n");
#else
        /* JavaScript support is disabled */
        printf("JavaScript support is disabled at compile time\n");
        return 1;
#endif
        /* Exit normally to ensure proper cleanup */
        return 0;
    }

    /* For server mode */
    printf("Starting server...\n");
    
    /* Check if server is already running */
    pid_t existing_pid = read_pid_file();
    if (existing_pid > 0 && process_exists(existing_pid)) {
        fprintf(stderr, "Error: Server is already running (PID: %d)\n", existing_pid);
        fprintf(stderr, "Use --terminate to stop the running server\n");
        return 1;
    }
    
    /* Create PID file directory if it doesn't exist */
    if (config.pid_file) {
        char pid_dir[PATH_MAX];
        strncpy(pid_dir, config.pid_file, PATH_MAX - 1);
        pid_dir[PATH_MAX - 1] = '\0';
        
        char* dir = dirname(pid_dir);
        struct stat st = {0};
        
        if (stat(dir, &st) == -1) {
            printf("Creating PID file directory: %s\n", dir);
            
            /* Use system to create nested directories if needed */
            char cmd[PATH_MAX + 50];
            snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dir);
            
            if (system(cmd) != 0) {
                fprintf(stderr, "Error: Failed to create PID file directory '%s': %s\n", 
                        dir, strerror(errno));
                return 1;
            }
        }
    }
    
    /* Create log file directory if it doesn't exist */
    if (config.log_file) {
        char log_dir[PATH_MAX];
        strncpy(log_dir, config.log_file, PATH_MAX - 1);
        log_dir[PATH_MAX - 1] = '\0';
        
        char* dir = dirname(log_dir);
        struct stat st = {0};
        
        if (stat(dir, &st) == -1) {
            printf("Creating log file directory: %s\n", dir);
            
            /* Use system to create nested directories if needed */
            char cmd[PATH_MAX + 50];
            snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dir);
            
            if (system(cmd) != 0) {
                fprintf(stderr, "Error: Failed to create log file directory '%s': %s\n", 
                        dir, strerror(errno));
                return 1;
            }
        }
    }
    
    /* Create database directory if it doesn't exist */
    if (config.db_path) {
        char db_dir[PATH_MAX];
        strncpy(db_dir, config.db_path, PATH_MAX - 1);
        db_dir[PATH_MAX - 1] = '\0';
        
        char* dir = dirname(db_dir);
        struct stat st = {0};
        
        if (stat(dir, &st) == -1) {
            printf("Creating database directory: %s\n", dir);
            
            /* Use system to create nested directories if needed */
            char cmd[PATH_MAX + 50];
            snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dir);
            
            if (system(cmd) != 0) {
                fprintf(stderr, "Error: Failed to create database directory '%s': %s\n", 
                        dir, strerror(errno));
                return 1;
            }
        }
    }

    /* Start in daemon mode if requested */
    if (!config.foreground_mode) {
        printf("Starting in daemon mode...\n");
        
        /* Fork the process */
        pid_t pid = fork();
        
        if (pid < 0) {
            fprintf(stderr, "Error: Failed to fork process\n");
            return 1;
        }
        
        if (pid > 0) {
            /* Parent process exits */
            printf("Server started in background (PID: %d)\n", pid);
            return 0;
        }
        
        /* Child process continues */
        
        /* Create a new session */
        if (setsid() < 0) {
            fprintf(stderr, "Error: Failed to create new session\n");
            return 1;
        }
        
        /* Close standard file descriptors */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        
        /* Redirect standard file descriptors to /dev/null */
        int fd = open("/dev/null", O_RDWR);
        if (fd < 0) {
            return 1;  /* Cannot log error as stdout/stderr are closed */
        }
        
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        
        if (fd > STDERR_FILENO) {
            close(fd);
        }
        
        /* Write PID to file */
        if (config.pid_file) {
            FILE* pid_fp = fopen(config.pid_file, "w");
            if (pid_fp) {
                fprintf(pid_fp, "%d\n", getpid());
                fclose(pid_fp);
            }
        }
    } else {
        /* In foreground mode, write PID to file */
        if (config.pid_file) {
            FILE* pid_fp = fopen(config.pid_file, "w");
            if (pid_fp) {
                fprintf(pid_fp, "%d\n", getpid());
                fclose(pid_fp);
            } else {
                fprintf(stderr, "Warning: Failed to write PID file '%s': %s\n", 
                        config.pid_file, strerror(errno));
            }
        }
    }
    
    /* Initialize logger */
    if (config.foreground_mode) {
        /* In foreground mode, direct logs to stdout/stderr */
        if (!logger_init(NULL, config.log_level)) {
            fprintf(stderr, "Error: Failed to initialize console logger\n");
            return 1;
        }
        if (g_logger) {
            g_logger->include_timestamp = 1;  /* Include timestamps in console output */
            g_logger->include_level = 1;      /* Include log level in console output */
        }
    } else if (config.log_file) {
        /* In daemon mode with specified log file */
        if (!logger_init(config.log_file, config.log_level)) {
            fprintf(stderr, "Error: Failed to initialize logger\n");
            return 1;
        }
    } else {
        /* In daemon mode without specified log file, use default */
        if (!logger_init(DEFAULT_LOG_FILE, config.log_level)) {
            fprintf(stderr, "Error: Failed to initialize default logger\n");
            return 1;
        }
    }
    
    /* Log server startup */
    if (g_logger) {
        LOG_INFO("Starting JSON Database Server");
        LOG_INFO("Binary directory: %s", config_get_binary_dir());
        LOG_INFO("PID: %d", getpid());
    }

    /* Initialize RBAC if needed */
    if (rbac_file_path[0] != '\0') {
        if (g_logger) {
            LOG_INFO("Initializing RBAC from '%s'", rbac_file_path);
        }
        
        /* Use rbac_load per rbac.h (line 52) */
        g_rbac = rbac_load(rbac_file_path);
        if (!g_rbac) {
            if (g_logger) {
                LOG_WARNING("Failed to load RBAC from '%s', creating new configuration", rbac_file_path);
            }
            g_rbac = rbac_init();  /* Use rbac_init per rbac.h (line 49) */
        }
    } else {
        /* Initialize with defaults if no path provided */
        g_rbac = rbac_init();
    }
    
    /* Initialize database */
    if (db_file_path[0] != '\0') {
        if (g_logger) {
            LOG_INFO("Initializing database from '%s'", db_file_path);
        }
        
        /* Use db_init per database.h (line 112) */
        g_database = db_init(db_file_path);
        if (!g_database) {
            if (g_logger) {
                LOG_WARNING("Failed to open database from '%s', creating new database", db_file_path);
            }
            /* Cannot create database from scratch directly, use db_init */
            fprintf(stderr, "Error: Failed to initialize database\n");
            return 1;
        }
    } else {
        if (g_logger) {
            LOG_WARNING("No database path specified, using in-memory database");
        }
        /* Use db_init with NULL path for in-memory */
        g_database = db_init(NULL);
        if (!g_database) {
            if (g_logger) {
                LOG_ERROR("Failed to create in-memory database");
            }
            fprintf(stderr, "Error: Failed to create in-memory database\n");
            return 1;
        }
    }
    
    /* Initialize API context */
    g_api_ctx = api_create_context(g_database, g_rbac, config.jwt_secret);
    if (!g_api_ctx) {
        if (g_logger) {
            LOG_ERROR("Failed to create API context");
        }
        fprintf(stderr, "Error: Failed to create API context\n");
        return 1;
    }

    /* Initialize metrics registry */
    /* Use metrics_registry_create per metrics.h (line 74) */
    g_metrics_registry = metrics_registry_create();
    if (!g_metrics_registry) {
        if (g_logger) {
            LOG_WARNING("Failed to initialize metrics registry");
        }
    }
    
    /* If in foreground mode, print more verbose output */
    if (config.foreground_mode) {
        /* Initialize JavaScript if enabled */
        if (config.js_enabled) {
#ifndef DISABLE_JS
            printf("Initializing JavaScript engine...\n");
            js_api_init(g_database);
            if (g_logger) {
                LOG_INFO("JavaScript engine initialized");
            }
            printf("JavaScript engine initialized\n");
#else
            if (g_logger) {
                LOG_WARNING("JavaScript support is disabled at compile time");
            }
            printf("JavaScript support is disabled at compile time\n");
#endif
        } else {
            if (g_logger) {
                LOG_INFO("JavaScript support is disabled in configuration");
            }
            printf("JavaScript support is disabled in configuration\n");
        }

        /* Initialize health API */
        printf("Initializing health monitoring API...\n");
        health_api_init();

        /* Register health API endpoints if API context is available */
        if (g_api_ctx) {
            register_health_api_endpoints(g_api_ctx);
            if (g_logger) {
                LOG_INFO("Health API endpoints registered");
            }
            printf("Health API endpoints registered\n");
        } else {
            if (g_logger) {
                LOG_WARNING("API context not available, health endpoints not registered");
            }
            printf("Warning: API context not available, health endpoints not registered\n");
        }
    }

    /* Initialize server */
    server_status_t status = server_init(&config);
    if (status != SERVER_OK) {
        if (g_logger) {
            LOG_ERROR("Failed to initialize server (status: %d)", status);
        }
        fprintf(stderr, "Error: Failed to initialize server\n");
        return 1;
    }

    /* Start server */
    status = server_start(&config);
    if (status != SERVER_OK) {
        if (g_logger) {
            LOG_ERROR("Failed to start server (status: %d)", status);
        }
        fprintf(stderr, "Error: Failed to start server\n");
        return 1;
    }

    if (config.foreground_mode) {
        /* Keep the server running in foreground mode */
        printf("Server running on %s:%d. Press Ctrl+C to stop.\n", 
               config.host ? config.host : "0.0.0.0", config.port);
        
        /* Wait for signal */
        pause();
    }
    
    return 0;
}
