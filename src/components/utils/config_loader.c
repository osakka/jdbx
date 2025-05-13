#include "utils/config_loader.h"
#include "utils/config_defaults.h"
#include "utils/json.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>  /* For PATH_MAX */
#include <libgen.h>  /* For dirname() and basename() */
#include <unistd.h>  /* For readlink() */

/* Global configuration structure - defined elsewhere when building tools */
#ifndef TOOLS_BUILD
server_config_t* g_server_config = NULL;
#endif

/* Path to the executable's directory, used for resolving relative paths */
static char g_binary_dir[PATH_MAX] = {0};

/**
 * Initialize the binary directory path for resolving relative paths
 * This should be called early in the program's execution
 */
void config_init_binary_dir(void) {
    if (g_binary_dir[0] != '\0') {
        /* Already initialized */
        return;
    }
    
    /* Get the path to the executable */
    char exe_path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", exe_path, PATH_MAX - 1);
    if (count == -1) {
        /* Fallback to current directory if readlink fails */
        if (getcwd(g_binary_dir, PATH_MAX - 1) == NULL) {
            /* Last resort, use a sensible default */
            strncpy(g_binary_dir, "/opt/jsondb", PATH_MAX - 1);
        }
        return;
    }
    
    /* Ensure null termination */
    exe_path[count] = '\0';
    
    /* Get the directory part */
    char* dir = dirname(exe_path);
    strncpy(g_binary_dir, dir, PATH_MAX - 1);
    g_binary_dir[PATH_MAX - 1] = '\0';
    
    if (g_logger) {
        LOG_DEBUG("Binary directory: %s", g_binary_dir);
    }
}

/**
 * Get the binary directory path
 * @return Binary directory path
 */
const char* config_get_binary_dir(void) {
    if (g_binary_dir[0] == '\0') {
        config_init_binary_dir();
    }
    return g_binary_dir;
}

/**
 * Resolve a path that might be relative to the binary directory
 * @param path Path to resolve (absolute or relative)
 * @return Resolved path (must be freed by caller)
 */
static char* resolve_path(const char* path) {
    if (!path) return NULL;
    
    /* If it's an absolute path, just duplicate it */
    if (path[0] == '/') {
        return strdup(path);
    }
    
    /* Make sure binary directory is initialized */
    if (g_binary_dir[0] == '\0') {
        config_init_binary_dir();
    }
    
    /* Allocate enough space for the full path */
    char* resolved_path = malloc(PATH_MAX);
    if (!resolved_path) {
        if (g_logger) {
            LOG_ERROR("Failed to allocate memory for path resolution");
        } else {
            fprintf(stderr, "Error: Failed to allocate memory for path resolution\n");
        }
        return NULL;
    }
    
    /* Combine binary directory with the relative path */
    snprintf(resolved_path, PATH_MAX, "%s/%s", g_binary_dir, path);
    
    if (g_logger) {
        LOG_DEBUG("Resolved path '%s' to '%s'", path, resolved_path);
    }
    
    return resolved_path;
}

/* Internal helper functions */
static char* trim_whitespace(char* str) {
    if (!str) return NULL;
    
    /* Trim leading whitespace */
    while (isspace(*str)) str++;
    
    /* All whitespace */
    if (*str == 0) return str;
    
    /* Trim trailing whitespace */
    char* end = str + strlen(str) - 1;
    while (end > str && isspace(*end)) end--;
    
    /* Null terminate the trimmed string */
    end[1] = '\0';
    
    return str;
}

/* Parse boolean value from string */
static int parse_bool(const char* value) {
    if (!value) return 0;
    
    char* trimmed = strdup(value);
    trimmed = trim_whitespace(trimmed);
    
    if (strcasecmp(trimmed, "true") == 0 ||
        strcasecmp(trimmed, "yes") == 0 ||
        strcasecmp(trimmed, "1") == 0 ||
        strcasecmp(trimmed, "on") == 0) {
        free(trimmed);
        return 1;
    }
    
    free(trimmed);
    return 0;
}

