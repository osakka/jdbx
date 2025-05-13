#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pthread.h>
#include <errno.h>

#include "src/include/jsondb.h"
#include "components/api/api.h"
#include "components/database/database.h"
#include "components/utils/json.h"
#include "components/utils/json_helpers.h"
#include "components/utils/logger.h"
#include "components/utils/import_export.h"
#include "components/utils/config_loader.h"

/* Default backup directory */
#define DEFAULT_BACKUP_DIR "./backups"
#define MAX_BACKUP_PATH_LEN 512
#define MAX_TIMESTAMP_LEN 32
#define DEFAULT_BACKUP_RETENTION 10
#define AUTO_BACKUP_INTERVAL_HOURS 24

typedef struct {
    int is_running;
    int backup_interval_hours;
    int backup_retention_count;
    pthread_t thread;
    pthread_mutex_t mutex;
} backup_service_t;

static backup_service_t g_backup_service = {
    .is_running = 0,
    .backup_interval_hours = AUTO_BACKUP_INTERVAL_HOURS,
    .backup_retention_count = DEFAULT_BACKUP_RETENTION,
    .thread = 0,
    .mutex = PTHREAD_MUTEX_INITIALIZER
};

extern database_t *g_database;

/* Helper Functions */

/* Ensure the backup directory exists */
static int ensure_backup_dir() {
    struct stat st = {0};
    
    if (stat(DEFAULT_BACKUP_DIR, &st) == -1) {
        /* Directory doesn't exist, create it */
        if (mkdir(DEFAULT_BACKUP_DIR, 0755) != 0) {
            LOG_ERROR("Failed to create backup directory: %s", strerror(errno));
            return 0;
        }
        LOG_INFO("Created backup directory: %s", DEFAULT_BACKUP_DIR);
    }
    
    return 1;
}

/* Generate a timestamp string for backup filename */
static void generate_timestamp(char *timestamp, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(timestamp, size, "%Y%m%d_%H%M%S", tm_info);
}

/* Create backup path with timestamp */
static void create_backup_path(char *path, size_t size, const char *prefix) {
    char timestamp[MAX_TIMESTAMP_LEN];
    generate_timestamp(timestamp, sizeof(timestamp));
    
    if (prefix && strlen(prefix) > 0) {
        snprintf(path, size, "%s/%s_%s.json", DEFAULT_BACKUP_DIR, prefix, timestamp);
    } else {
        snprintf(path, size, "%s/backup_%s.json", DEFAULT_BACKUP_DIR, timestamp);
    }
}

/* Remove oldest backups to maintain retention count */
static void cleanup_old_backups() {
    DIR *dir;
    struct dirent *entry;
    int retention = g_backup_service.backup_retention_count;
    
    if (retention <= 0) {
        return;  // No cleanup needed if retention is unlimited
    }
    
    dir = opendir(DEFAULT_BACKUP_DIR);
    if (!dir) {
        LOG_ERROR("Failed to open backup directory for cleanup: %s", strerror(errno));
        return;
    }
    
    // Count and collect backup files
    typedef struct {
        char filename[MAX_BACKUP_PATH_LEN];
        time_t mtime;
    } backup_file_t;
    
    backup_file_t *files = NULL;
    int count = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && strstr(entry->d_name, ".json") != NULL) {
            count++;
        }
    }
    
    if (count <= retention) {
        closedir(dir);
        return;  // No cleanup needed
    }
    
    // Allocate memory for file list
    files = (backup_file_t *)malloc(count * sizeof(backup_file_t));
    if (!files) {
        LOG_ERROR("Failed to allocate memory for backup file list");
        closedir(dir);
        return;
    }
    
    // Reset count and directory position
    count = 0;
    rewinddir(dir);
    
    // Collect file info
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && strstr(entry->d_name, ".json") != NULL) {
            char full_path[MAX_BACKUP_PATH_LEN];
            struct stat st;
            
            snprintf(full_path, sizeof(full_path), "%s/%s", DEFAULT_BACKUP_DIR, entry->d_name);
            if (stat(full_path, &st) == 0) {
                strcpy(files[count].filename, entry->d_name);
                files[count].mtime = st.st_mtime;
                count++;
            }
        }
    }
    
    closedir(dir);
    
    // Sort files by modification time (oldest first)
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (files[i].mtime > files[j].mtime) {
                backup_file_t temp = files[i];
                files[i] = files[j];
                files[j] = temp;
            }
        }
    }
    
    // Remove oldest files to maintain retention count
    int to_remove = count - retention;
    for (int i = 0; i < to_remove; i++) {
        char full_path[MAX_BACKUP_PATH_LEN];
        snprintf(full_path, sizeof(full_path), "%s/%s", DEFAULT_BACKUP_DIR, files[i].filename);
        
        if (remove(full_path) != 0) {
            LOG_ERROR("Failed to remove old backup: %s - %s", full_path, strerror(errno));
        } else {
            LOG_INFO("Removed old backup: %s", full_path);
        }
    }
    
    free(files);
}

