#include "core/server.h"
#include "database/database.h"
#include "rbac/rbac.h"
#include "rbac/rbac_enhanced.h"
#include "rbac/rbac_db.h"
#include "api/api.h"
#include "api/rbac_api.h"
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

/* Ensure USE_QUICKJS is defined when JavaScript is enabled */
#if !defined(DISABLE_JS) && !defined(USE_QUICKJS)
#define USE_QUICKJS
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
        /* Use logger if available */
        if (g_logger) {
            LOG_WARNING("PID file path not set");
        } else {
            fprintf(stderr, "Warning: PID file path not set\n");
        }
        return -1;
    }
    
    /* Check if file exists first */
    if (access(pid_file_path, F_OK) != 0) {
        /* PID file doesn't exist, meaning no running instance */
        return -1;
    }
    
    /* Then try to open it */
    FILE* pid_fp = fopen(pid_file_path, "r");
    if (!pid_fp) {
        /* PID file exists but can't be opened - could be a permissions issue */
        if (g_logger) {
            LOG_WARNING("Failed to open PID file '%s': %s", 
                        pid_file_path, strerror(errno));
        } else {
            fprintf(stderr, "Warning: Failed to open PID file '%s': %s\n", 
                    pid_file_path, strerror(errno));
        }
        return -1;
    }
    
    pid_t pid;
    if (fscanf(pid_fp, "%d", &pid) != 1) {
        fclose(pid_fp);
        
        /* PID file exists but is invalid */
        if (g_logger) {
            LOG_WARNING("Failed to read valid PID from file '%s'", pid_file_path);
        } else {
            fprintf(stderr, "Warning: Failed to read valid PID from file '%s'\n", pid_file_path);
        }
        
        /* Invalid PID file should be removed */
        unlink(pid_file_path);
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
    
    /* Check if process with this PID exists */
    if (kill(pid, 0) == 0) {
        /* Process exists */
        return 1;
    } else if (errno == ESRCH) {
        /* Process does not exist - clean up stale PID file if it exists */
        if (pid_file_path[0] != '\0' && access(pid_file_path, F_OK) == 0) {
            if (g_logger) {
                LOG_INFO("Removing stale PID file for non-existent process %d", pid);
            }
            unlink(pid_file_path);
        }
        return 0;
    } else {
        /* Permission denied or other error */
        if (g_logger) {
            LOG_WARNING("Error checking process existence (PID %d): %s", 
                        pid, strerror(errno));
        } else {
            fprintf(stderr, "Warning: Error checking process existence (PID %d): %s\n", 
                    pid, strerror(errno));
        }
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
    printf("  -i, --pid-file=FILE           Set PID file path\n");
    printf("  -o, --log-file=FILE           Set log file path\n");
    printf("  -w, --web-root=DIRECTORY      Set web admin interface root directory\n");
    printf("  -p, --port=PORT               Set server port (default: 5000)\n");
    printf("  -H, --host=HOST               Set server bind address (default: 0.0.0.0)\n");
    printf("  -V, --validators-dir=DIR      Set validators directory\n");
    printf("  -T, --transforms-dir=DIR      Set transforms directory\n");
    printf("  -M, --metrics-dir=DIR         Set metrics directory\n");
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

/* Flag to prevent multiple cleanup calls */
static int cleanup_registered = 0;
static int cleanup_in_progress = 0;

/* Cleanup resources safely */
void cleanup() {
    /* Prevent recursive cleanup */
    if (cleanup_in_progress) {
        return;
    }
    
    cleanup_in_progress = 1;
    
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
        /* Use database-only RBAC save */
        rbac_enhanced_save(g_database, g_rbac, NULL);
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
    server_config_t local_config = {0};
    server_config_t* heap_config = malloc(sizeof(server_config_t));
    if (!heap_config) {
        fprintf(stderr, "Error: Failed to allocate memory for server configuration\n");
        return 1;
    }
    memset(heap_config, 0, sizeof(server_config_t));
    
    /* Initialize with default values */
    config_init_defaults(&local_config);
    
    /* Copy to heap allocated config */
    memcpy(heap_config, &local_config, sizeof(server_config_t));
    
    /* Set global pointer to heap allocated config */
    g_server_config = heap_config;
    
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
    char* web_root = NULL;
    char* config_file = NULL;
    char* js_file = NULL;
    char* port_str = NULL;
    char* host_str = NULL;
    char* validators_dir = NULL;
    char* transforms_dir = NULL;
    char* metrics_dir = NULL;
    
    /* Define long options */
    static struct option long_options[] = {
        {"help",           no_argument,       0, 'h'},
        {"daemon",         no_argument,       0, 'd'},
        {"foreground",     no_argument,       0, 'f'},
        {"terminate",      no_argument,       0, 't'},
        {"log-level",      required_argument, 0, 'l'},
        {"db-dir",         required_argument, 0, 'b'},
        {"rbac-file",      required_argument, 0, 'r'},
        {"pid-file",       required_argument, 0, 'i'}, /* Changed from 'p' to 'i' */
        {"log-file",       required_argument, 0, 'o'},
        {"web-root",       required_argument, 0, 'w'},
        {"config",         required_argument, 0, 'c'},
        {"version",        no_argument,       0, 'v'},
        {"js-file",        required_argument, 0, 'j'},
        {"port",           required_argument, 0, 'p'}, /* Changed from 'P' to 'p' */
        {"host",           required_argument, 0, 'H'},
        {"validators-dir", required_argument, 0, 'V'},
        {"transforms-dir", required_argument, 0, 'T'},
        {"metrics-dir",    required_argument, 0, 'M'},
        {0, 0, 0, 0}
    };
    
    /* Parse command line options */
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "hdftl:b:r:i:o:w:c:vj:p:H:V:T:M:", long_options, &option_index)) != -1) {
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
            case 'i': /* Changed from 'p' to 'i' for pid-file */
                pid_file = optarg;
                break;
            case 'o':
                log_file = optarg;
                break;
            case 'w':
                web_root = optarg;
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
            case 'p': /* Changed from 'P' to 'p' for port */
                port_str = optarg;
                break;
            case 'H':
                host_str = optarg;
                break;
            case 'V':
                validators_dir = optarg;
                break;
            case 'T':
                transforms_dir = optarg;
                break;
            case 'M':
                metrics_dir = optarg;
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
        if (!config_load(config_file, g_server_config)) {
            fprintf(stderr, "Error: Failed to load configuration from '%s'\n", config_file);
            return 1;
        }
        printf("Configuration loaded from: %s\n", config_file);
    }
    
    /* Override configuration with command line arguments */
    if (run_daemon) {
        g_server_config->foreground_mode = 0;
    } else if (run_foreground) {
        g_server_config->foreground_mode = 1;
    }
    
    if (log_level_str) {
        g_server_config->log_level = parse_log_level(log_level_str);
    }
    
    if (db_dir) {
        /* Free previous value if allocated */
        if (g_server_config->db_path) {
            free(g_server_config->db_path);
        }
        g_server_config->db_path = strdup(db_dir);
    }
    
    if (rbac_file) {
        /* Free previous value if allocated */
        if (g_server_config->rbac_path) {
            free(g_server_config->rbac_path);
        }
        g_server_config->rbac_path = strdup(rbac_file);
    }
    
    if (web_root) {
        /* Free previous value if allocated */
        if (g_server_config->web_root) {
            free(g_server_config->web_root);
        }
        g_server_config->web_root = strdup(web_root);
    }
    
    if (pid_file) {
        /* Free previous value if allocated */
        if (g_server_config->pid_file) {
            free(g_server_config->pid_file);
        }
        g_server_config->pid_file = strdup(pid_file);
    }
    
    if (log_file) {
        /* Free previous value if allocated */
        if (g_server_config->log_file) {
            free(g_server_config->log_file);
        }
        g_server_config->log_file = strdup(log_file);
    }
    
    /* Process port argument */
    if (port_str) {
        int port = atoi(port_str);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Error: Invalid port number '%s'\n", port_str);
            return 1;
        }
        g_server_config->port = port;
    }
    
    /* Process host argument */
    if (host_str) {
        /* Free previous value if allocated */
        if (g_server_config->host) {
            free(g_server_config->host);
        }
        g_server_config->host = strdup(host_str);
    }
    
    /* Process validators directory argument */
    if (validators_dir) {
        /* Free previous value if allocated */
        if (g_server_config->validators_dir) {
            free(g_server_config->validators_dir);
        }
        g_server_config->validators_dir = strdup(validators_dir);
    }
    
    /* Process transforms directory argument */
    if (transforms_dir) {
        /* Free previous value if allocated */
        if (g_server_config->transforms_dir) {
            free(g_server_config->transforms_dir);
        }
        g_server_config->transforms_dir = strdup(transforms_dir);
    }
    
    /* Process metrics directory argument */
    if (metrics_dir) {
        /* Free previous value if allocated */
        if (g_server_config->metrics_dir) {
            free(g_server_config->metrics_dir);
        }
        g_server_config->metrics_dir = strdup(metrics_dir);
    }
    
    /* Copy paths to local variables for convenience */
    if (g_server_config->pid_file) {
        strncpy(pid_file_path, g_server_config->pid_file, PATH_MAX - 1);
        pid_file_path[PATH_MAX - 1] = '\0';
    }
    
    if (g_server_config->log_file) {
        strncpy(log_file_path, g_server_config->log_file, PATH_MAX - 1);
        log_file_path[PATH_MAX - 1] = '\0';
    }
    
    if (g_server_config->db_path) {
        strncpy(db_file_path, g_server_config->db_path, PATH_MAX - 1);
        db_file_path[PATH_MAX - 1] = '\0';
    }
    
    if (g_server_config->rbac_path) {
        strncpy(rbac_file_path, g_server_config->rbac_path, PATH_MAX - 1);
        rbac_file_path[PATH_MAX - 1] = '\0';
    }
    
    /* Set log level from config */
    log_level = g_server_config->log_level;
    
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

    /* Register cleanup handler only once */
    if (!cleanup_registered) {
        atexit(cleanup);
        cleanup_registered = 1;
        
        if (g_logger) {
            LOG_DEBUG("Registered cleanup handler");
        }
    }

    /* Display active configuration */
    printf("Binary directory: %s\n", config_get_binary_dir());
    printf("Log level: %s\n", log_level_str ? log_level_str : "default");
    printf("Database path: %s\n", g_server_config->db_path ? g_server_config->db_path : "not set");
    printf("RBAC file: %s\n", g_server_config->rbac_path ? g_server_config->rbac_path : "not set");
    printf("Web root: %s\n", g_server_config->web_root ? g_server_config->web_root : "not set");
    printf("PID file: %s\n", g_server_config->pid_file ? g_server_config->pid_file : "not set");
    printf("Log file: %s\n", g_server_config->log_file ? g_server_config->log_file : "not set");
    printf("Server host: %s\n", g_server_config->host ? g_server_config->host : "0.0.0.0");
    printf("Server port: %d\n", g_server_config->port);
    printf("Validators directory: %s\n", g_server_config->validators_dir ? g_server_config->validators_dir : DEFAULT_VALIDATORS_DIR);
    printf("Transforms directory: %s\n", g_server_config->transforms_dir ? g_server_config->transforms_dir : DEFAULT_TRANSFORMS_DIR);
    printf("Metrics directory: %s\n", g_server_config->metrics_dir ? g_server_config->metrics_dir : DEFAULT_METRICS_DIR);
    printf("Foreground mode: %s\n", g_server_config->foreground_mode ? "yes" : "no");
    
    /* If running in script mode, initialize JavaScript now if enabled */
    if (js_file) {
        /* Check if JavaScript is enabled in config */
        if (!g_server_config->js_enabled) {
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
    /* Use LOG macro instead of printf after logger is initialized */
    printf("Starting server...\n");
    
    /* In foreground mode, always remove any existing PID file */
    if (g_server_config->foreground_mode) {
        if (g_server_config->pid_file && access(g_server_config->pid_file, F_OK) != -1) {
            printf("Removing existing PID file in foreground mode\n");
            if (unlink(g_server_config->pid_file) != 0) {
                printf("Warning: Failed to remove existing PID file: %s\n", strerror(errno));
            }
        }
    } 
    /* In daemon mode, always check for existing process */
    else {
        /* Always clear stored PID file path first */
        if (pid_file_path[0] != '\0') {
            printf("Clearing stored PID file path '%s'\n", pid_file_path);
            pid_file_path[0] = '\0';
        }
        
        /* Copy current PID file path to global */
        if (g_server_config->pid_file) {
            strncpy(pid_file_path, g_server_config->pid_file, PATH_MAX - 1);
            pid_file_path[PATH_MAX - 1] = '\0';
            printf("Set PID file path to '%s'\n", pid_file_path);
        }
        
        /* Check for existing server using pid_file_path */
        pid_t existing_pid = read_pid_file();
        printf("Checking for existing server (PID file: %s, PID: %d)\n", 
               pid_file_path, existing_pid);
        
        if (existing_pid > 0) {
            printf("Testing if PID %d exists...\n", existing_pid);
            if (process_exists(existing_pid)) {
                fprintf(stderr, "Error: Server is already running (PID: %d)\n", existing_pid);
                fprintf(stderr, "Use --terminate to stop the running server\n");
                return 1;
            } else {
                printf("PID %d no longer exists, removing stale PID file\n", existing_pid);
                if (pid_file_path[0] != '\0' && access(pid_file_path, F_OK) != -1) {
                    unlink(pid_file_path);
                }
            }
        } else {
            printf("No existing server PID found\n");
        }
    }
    
    /* Create PID file directory if it doesn't exist */
    if (g_server_config->pid_file) {
        char pid_dir[PATH_MAX];
        strncpy(pid_dir, g_server_config->pid_file, PATH_MAX - 1);
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
    if (g_server_config->log_file) {
        char log_dir[PATH_MAX];
        strncpy(log_dir, g_server_config->log_file, PATH_MAX - 1);
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
    if (g_server_config->db_path) {
        char db_dir[PATH_MAX];
        strncpy(db_dir, g_server_config->db_path, PATH_MAX - 1);
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
    
    /* Create web root directory if it doesn't exist */
    if (g_server_config->web_root) {
        struct stat st = {0};
        
        if (stat(g_server_config->web_root, &st) == -1) {
            printf("Creating web root directory: %s\n", g_server_config->web_root);
            
            /* Use system to create nested directories if needed */
            char cmd[PATH_MAX + 50];
            snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", g_server_config->web_root);
            
            if (system(cmd) != 0) {
                fprintf(stderr, "Error: Failed to create web root directory '%s': %s\n", 
                        g_server_config->web_root, strerror(errno));
                return 1;
            }
        }
    }

    /* Start in daemon mode if requested */
    if (!g_server_config->foreground_mode) {
        printf("Starting in daemon mode...\n");
        
        /* Fork the process */
        pid_t pid = fork();
        
        if (pid < 0) {
            fprintf(stderr, "Error: Failed to fork process\n");
            return 1;
        }
        
        if (pid > 0) {
            /* Parent process exits without running cleanup */
            /* We must NOT call cleanup in the parent, so we unregister it */
            cleanup_registered = 0;  /* Prevent cleanup from running in parent */
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
        if (g_server_config->pid_file) {
            /* Make sure the PID file path is stored in our global variable for cleanup */
            strncpy(pid_file_path, g_server_config->pid_file, PATH_MAX - 1);
            pid_file_path[PATH_MAX - 1] = '\0';
            
            FILE* pid_fp = fopen(g_server_config->pid_file, "w");
            if (pid_fp) {
                fprintf(pid_fp, "%d\n", getpid());
                fclose(pid_fp);
                /* Cannot log here as logger may not be initialized yet */
            }
            /* Cannot report error here as stderr is closed */
        }
    } else {
        /* PID file writing now happens after logger initialization */
    }
    
    /* Initialize logger */
    if (g_server_config->foreground_mode) {
        /* In foreground mode, direct logs to stdout/stderr */
        if (!logger_init(NULL, g_server_config->log_level)) {
            fprintf(stderr, "Error: Failed to initialize console logger\n");
            return 1;
        }
        if (g_logger) {
            g_logger->include_timestamp = 1;  /* Include timestamps in console output */
            g_logger->include_level = 1;      /* Include log level in console output */
            LOG_INFO("Console logger initialized");
        }
    } else if (g_server_config->log_file) {
        /* In daemon mode with specified log file */
        if (!logger_init(g_server_config->log_file, g_server_config->log_level)) {
            fprintf(stderr, "Error: Failed to initialize logger\n");
            return 1;
        }
        if (g_logger) {
            LOG_INFO("File logger initialized: %s", g_server_config->log_file);
        }
    } else {
        /* In daemon mode without specified log file, use default */
        if (!logger_init(DEFAULT_LOG_FILE, g_server_config->log_level)) {
            fprintf(stderr, "Error: Failed to initialize default logger\n");
            return 1;
        }
        if (g_logger) {
            LOG_INFO("Default file logger initialized: %s", DEFAULT_LOG_FILE);
        }
    }
    
    /* Now that logger is initialized, write PID file if needed (in foreground mode) */
    if (g_server_config->foreground_mode && g_server_config->pid_file) {
        /* Make sure pid_file_path is set */
        if (pid_file_path[0] == '\0') {
            strncpy(pid_file_path, g_server_config->pid_file, PATH_MAX - 1);
            pid_file_path[PATH_MAX - 1] = '\0';
            LOG_DEBUG("Set pid_file_path to '%s'", pid_file_path);
        }
        
        FILE* pid_fp = fopen(g_server_config->pid_file, "w");
        if (pid_fp) {
            fprintf(pid_fp, "%d\n", getpid());
            fclose(pid_fp);
            LOG_INFO("PID file written: %s (PID: %d)", g_server_config->pid_file, getpid());
        } else {
            LOG_ERROR("Failed to write PID file '%s': %s", 
                     g_server_config->pid_file, strerror(errno));
        }
    }
    
    /* Log server startup */
    if (g_logger) {
        LOG_INFO("Starting JSON Database Server");
        LOG_INFO("Binary directory: %s", config_get_binary_dir());
        LOG_INFO("PID: %d", getpid());
    }

    /* Initialize database first */
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
    
    /* Initialize RBAC using enhanced system */
    if (g_logger) {
        LOG_INFO("Initializing RBAC using enhanced system");
    }
        
    /* Use rbac_enhanced_init to check database first */
    g_rbac = rbac_enhanced_init(g_database, rbac_file_path[0] != '\0' ? rbac_file_path : NULL);
    if (!g_rbac) {
        if (g_logger) {
            LOG_ERROR("Failed to initialize RBAC system");
        }
        fprintf(stderr, "Error: Failed to initialize RBAC system\n");
        return 1;
    }
    
    if (g_logger) {
        LOG_INFO("RBAC system initialized successfully");
    }
    
    /* Initialize document indices for faster lookups */
    if (g_logger) {
        LOG_INFO("Building document indices for faster lookups");
    }
    db_rebuild_indices(g_database);
    if (g_logger) {
        LOG_INFO("Document indices built successfully");
    }
    
    /* Initialize API context */
    if (g_logger) {
        LOG_INFO("Initializing API context with JWT secret: '%s'", g_server_config->jwt_secret);
    }
    
    g_api_ctx = api_create_context(g_database, g_rbac, g_server_config->jwt_secret);
    if (!g_api_ctx) {
        if (g_logger) {
            LOG_ERROR("Failed to create API context");
        }
        fprintf(stderr, "Error: Failed to create API context\n");
        return 1;
    }
    
    /* Register RBAC API routes */
    if (g_logger) {
        LOG_INFO("Registering RBAC API routes");
    }
    
    /* Get number of routes currently registered */
    int num_routes = g_api_ctx->num_routes;
    
    /* Register RBAC API routes */
    num_routes = rbac_api_register_routes(g_api_ctx->routes, num_routes, g_database, 
                                          g_rbac, rbac_file_path[0] != '\0' ? rbac_file_path : NULL);
    
    /* Update number of routes in API context */
    g_api_ctx->num_routes = num_routes;
    
    if (g_logger) {
        LOG_INFO("API context initialized with RBAC routes successfully");
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
    if (g_server_config->foreground_mode) {
        /* Initialize JavaScript if enabled */
        if (g_server_config->js_enabled) {
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
    printf("Initializing server...\n");
    server_status_t status = server_init(g_server_config);
    if (status != SERVER_OK) {
        if (g_logger) {
            LOG_ERROR("Failed to initialize server (status: %d)", status);
        }
        fprintf(stderr, "Error: Failed to initialize server\n");
        return 1;
    }
    printf("Server initialized successfully\n");

    /* Start server */
    printf("Starting server on port %d...\n", g_server_config->port);
    status = server_start(g_server_config);
    if (status != SERVER_OK) {
        if (g_logger) {
            LOG_ERROR("Failed to start server (status: %d)", status);
        }
        fprintf(stderr, "Error: Failed to start server\n");
        return 1;
    }
    printf("Server started successfully\n");

    if (g_server_config->foreground_mode) {
        /* Keep the server running in foreground mode */
        printf("Server running on %s:%d. Press Ctrl+C to stop.\n", 
               g_server_config->host ? g_server_config->host : "0.0.0.0", g_server_config->port);
    } else {
        /* In daemon mode, log that server started successfully */
        if (g_logger) {
            LOG_INFO("Server running in daemon mode on %s:%d", 
                    g_server_config->host ? g_server_config->host : "0.0.0.0", g_server_config->port);
        }
    }
    
    /* Wait for signal - different handling in foreground vs daemon */
    if (g_server_config->foreground_mode) {
        /* In foreground mode, we can simply pause */
        if (g_logger) {
            LOG_INFO("Server is now running in foreground mode, waiting for signals");
        }
        printf("Server is running. Press Ctrl+C to terminate.\n");
        pause();
    } else {
        /* In daemon mode, we sleep for a very long time instead of using pause() */
        if (g_logger) {
            LOG_INFO("Server is running in daemon mode, starting main loop");
        }
        
        /* Signal handling will wake up from this sleep if needed */
        while (1) {
            sleep(3600); /* Sleep for an hour at a time */
        }
    }
    
    /* Should only reach here in foreground mode after receiving a signal */
    printf("Server terminating\n");
    if (g_logger) {
        LOG_INFO("Server terminating normally");
    }
    
    return 0;
}