/* Load configuration from a JSON file */
int config_load_json(const char* filepath, server_config_t* config) {
    if (!filepath || !config) {
        if (g_logger) {
            LOG_ERROR("Invalid parameters for config_load_json");
        } else {
            fprintf(stderr, "Error: Invalid parameters for config_load_json\n");
        }
        return 0;
    }
    
    FILE* file = fopen(filepath, "r");
    if (!file) {
        if (g_logger) {
            LOG_ERROR("Could not open config file: %s", filepath);
        } else {
            fprintf(stderr, "Error: Could not open config file: %s\n", filepath);
        }
        return 0;
    }
    
    /* Determine file size */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    /* Read entire file into buffer */
    char* json_buffer = (char*)malloc(file_size + 1);
    if (!json_buffer) {
        if (g_logger) {
            LOG_ERROR("Failed to allocate memory for config file");
        } else {
            fprintf(stderr, "Error: Failed to allocate memory for config file\n");
        }
        fclose(file);
        return 0;
    }
    
    size_t read_size = fread(json_buffer, 1, file_size, file);
    fclose(file);
    
    /* Cast file_size to size_t to avoid signedness comparison warning */
    if (read_size != (size_t)file_size) {
        if (g_logger) {
            LOG_ERROR("Failed to read config file: %s", filepath);
        } else {
            fprintf(stderr, "Error: Failed to read config file: %s\n", filepath);
        }
        free(json_buffer);
        return 0;
    }
    
    json_buffer[file_size] = '\0';
    
    /* Parse JSON */
    json_value_t* json = json_parse(json_buffer);
    free(json_buffer);
    
    if (!json || json->type != JSON_OBJECT) {
        if (g_logger) {
            LOG_ERROR("Invalid JSON format in config file: %s", filepath);
        } else {
            fprintf(stderr, "Error: Invalid JSON format in config file: %s\n", filepath);
        }
        if (json) json_free(json);
        return 0;
    }
    
    if (g_logger) {
        LOG_INFO("Loading configuration from JSON file: %s", filepath);
    } else {
        printf("Loading configuration from JSON file: %s\n", filepath);
    }
    
    /* Parse server section */
    json_value_t* server_section = json_object_get(json, "server");
    if (server_section && server_section->type == JSON_OBJECT) {
        json_value_t* port_val = json_object_get(server_section, "port");
        if (port_val && port_val->type == JSON_INTEGER) {
            config->port = (int)port_val->value.integer;
            if (g_logger) {
                LOG_DEBUG("Config: Set port to %d", config->port);
            }
        }
        
        json_value_t* host_val = json_object_get(server_section, "host");
        if (host_val && host_val->type == JSON_STRING) {
            config->host = strdup(host_val->value.string);
            if (g_logger) {
                LOG_DEBUG("Config: Set host to '%s'", config->host);
            }
        } else {
            config->host = strdup(DEFAULT_HOST);
        }
        
        json_value_t* max_conn_val = json_object_get(server_section, "max_connections");
        if (max_conn_val && max_conn_val->type == JSON_INTEGER) {
            config->max_connections = (int)max_conn_val->value.integer;
            if (g_logger) {
                LOG_DEBUG("Config: Set max_connections to %d", config->max_connections);
            }
        } else {
            config->max_connections = DEFAULT_MAX_CONNECTIONS;
        }
    }
    
    /* Parse database section */
    json_value_t* db_section = json_object_get(json, "database");
    if (db_section && db_section->type == JSON_OBJECT) {
        json_value_t* path_val = json_object_get(db_section, "path");
        if (path_val && path_val->type == JSON_STRING) {
            config->db_path = strdup(path_val->value.string);
            if (g_logger) {
                LOG_DEBUG("Config: Set db_path to '%s'", config->db_path);
            }
        }
    }
    
    /* Parse rbac section */
    json_value_t* rbac_section = json_object_get(json, "rbac");
    if (rbac_section && rbac_section->type == JSON_OBJECT) {
        json_value_t* path_val = json_object_get(rbac_section, "path");
        if (path_val && path_val->type == JSON_STRING) {
            config->rbac_path = strdup(path_val->value.string);
            if (g_logger) {
                LOG_DEBUG("Config: Set rbac_path to '%s'", config->rbac_path);
            }
        }
    }
    
    /* Parse JWT section */
    json_value_t* jwt_section = json_object_get(json, "jwt");
    if (jwt_section && jwt_section->type == JSON_OBJECT) {
        json_value_t* secret_val = json_object_get(jwt_section, "secret");
        if (secret_val && secret_val->type == JSON_STRING) {
            config->jwt_secret = strdup(secret_val->value.string);
            if (g_logger) {
                LOG_DEBUG("Config: Set JWT secret");
            }
        }
    }
    
    /* Parse logging section */
    json_value_t* logging_section = json_object_get(json, "logging");
    if (logging_section && logging_section->type == JSON_OBJECT) {
        json_value_t* level_val = json_object_get(logging_section, "level");
        if (level_val && level_val->type == JSON_STRING) {
            const char* level_str = level_val->value.string;
            
            if (strcasecmp(level_str, "none") == 0) {
                config->log_level = LOG_LEVEL_NONE;
            } else if (strcasecmp(level_str, "error") == 0) {
                config->log_level = LOG_LEVEL_ERROR;
            } else if (strcasecmp(level_str, "warning") == 0) {
                config->log_level = LOG_LEVEL_WARNING;
            } else if (strcasecmp(level_str, "info") == 0) {
                config->log_level = LOG_LEVEL_INFO;
            } else if (strcasecmp(level_str, "debug") == 0) {
                config->log_level = LOG_LEVEL_DEBUG;
            } else if (strcasecmp(level_str, "trace") == 0) {
                config->log_level = LOG_LEVEL_TRACE;
            }
            
            if (g_logger) {
                LOG_DEBUG("Config: Set log_level to %d", config->log_level);
            }
        }
        
        json_value_t* file_val = json_object_get(logging_section, "file");
        if (file_val && file_val->type == JSON_STRING) {
            config->log_file = strdup(file_val->value.string);
            if (g_logger) {
                LOG_DEBUG("Config: Set log_file to '%s'", config->log_file);
            }
        }
    }
    
    /* Parse PID file setting */
    json_value_t* pid_file_val = json_object_get(json, "pid_file");
    if (pid_file_val && pid_file_val->type == JSON_STRING) {
        config->pid_file = strdup(pid_file_val->value.string);
        if (g_logger) {
            LOG_DEBUG("Config: Set pid_file to '%s'", config->pid_file);
        }
    }
    
    /* Parse foreground mode setting */
    json_value_t* foreground_val = json_object_get(json, "foreground_mode");
    if (foreground_val) {
        if (foreground_val->type == JSON_BOOLEAN) {
            config->foreground_mode = foreground_val->value.boolean;
        } else if (foreground_val->type == JSON_INTEGER) {
            config->foreground_mode = (foreground_val->value.integer != 0);
        } else if (foreground_val->type == JSON_STRING) {
            config->foreground_mode = parse_bool(foreground_val->value.string);
        }
        
        if (g_logger) {
            LOG_DEBUG("Config: Set foreground_mode to %d", config->foreground_mode);
        }
    }
    
    /* Parse CORS settings */
    json_value_t* cors_section = json_object_get(json, "cors");
    if (cors_section && cors_section->type == JSON_OBJECT) {
        /* Initialize CORS configuration */
        init_cors_config(&config->cors);
        
        json_value_t* enabled_val = json_object_get(cors_section, "enabled");
        if (enabled_val) {
            int enabled = 0;
            if (enabled_val->type == JSON_BOOLEAN) {
                enabled = enabled_val->value.boolean;
            } else if (enabled_val->type == JSON_INTEGER) {
                enabled = (enabled_val->value.integer != 0);
            } else if (enabled_val->type == JSON_STRING) {
                enabled = parse_bool(enabled_val->value.string);
            }
            
            config->cors.enabled = enabled;
            if (g_logger) {
                LOG_DEBUG("Config: Set CORS enabled to %d", config->cors.enabled);
            }
        }
        
        json_value_t* origins_val = json_object_get(cors_section, "allowed_origins");
        if (origins_val && origins_val->type == JSON_ARRAY) {
            /* Free existing origins if any */
            if (config->cors.allowed_origins) {
                for (int i = 0; i < config->cors.allowed_origins_count; i++) {
                    free(config->cors.allowed_origins[i]);
                }
                free(config->cors.allowed_origins);
                config->cors.allowed_origins = NULL;
                config->cors.allowed_origins_count = 0;
            }
            
            /* Allocate new origins array */
            size_t count = json_array_size(origins_val);
            config->cors.allowed_origins = (char**)malloc(count * sizeof(char*));
            config->cors.allowed_origins_count = count;
            
            /* Copy origins */
            for (size_t i = 0; i < count; i++) {
                json_value_t* origin = json_array_get(origins_val, i);
                if (origin && origin->type == JSON_STRING) {
                    config->cors.allowed_origins[i] = strdup(origin->value.string);
                    if (g_logger) {
                        LOG_DEBUG("Config: Added CORS allowed origin: '%s'", config->cors.allowed_origins[i]);
                    }
                } else {
                    config->cors.allowed_origins[i] = strdup("*");
                }
            }
        }
        
        json_value_t* methods_val = json_object_get(cors_section, "allowed_methods");
        if (methods_val && methods_val->type == JSON_ARRAY) {
            /* Free existing methods if any */
            if (config->cors.allowed_methods) {
                for (int i = 0; i < config->cors.allowed_methods_count; i++) {
                    free(config->cors.allowed_methods[i]);
                }
                free(config->cors.allowed_methods);
                config->cors.allowed_methods = NULL;
                config->cors.allowed_methods_count = 0;
            }
            
            /* Allocate new methods array */
            size_t count = json_array_size(methods_val);
            config->cors.allowed_methods = (char**)malloc(count * sizeof(char*));
            config->cors.allowed_methods_count = count;
            
            /* Copy methods */
            for (size_t i = 0; i < count; i++) {
                json_value_t* method = json_array_get(methods_val, i);
                if (method && method->type == JSON_STRING) {
                    config->cors.allowed_methods[i] = strdup(method->value.string);
                    if (g_logger) {
                        LOG_DEBUG("Config: Added CORS allowed method: '%s'", config->cors.allowed_methods[i]);
                    }
                }
            }
        }
        
        json_value_t* headers_val = json_object_get(cors_section, "allowed_headers");
        if (headers_val && headers_val->type == JSON_ARRAY) {
            /* Free existing headers if any */
            if (config->cors.allowed_headers) {
                for (int i = 0; i < config->cors.allowed_headers_count; i++) {
                    free(config->cors.allowed_headers[i]);
                }
                free(config->cors.allowed_headers);
                config->cors.allowed_headers = NULL;
                config->cors.allowed_headers_count = 0;
            }
            
            /* Allocate new headers array */
            size_t count = json_array_size(headers_val);
            config->cors.allowed_headers = (char**)malloc(count * sizeof(char*));
            config->cors.allowed_headers_count = count;
            
            /* Copy headers */
            for (size_t i = 0; i < count; i++) {
                json_value_t* header = json_array_get(headers_val, i);
                if (header && header->type == JSON_STRING) {
                    config->cors.allowed_headers[i] = strdup(header->value.string);
                    if (g_logger) {
                        LOG_DEBUG("Config: Added CORS allowed header: '%s'", config->cors.allowed_headers[i]);
                    }
                }
            }
        }
    }
    
    /* Free the JSON object */
    json_free(json);
    
    if (g_logger) {
        LOG_INFO("Configuration loaded successfully from %s", filepath);
    } else {
        printf("Configuration loaded successfully from %s\n", filepath);
    }
    
    return 1;
}