/* Function to perform actual backup */
static int perform_backup(const char *backup_path, database_t *db) {
    LOG_INFO("Starting database backup to %s", backup_path);
    
    if (!db) {
        LOG_ERROR("Invalid database handle for backup");
        return -1;
    }
    
    int result = export_database(db, backup_path);
    if (result != EXPORT_SUCCESS) {
        LOG_ERROR("Failed to backup database to %s: %s", 
                 backup_path, export_error_message(result));
        return -1;
    }
    
    LOG_INFO("Database backup completed successfully: %s", backup_path);
    return 0;
}

/* Function to perform actual restore */
static int perform_restore(const char *backup_path, database_t *db) {
    LOG_INFO("Starting database restore from %s", backup_path);
    
    if (!db) {
        LOG_ERROR("Invalid database handle for restore");
        return -1;
    }
    
    int result = import_database(db, backup_path, 1);  // 1 = overwrite
    if (result != IMPORT_SUCCESS) {
        LOG_ERROR("Failed to restore database from %s: %s", 
                 backup_path, import_error_message(result));
        return -1;
    }
    
    LOG_INFO("Database restore completed successfully from %s", backup_path);
    return 0;
}

/* Automatic backup thread function */
static void* auto_backup_thread(void *arg) {
    (void)arg;  // Unused parameter
    
    while (1) {
        // Sleep for the configured interval
        sleep(g_backup_service.backup_interval_hours * 3600);
        
        // Check if service is still running
        pthread_mutex_lock(&g_backup_service.mutex);
        if (!g_backup_service.is_running) {
            pthread_mutex_unlock(&g_backup_service.mutex);
            break;
        }
        pthread_mutex_unlock(&g_backup_service.mutex);
        
        // Perform backup
        ensure_backup_dir();
        
        char backup_path[MAX_BACKUP_PATH_LEN];
        create_backup_path(backup_path, sizeof(backup_path), "auto");
        
        /* Access database through API context instead of global */
        database_t* db = NULL;
        api_context_t* api_ctx = NULL;

        /* Find API context */
        if (api_ctx && api_ctx->db) {
            db = api_ctx->db;
            if (perform_backup(backup_path, db) == 0) {
                cleanup_old_backups();
            }
        }
    }
    
    return NULL;
}

/* Get backup file info */
static json_value_t* get_backup_info(const char* filename) {
    /* Check if it's a regular file */
    struct stat st;
    char full_path[MAX_BACKUP_PATH_LEN];
    snprintf(full_path, sizeof(full_path), "%s/%s", DEFAULT_BACKUP_DIR, filename);
    
    if (stat(full_path, &st) != 0 || !S_ISREG(st.st_mode)) {
        return NULL;
    }
    
    /* Create backup info object */
    json_value_t* backup = json_create_object();
    if (!backup) {
        return NULL;
    }
    
    /* Set backup info */
    json_object_set(backup, "filename", json_create_string(filename));
    json_object_set(backup, "size", json_create_number(st.st_size));
    json_object_set(backup, "created", json_create_number(st.st_mtime));
    
    /* Try to parse timestamp from filename */
    char* timestamp = strrchr(filename, '_');
    if (timestamp) {
        timestamp++; /* Skip underscore */
        char* dot = strrchr(timestamp, '.');
        if (dot) *dot = '\0';  // Remove extension
        json_object_set(backup, "timestamp", json_create_string(timestamp));
    }
    
    return backup;
}

