#!/bin/bash

# Fix JavaScript conditional compilation issues in the codebase
# This script updates main.c to properly handle JavaScript conditional compilation

echo "Fixing JavaScript conditional compilation issues..."

# First, let's create a backup of main.c
echo "Creating backup of src/core/main.c to src/core/main.c.bak..."
cp src/core/main.c src/core/main.c.bak

# Update main.c to conditionally define the js_api_init and js_engine related functions
cat > src/core/main.c.new << 'EOF'
#include "jsondb/core/server.h"
#include "jsondb/database/database.h"
#include "jsondb/rbac/rbac.h"
#include "jsondb/api/api.h"
#include "jsondb/rbac/jwt.h"
#include "jsondb/utils/metrics.h"
#include "jsondb/rbac/rbac_refcount.h"
#include "jsondb/utils/logger.h"
#include "jsondb/utils/config_loader.h"
#include "jsondb/utils/js_file_utils.h"

/* JavaScript dependencies */
#ifndef DISABLE_JS
#include "jsondb/js/js_api.h"
#include "jsondb/js/js_engine.h"
extern js_engine_t* g_js_engine;
#else
/* Stub definitions when JavaScript is disabled */
typedef void js_engine_t;
void js_api_init(database_t* db) { 
    /* No-op implementation when JS is disabled */
    (void)db; /* Avoid unused parameter warning */
}
void js_api_cleanup() { 
    /* No-op implementation when JS is disabled */
}
int js_execute_file(js_engine_t* engine, const char* filename) {
    /* No-op implementation when JS is disabled */
    (void)engine; /* Avoid unused parameter warning */
    (void)filename; /* Avoid unused parameter warning */
    return 0;
}
void js_engine_free(js_engine_t* engine) {
    /* No-op implementation when JS is disabled */
    (void)engine; /* Avoid unused parameter warning */
}
js_engine_t* g_js_engine = NULL;
#endif

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

/* Default relative paths (will be resolved relative to the binary path) */
#define DEFAULT_DB_PATH "var/data/jsondb/db.json"
#define DEFAULT_RBAC_PATH "var/data/jsondb/rbac.json"
#define DEFAULT_JWT_SECRET "change-this-secret-in-production"
#define DEFAULT_PID_FILE "var/run/jsondb_server.pid"
#define DEFAULT_LOG_FILE "var/log/jsondb/server.log"

/* Global to store the binary directory path (not including trailing slash) */
static char binary_dir[PATH_MAX] = {0};

/* Default log level */
#define DEFAULT_LOG_LEVEL LOG_LEVEL_INFO

/* Global variables for cleanup handling */
/* g_server_config is now declared in config_loader.h/c */
static database_t* g_database = NULL;
static rbac_refcount_t* g_rbac_ref = NULL;  /* Reference counted RBAC */
static rbac_system_t* g_rbac = NULL;       /* Regular RBAC for backwards compatibility */

#ifndef TOOLS_BUILD
api_context_t* g_api_ctx = NULL;
metrics_registry_t* g_metrics_registry = NULL;
#else
extern api_context_t* g_api_ctx;
extern metrics_registry_t* g_metrics_registry;
#endif

/* Runtime file paths */
char pid_file_path[PATH_MAX] = DEFAULT_PID_FILE;
char log_file_path[PATH_MAX] = DEFAULT_LOG_FILE;
char db_file_path[PATH_MAX] = DEFAULT_DB_PATH;
char rbac_file_path[PATH_MAX] = DEFAULT_RBAC_PATH;
log_level_t log_level = DEFAULT_LOG_LEVEL;

/* Rest of main.c follows here */
EOF

# Now copy the remaining content from the backup file, excluding the header part
tail -n +51 src/core/main.c.bak | grep -v "extern js_engine_t\* g_js_engine;" >> src/core/main.c.new

# Replace the original file with our new version
mv src/core/main.c.new src/core/main.c

echo "Fixed JavaScript conditional compilation issues in src/core/main.c"
echo "Original file backed up to src/core/main.c.bak"