/* Load configuration from key=value file */
int config_load_keyvalue(const char* filepath, server_config_t* config) {
    if (!filepath || !config) {
        if (g_logger) {
            LOG_ERROR("Invalid parameters for config_load_keyvalue");
        } else {
            fprintf(stderr, "Error: Invalid parameters for config_load_keyvalue\n");
        }
        return 0;
    }
    
    FILE* file = fopen(filepath, "r");
    if (!file) {
        if (g_logger) {
            LOG_ERROR("Could not open config file: %s", filepath);
        } else {
            fprintf(stderr, "Error: Could not open config file: %s\n", filepath);
        }
        return 0;
    }
    
    if (g_logger) {
        LOG_INFO("Loading configuration from key-value file: %s", filepath);
    } else {
        printf("Loading configuration from key-value file: %s\n", filepath);
    }
    
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }
        
        /* Find '=' character */
        char* equals = strchr(line, '=');
        if (!equals) {
            continue;
        }
        
        /* Split key and value */
        *equals = '\0';
        char* key = trim_whitespace(line);
        char* value = trim_whitespace(equals + 1);
        
        /* Remove quotes from value if present */
        size_t len = strlen(value);
        if (len >= 2 && value[0] == '"' && value[len - 1] == '"') {
            value[len - 1] = '\0';
            value++;
        }
        
        /* Process key/value pairs */
        if (strcasecmp(key, "port") == 0) {
            config->port = atoi(value);
            if (g_logger) {
                LOG_DEBUG("Config: Set port to %d", config->port);
            }
        } else if (strcasecmp(key, "host") == 0) {
            config->host = strdup(value);
            if (g_logger) {
                LOG_DEBUG("Config: Set host to '%s'", config->host);
            }
        } else if (strcasecmp(key, "max_connections") == 0) {
            config->max_connections = atoi(value);
            if (g_logger) {
                LOG_DEBUG("Config: Set max_connections to %d", config->max_connections);
            }
        } else if (strcasecmp(key, "db_path") == 0) {
            config->db_path = strdup(value);
            if (g_logger) {
                LOG_DEBUG("Config: Set db_path to '%s'", config->db_path);
            }
        } else if (strcasecmp(key, "rbac_path") == 0) {
            config->rbac_path = strdup(value);
            if (g_logger) {
                LOG_DEBUG("Config: Set rbac_path to '%s'", config->rbac_path);
            }
        } else if (strcasecmp(key, "jwt_secret") == 0) {
            config->jwt_secret = strdup(value);
            if (g_logger) {
                LOG_DEBUG("Config: Set JWT secret");
            }
        } else if (strcasecmp(key, "pid_file") == 0) {
            config->pid_file = strdup(value);
            if (g_logger) {
                LOG_DEBUG("Config: Set pid_file to '%s'", config->pid_file);
            }
        } else if (strcasecmp(key, "log_file") == 0) {
            config->log_file = strdup(value);
            if (g_logger) {
                LOG_DEBUG("Config: Set log_file to '%s'", config->log_file);
            }
        } else if (strcasecmp(key, "log_level") == 0) {
            if (strcasecmp(value, "none") == 0) {
                config->log_level = LOG_LEVEL_NONE;
            } else if (strcasecmp(value, "error") == 0) {
                config->log_level = LOG_LEVEL_ERROR;
            } else if (strcasecmp(value, "warning") == 0) {
                config->log_level = LOG_LEVEL_WARNING;
            } else if (strcasecmp(value, "info") == 0) {
                config->log_level = LOG_LEVEL_INFO;
            } else if (strcasecmp(value, "debug") == 0) {
                config->log_level = LOG_LEVEL_DEBUG;
            } else if (strcasecmp(value, "trace") == 0) {
                config->log_level = LOG_LEVEL_TRACE;
            }
            if (g_logger) {
                LOG_DEBUG("Config: Set log_level to %d", config->log_level);
            }
        } else if (strcasecmp(key, "foreground_mode") == 0) {
            config->foreground_mode = parse_bool(value);
            if (g_logger) {
                LOG_DEBUG("Config: Set foreground_mode to %d", config->foreground_mode);
            }
        } else if (strcasecmp(key, "js_enabled") == 0) {
            config->js_enabled = parse_bool(value);
            if (g_logger) {
                LOG_DEBUG("Config: Set js_enabled to %d", config->js_enabled);
            }
        }
        /* CORS settings could be handled here but would require more complex parsing */
    }
    
    fclose(file);
    
    if (g_logger) {
        LOG_INFO("Configuration loaded successfully from %s", filepath);
    } else {
        printf("Configuration loaded successfully from %s\n", filepath);
    }
    
    return 1;
}