/* List all backup files */
static json_value_t* list_backups() {
    /* Ensure backup directory exists */
    ensure_backup_dir();
    
    /* Open backup directory */
    DIR* dir = opendir(DEFAULT_BACKUP_DIR);
    if (!dir) {
        return NULL;
    }
    
    /* Create backups array */
    json_value_t* backups = json_create_array();
    if (!backups) {
        closedir(dir);
        return NULL;
    }
    
    /* Read directory entries */
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Skip . and .. */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        /* Get backup info */
        json_value_t* backup = get_backup_info(entry->d_name);
        if (backup) {
            json_array_append(backups, backup);
        }
    }
    
    closedir(dir);
    
    return backups;
}

/* API Endpoint Handlers */

/* Create a backup of the database */
http_response_t* api_handle_backup_create(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Parse request body for backup prefix (optional) */
    char prefix[64] = "manual";
    if (request->body && request->content_length > 0) {
        json_value_t *body = json_parse(request->body);
        if (body && json_get_type(body) == JSON_OBJECT) {
            json_value_t *prefix_val = json_object_get(body, "prefix");
            if (prefix_val && json_get_type(prefix_val) == JSON_STRING) {
                const char *user_prefix = json_get_string(prefix_val);
                if (user_prefix && strlen(user_prefix) > 0 && strlen(user_prefix) < 50) {
                    strcpy(prefix, user_prefix);
                }
            }
        }
    }
    
    /* Ensure backup directory exists */
    if (!ensure_backup_dir()) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to create backup directory\"}", 
                                  "application/json");
    }
    
    /* Create backup path */
    char backup_path[MAX_BACKUP_PATH_LEN];
    create_backup_path(backup_path, sizeof(backup_path), prefix);
    
    /* Perform backup */
    int result = perform_backup(backup_path, ctx->db);
    if (result != 0) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Backup operation failed\"}", 
                                  "application/json");
    }
    
    /* Clean up old backups */
    cleanup_old_backups();
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_object_set(response, "success", json_create_boolean(1));
    json_object_set(response, "message", json_create_string("Backup created successfully"));
    
    /* Extract filename from path */
    const char* filename = strrchr(backup_path, '/');
    if (filename) {
        filename++; /* Skip the slash */
    } else {
        filename = backup_path;
    }
    
    json_object_set(response, "filename", json_create_string(filename));
    json_object_set(response, "path", json_create_string(backup_path));
    
    /* Serialize response */
    char* response_str = json_stringify(response);
    
    /* Free resources */
    json_free(response);
    
    /* Create HTTP response */
    http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
    
    /* Free response string */
    free(response_str);
    
    return http_response;
}

/* List all database backups */
http_response_t* api_handle_backup_list(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* List backups */
    json_value_t* backups = list_backups();
    if (!backups) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to list backups\"}", 
                                  "application/json");
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_object_set(response, "backups", backups);
    json_object_set(response, "count", json_create_number(json_array_size(backups)));
    json_object_set(response, "backup_directory", json_create_string(DEFAULT_BACKUP_DIR));
    
    /* Add backup settings */
    pthread_mutex_lock(&g_backup_service.mutex);
    json_object_set(response, "auto_backup", json_create_boolean(g_backup_service.is_running));
    json_object_set(response, "interval_hours", json_create_number(g_backup_service.backup_interval_hours));
    json_object_set(response, "retention_count", json_create_number(g_backup_service.backup_retention_count));
    pthread_mutex_unlock(&g_backup_service.mutex);
    
    /* Serialize response */
    char* response_str = json_stringify(response);
    
    /* Free resources */
    json_free(response);
    
    /* Create HTTP response */
    http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
    
    /* Free response string */
    free(response_str);
    
    return http_response;
}

