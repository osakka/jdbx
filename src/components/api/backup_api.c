#include "api/api.h"
#include "database/database.h"
#include "utils/import_export.h"
#include "utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

/* Default backup directory */
#define DEFAULT_BACKUP_DIR "./backups"

/* Ensure the backup directory exists */
static int ensure_backup_dir() {
    struct stat st = {0};
    
    if (stat(DEFAULT_BACKUP_DIR, &st) == -1) {
        /* Directory doesn't exist, create it */
        if (mkdir(DEFAULT_BACKUP_DIR, 0755) != 0) {
            return 0;
        }
    }
    
    return 1;
}

/* Get backup file info */
static json_value_t* get_backup_info(const char* filename) {
    /* Check if it's a regular file */
    struct stat st;
    char full_path[512];
    sprintf(full_path, "%s/%s", DEFAULT_BACKUP_DIR, filename);
    
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
    json_object_set(backup, "size", json_create_integer(st.st_size));
    json_object_set(backup, "created", json_create_integer(st.st_mtime));
    
    /* Try to parse timestamp from filename */
    char* timestamp = strrchr(filename, '_');
    if (timestamp) {
        timestamp++; /* Skip underscore */
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

/* Create a backup of the database */
http_response_t* api_handle_backup_create(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Ensure backup directory exists */
    if (!ensure_backup_dir()) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to create backup directory\"}", 
                                  "application/json");
    }
    
    /* Create backup */
    char* backup_path = backup_database(ctx->db, DEFAULT_BACKUP_DIR);
    if (!backup_path) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to create backup\"}", 
                                  "application/json");
    }
    
    /* Extract filename from path */
    const char* filename = strrchr(backup_path, '/');
    if (filename) {
        filename++; /* Skip the slash */
    } else {
        filename = backup_path;
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_object_set(response, "success", json_create_boolean(1));
    json_object_set(response, "message", json_create_string("Backup created successfully"));
    json_object_set(response, "filename", json_create_string(filename));
    json_object_set(response, "path", json_create_string(backup_path));
    
    /* Serialize response */
    char* response_str = json_stringify(response);
    
    /* Free resources */
    json_free(response);
    free(backup_path);
    
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
    json_object_set(response, "count", json_create_integer(json_array_size(backups)));
    
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
    if (!body || body->type != JSON_OBJECT) {
        if (body) json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request body\"}", "application/json");
    }
    
    /* Extract filename */
    json_value_t* filename_val = json_object_get(body, "filename");
    if (!filename_val || filename_val->type != JSON_STRING) {
        json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Backup filename is required\"}", "application/json");
    }
    
    const char* filename = filename_val->value.string;
    
    /* Validate filename (prevent directory traversal) */
    if (strchr(filename, '/') || strchr(filename, '\\')) {
        json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid backup filename\"}", "application/json");
    }
    
    /* Check if file exists */
    char backup_path[512];
    sprintf(backup_path, "%s/%s", DEFAULT_BACKUP_DIR, filename);
    
    if (access(backup_path, F_OK) != 0) {
        json_free(body);
        return create_http_response(HTTP_NOT_FOUND, 
                                  "{\"error\":\"Backup file not found\"}", "application/json");
    }
    
    /* Extract overwrite flag */
    int overwrite = 0;
    json_value_t* overwrite_val = json_object_get(body, "overwrite");
    if (overwrite_val && overwrite_val->type == JSON_BOOLEAN) {
        overwrite = overwrite_val->value.boolean;
    }
    
    /* Restore database */
    int result = import_database(ctx->db, backup_path, overwrite);
    
    json_free(body);
    
    if (result != IMPORT_SUCCESS) {
        /* Create error response */
        json_value_t* error = json_create_object();
        json_object_set(error, "success", json_create_boolean(0));
        json_object_set(error, "error", json_create_string(import_error_message(result)));
        json_object_set(error, "code", json_create_integer(result));
        
        /* Serialize error */
        char* error_str = json_stringify(error);
        
        /* Free resources */
        json_free(error);
        
        /* Create HTTP response */
        http_response_t* http_response = create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                                           error_str, "application/json");
        
        /* Free error string */
        free(error_str);
        
        return http_response;
    }
    
    /* Create success response */
    json_value_t* response = json_create_object();
    json_object_set(response, "success", json_create_boolean(1));
    json_object_set(response, "message", json_create_string("Database restored successfully"));
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
    char backup_path[512];
    sprintf(backup_path, "%s/%s", DEFAULT_BACKUP_DIR, filename);
    
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