/* Detect config file type and load it */
int config_load(const char* filepath, server_config_t* config) {
    if (!filepath || !config) {
        if (g_logger) {
            LOG_ERROR("Invalid parameters for config_load");
        } else {
            fprintf(stderr, "Error: Invalid parameters for config_load\n");
        }
        return 0;
    }
    
    /* Determine file extension */
    const char* ext = strrchr(filepath, '.');
    if (!ext) {
        /* Default to key=value if no extension */
        return config_load_keyvalue(filepath, config);
    }
    
    if (strcasecmp(ext, ".json") == 0) {
        return config_load_json(filepath, config);
    } else if (strcasecmp(ext, ".conf") == 0) {
        return config_load_keyvalue(filepath, config);
    } else {
        /* Unknown extension, try key=value format */
        return config_load_keyvalue(filepath, config);
    }
}

/* Free configuration resources */
void config_free(server_config_t* config) {
    if (!config) return;
    
    /* Free string resources */
    if (config->host) free(config->host);
    if (config->db_path) free(config->db_path);
    if (config->rbac_path) free(config->rbac_path);
    if (config->jwt_secret) free(config->jwt_secret);
    if (config->pid_file) free(config->pid_file);
    if (config->log_file) free(config->log_file);
    
    /* Free CORS configuration */
    if (config->cors.allowed_origins) {
        for (int i = 0; i < config->cors.allowed_origins_count; i++) {
            free(config->cors.allowed_origins[i]);
        }
        free(config->cors.allowed_origins);
    }
    
    if (config->cors.allowed_methods) {
        for (int i = 0; i < config->cors.allowed_methods_count; i++) {
            free(config->cors.allowed_methods[i]);
        }
        free(config->cors.allowed_methods);
    }
    
    if (config->cors.allowed_headers) {
        for (int i = 0; i < config->cors.allowed_headers_count; i++) {
            free(config->cors.allowed_headers[i]);
        }
        free(config->cors.allowed_headers);
    }
    
    /* Reset configuration */
    memset(config, 0, sizeof(server_config_t));
}