/* Restore database from a backup */
http_response_t* api_handle_backup_restore(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->body) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Parse request body */
    json_value_t* body = json_parse(request->body);
    if (!body || json_get_type(body) != JSON_OBJECT) {
        if (body) json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request body\"}", "application/json");
    }
    
    /* Extract filename */
    json_value_t* filename_val = json_object_get(body, "filename");
    if (!filename_val || json_get_type(filename_val) != JSON_STRING) {
        json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Backup filename is required\"}", "application/json");
    }
    
    const char* filename = json_get_string(filename_val);
    
    /* Validate filename (prevent directory traversal) */
    if (strchr(filename, '/') || strchr(filename, '\\')) {
        json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid backup filename\"}", "application/json");
    }
    
    /* Check if file exists */
    char backup_path[MAX_BACKUP_PATH_LEN];
    snprintf(backup_path, sizeof(backup_path), "%s/%s", DEFAULT_BACKUP_DIR, filename);
    
    if (access(backup_path, F_OK) != 0) {
        json_free(body);
        return create_http_response(HTTP_NOT_FOUND, 
                                  "{\"error\":\"Backup file not found\"}", "application/json");
    }
    
    /* Create a backup before restore (safeguard) */
    char pre_restore_backup[MAX_BACKUP_PATH_LEN];
    create_backup_path(pre_restore_backup, sizeof(pre_restore_backup), "pre_restore");
    
    if (perform_backup(pre_restore_backup, ctx->db) != 0) {
        LOG_WARNING("Failed to create pre-restore backup");
    }
    
    /* Restore database */
    int result = perform_restore(backup_path, ctx->db);
    
    json_free(body);
    
    if (result != 0) {
        /* Create error response */
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Restore operation failed\"}", "application/json");
    }
    
    /* Extract filename from pre-restore backup path */
    const char* pre_restore_filename = strrchr(pre_restore_backup, '/');
    if (pre_restore_filename) {
        pre_restore_filename++; /* Skip the slash */
    } else {
        pre_restore_filename = pre_restore_backup;
    }
    
    /* Create success response */
    json_value_t* response = json_create_object();
    json_object_set(response, "success", json_create_boolean(1));
    json_object_set(response, "message", json_create_string("Database restored successfully"));
    json_object_set(response, "filename", json_create_string(filename));
    json_object_set(response, "pre_restore_backup", json_create_string(pre_restore_filename));
    
    /* Serialize response */
    char* response_str = json_stringify(response);
    
    /* Free resources */
    json_free(response);
    
    /* Create HTTP response */
    http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
    
    /* Free response string */
    free(response_str);
    
    return http_response;
}

/* Delete a backup file */
http_response_t* api_handle_backup_delete(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract filename from path */
    const char* path = request->path;
    if (strncmp(path, "/api/backup/", 12) != 0) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid backup path\"}", "application/json");
    }
    
    const char* filename = path + 12;
    
    /* Validate filename (prevent directory traversal) */
    if (strchr(filename, '/') || strchr(filename, '\\') || strlen(filename) == 0) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid backup filename\"}", "application/json");
    }
    
    /* Check if file exists */
    char backup_path[MAX_BACKUP_PATH_LEN];
    snprintf(backup_path, sizeof(backup_path), "%s/%s", DEFAULT_BACKUP_DIR, filename);
    
    if (access(backup_path, F_OK) != 0) {
        return create_http_response(HTTP_NOT_FOUND, 
                                  "{\"error\":\"Backup file not found\"}", "application/json");
    }
    
    /* Delete file */
    if (unlink(backup_path) != 0) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to delete backup file\"}", "application/json");
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_object_set(response, "success", json_create_boolean(1));
    json_object_set(response, "message", json_create_string("Backup deleted successfully"));
    json_object_set(response, "filename", json_create_string(filename));
    
    /* Serialize response */
    char* response_str = json_stringify(response);
    
    /* Free resources */
    json_free(response);
    
    /* Create HTTP response */
    http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
    
    /* Free response string */
    free(response_str);
    
    return http_response;
}