/* Export database to a file */
http_response_t* api_handle_export(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->body) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Parse request body */
    json_value_t* body = json_parse(request->body);
    if (!body || body->type != JSON_OBJECT) {
        if (body) json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request body\"}", "application/json");
    }
    
    /* Extract output path */
    json_value_t* path_val = json_object_get(body, "path");
    if (!path_val || path_val->type != JSON_STRING) {
        json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Export path is required\"}", "application/json");
    }
    
    const char* output_path = path_val->value.string;
    
    /* Check for collections filter */
    json_value_t* collections_val = json_object_get(body, "collections");
    
    int result;
    if (collections_val && collections_val->type == JSON_ARRAY && json_array_size(collections_val) > 0) {
        /* Export specific collections */
        const char** collections = malloc(json_array_size(collections_val) * sizeof(char*));
        if (!collections) {
            json_free(body);
            return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                      "{\"error\":\"Memory allocation failed\"}", "application/json");
        }
        
        /* Extract collection names */
        for (size_t i = 0; i < json_array_size(collections_val); i++) {
            json_value_t* coll_val = json_array_get(collections_val, i);
            if (coll_val && coll_val->type == JSON_STRING) {
                collections[i] = coll_val->value.string;
            } else {
                collections[i] = NULL;
            }
        }
        
        /* Export collections */
        result = export_collections(ctx->db, output_path, collections, json_array_size(collections_val));
        
        /* Free collections array */
        free(collections);
    } else {
        /* Export entire database */
        result = export_database(ctx->db, output_path);
    }
    
    if (result != EXPORT_SUCCESS) {
        /* Create error response */
        json_value_t* error = json_create_object();
        json_object_set(error, "success", json_create_boolean(0));
        json_object_set(error, "error", json_create_string(export_error_message(result)));
        json_object_set(error, "code", json_create_integer(result));
        
        /* Serialize error */
        char* error_str = json_stringify(error);
        
        /* Free resources */
        json_free(error);
        json_free(body);
        
        /* Create HTTP response */
        http_response_t* http_response = create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                                           error_str, "application/json");
        
        /* Free error string */
        free(error_str);
        
        return http_response;
    }
    
    /* Create success response */
    json_value_t* response = json_create_object();
    json_object_set(response, "success", json_create_boolean(1));
    json_object_set(response, "message", json_create_string("Database exported successfully"));
    json_object_set(response, "path", json_create_string(output_path));
    
    /* Serialize response */
    char* response_str = json_stringify(response);
    
    /* Free resources */
    json_free(response);
    json_free(body);
    
    /* Create HTTP response */
    http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
    
    /* Free response string */
    free(response_str);
    
    return http_response;
}

/* Import database from a file */
http_response_t* api_handle_import(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->body) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Parse request body */
    json_value_t* body = json_parse(request->body);
    if (!body || body->type != JSON_OBJECT) {
        if (body) json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request body\"}", "application/json");
    }
    
    /* Extract input path */
    json_value_t* path_val = json_object_get(body, "path");
    if (!path_val || path_val->type != JSON_STRING) {
        json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Import path is required\"}", "application/json");
    }
    
    const char* input_path = path_val->value.string;
    
    /* Extract overwrite flag */
    int overwrite = 0;
    json_value_t* overwrite_val = json_object_get(body, "overwrite");
    if (overwrite_val && overwrite_val->type == JSON_BOOLEAN) {
        overwrite = overwrite_val->value.boolean;
    }
    
    /* Check for collections filter */
    json_value_t* collections_val = json_object_get(body, "collections");
    
    int result;
    if (collections_val && collections_val->type == JSON_ARRAY && json_array_size(collections_val) > 0) {
        /* Import specific collections */
        const char** collections = malloc(json_array_size(collections_val) * sizeof(char*));
        if (!collections) {
            json_free(body);
            return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                      "{\"error\":\"Memory allocation failed\"}", "application/json");
        }
        
        /* Extract collection names */
        for (size_t i = 0; i < json_array_size(collections_val); i++) {
            json_value_t* coll_val = json_array_get(collections_val, i);
            if (coll_val && coll_val->type == JSON_STRING) {
                collections[i] = coll_val->value.string;
            } else {
                collections[i] = NULL;
            }
        }
        
        /* Import collections */
        result = import_collections(ctx->db, input_path, collections, json_array_size(collections_val), overwrite);
        
        /* Free collections array */
        free(collections);
    } else {
        /* Import entire database */
        result = import_database(ctx->db, input_path, overwrite);
    }
    
    if (result != IMPORT_SUCCESS) {
        /* Create error response */
        json_value_t* error = json_create_object();
        json_object_set(error, "success", json_create_boolean(0));
        json_object_set(error, "error", json_create_string(import_error_message(result)));
        json_object_set(error, "code", json_create_integer(result));
        
        /* Serialize error */
        char* error_str = json_stringify(error);
        
        /* Free resources */
        json_free(error);
        json_free(body);
        
        /* Create HTTP response */
        http_response_t* http_response = create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                                           error_str, "application/json");
        
        /* Free error string */
        free(error_str);
        
        return http_response;
    }
    
    /* Create success response */
    json_value_t* response = json_create_object();
    json_object_set(response, "success", json_create_boolean(1));
    json_object_set(response, "message", json_create_string("Database imported successfully"));
    json_object_set(response, "path", json_create_string(input_path));
    
    /* Serialize response */
    char* response_str = json_stringify(response);
    
    /* Free resources */
    json_free(response);
    json_free(body);
    
    /* Create HTTP response */
    http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
    
    /* Free response string */
    free(response_str);
    
    return http_response;
}