/**
 * Initialize configuration with default values from config_defaults.h
 * @param config Pointer to the configuration structure to initialize
 */
void config_init_defaults(server_config_t* config) {
    if (!config) return;
    
    /* Clear any existing configuration */
    config_free(config);
    
    /* Ensure binary directory is initialized for path resolution */
    config_init_binary_dir();
    
    /* Server settings */
    config->port = DEFAULT_PORT;
    config->host = strdup(DEFAULT_HOST);
    config->max_connections = DEFAULT_MAX_CONNECTIONS;
    
    /* Runtime settings */
    config->foreground_mode = DEFAULT_FOREGROUND_MODE;
    config->log_level = DEFAULT_LOG_LEVEL;
    config->js_enabled = DEFAULT_JS_ENABLED;
    config->use_ssl = DEFAULT_SSL_ENABLED;
    
    /* File paths (resolved relative to binary directory) */
    config->db_path = resolve_path(DEFAULT_DB_PATH);
    config->rbac_path = resolve_path(DEFAULT_RBAC_PATH);
    config->pid_file = resolve_path(DEFAULT_PID_FILE);
    config->log_file = resolve_path(DEFAULT_LOG_FILE);
    
    /* Security settings */
    config->jwt_secret = strdup(DEFAULT_JWT_SECRET);
    
    /* Initialize CORS configuration */
    init_cors_config(&config->cors);
    
    /* Additional settings */
    config->cors.enabled = DEFAULT_CORS_ENABLED;
    config->cors.allow_credentials = DEFAULT_CORS_ALLOW_CREDENTIALS;
    config->cors.max_age = DEFAULT_CORS_MAX_AGE;
    
    if (g_logger) {
        LOG_INFO("Configuration initialized with default values");
        LOG_DEBUG("Default port: %d", config->port);
        LOG_DEBUG("Default host: %s", config->host);
        LOG_DEBUG("Default DB path: %s", config->db_path);
        LOG_DEBUG("Default RBAC path: %s", config->rbac_path);
        LOG_DEBUG("Default PID file: %s", config->pid_file);
        LOG_DEBUG("Default log file: %s", config->log_file);
    }
}