/* Configure backup settings */
http_response_t* api_handle_backup_configure(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->body) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Parse request body */
    json_value_t* body = json_parse(request->body);
    if (!body || json_get_type(body) != JSON_OBJECT) {
        if (body) json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request body\"}", "application/json");
    }
    
    int updated = 0;
    
    /* Update backup retention if specified */
    json_value_t* retention_val = json_object_get(body, "retention_count");
    if (retention_val && json_get_type(retention_val) == JSON_NUMBER) {
        int retention = (int)json_get_number(retention_val);
        if (retention >= 0) {
            pthread_mutex_lock(&g_backup_service.mutex);
            g_backup_service.backup_retention_count = retention;
            pthread_mutex_unlock(&g_backup_service.mutex);
            updated = 1;
        }
    }
    
    /* Update backup interval if specified */
    json_value_t* interval_val = json_object_get(body, "interval_hours");
    if (interval_val && json_get_type(interval_val) == JSON_NUMBER) {
        int interval = (int)json_get_number(interval_val);
        if (interval > 0) {
            pthread_mutex_lock(&g_backup_service.mutex);
            g_backup_service.backup_interval_hours = interval;
            pthread_mutex_unlock(&g_backup_service.mutex);
            updated = 1;
        }
    }
    
    /* Toggle automatic backup if specified */
    json_value_t* auto_backup_val = json_object_get(body, "auto_backup");
    if (auto_backup_val && json_get_type(auto_backup_val) == JSON_BOOLEAN) {
        int enable = json_get_boolean(auto_backup_val);
        
        pthread_mutex_lock(&g_backup_service.mutex);
        if (enable && !g_backup_service.is_running) {
            /* Start auto backup thread */
            g_backup_service.is_running = 1;
            if (pthread_create(&g_backup_service.thread, NULL, auto_backup_thread, NULL) != 0) {
                g_backup_service.is_running = 0;
                pthread_mutex_unlock(&g_backup_service.mutex);
                json_free(body);
                return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                           "{\"error\":\"Failed to start auto backup thread\"}", 
                                           "application/json");
            }
            updated = 1;
        } else if (!enable && g_backup_service.is_running) {
            /* Stop auto backup thread */
            g_backup_service.is_running = 0;
            pthread_mutex_unlock(&g_backup_service.mutex);
            
            /* Wait for thread to terminate */
            pthread_join(g_backup_service.thread, NULL);
            updated = 1;
            
            pthread_mutex_lock(&g_backup_service.mutex);
        }
        pthread_mutex_unlock(&g_backup_service.mutex);
    }
    
    json_free(body);
    
    if (!updated) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"No valid configuration changes provided\"}", 
                                  "application/json");
    }
    
    /* Create JSON response with current configuration */
    json_value_t* response = json_create_object();
    
    pthread_mutex_lock(&g_backup_service.mutex);
    json_object_set(response, "success", json_create_boolean(1));
    json_object_set(response, "message", json_create_string("Backup configuration updated"));
    json_object_set(response, "auto_backup", json_create_boolean(g_backup_service.is_running));
    json_object_set(response, "interval_hours", json_create_number(g_backup_service.backup_interval_hours));
    json_object_set(response, "retention_count", json_create_number(g_backup_service.backup_retention_count));
    pthread_mutex_unlock(&g_backup_service.mutex);
    
    /* Serialize response */
    char* response_str = json_stringify(response);
    
    /* Free resources */
    json_free(response);
    
    /* Create HTTP response */
    http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
    
    /* Free response string */
    free(response_str);
    
    return http_response;
}

/* Initialization function */
void backup_api_init(void) {
    /* Ensure backup directory exists */
    ensure_backup_dir();
    
    /* Read configuration if available */
    server_config_t *config = g_server_config;
    if (config) {
        json_value_t *backup_config = NULL;
        if (backup_config && json_get_type(backup_config) == JSON_OBJECT) {
            json_value_t *auto_val = json_object_get(backup_config, "auto_backup");
            json_value_t *interval_val = json_object_get(backup_config, "interval_hours");
            json_value_t *retention_val = json_object_get(backup_config, "retention_count");
            
            if (interval_val && json_get_type(interval_val) == JSON_NUMBER) {
                g_backup_service.backup_interval_hours = (int)json_get_number(interval_val);
            }
            
            if (retention_val && json_get_type(retention_val) == JSON_NUMBER) {
                g_backup_service.backup_retention_count = (int)json_get_number(retention_val);
            }
            
            if (auto_val && json_get_type(auto_val) == JSON_BOOLEAN && json_get_boolean(auto_val)) {
                /* Start auto backup thread */
                g_backup_service.is_running = 1;
                if (pthread_create(&g_backup_service.thread, NULL, auto_backup_thread, NULL) != 0) {
                    LOG_ERROR("Failed to start auto backup thread");
                    g_backup_service.is_running = 0;
                } else {
                    LOG_INFO("Auto backup thread started");
                }
            }
        }
    }
    
    LOG_INFO("Backup API initialized");
}

/* API endpoint registration */
void register_backup_api_endpoints(api_context_t *ctx) {
    /* Initialize backup API */
    backup_api_init();

    /* Note: The endpoints are already registered in the routes array in core/api.c */
    /* This function is called to initialize the backup API */

    if (g_logger) {
        LOG_INFO("Backup API initialized");
    }
}