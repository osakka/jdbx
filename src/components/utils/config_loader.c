#include "utils/config_loader.h"
#include "utils/config_defaults.h"
#include "utils/json.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include "utils/config_string_pool.h"
#include "utils/ssl.h"
#include "core/server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h> /* For PATH_MAX */
#include <libgen.h> /* For dirname() and basename() */
#include <unistd.h> /* For readlink() */
#include <errno.h>  /* For errno and strerror() */
#include <sys/stat.h> /* For stat() */

/* Global configuration structure - defined elsewhere when building tools */
#ifndef TOOLS_BUILD
server_config_t* g_server_config = NULL;
#endif

/* Path to the executable's directory, used for resolving relative paths */
static char g_binary_dir[PATH_MAX] = {0};

/**
 * Get string representation of JSON type
 * @param type JSON value type
 * @return String representation of the type
 */
static const char* json_type_name(int type) {
  switch (type) {
    case JSON_NULL:
      return "null";
    case JSON_BOOLEAN:
      return "boolean";
    case JSON_INTEGER:
      return "integer";
    case JSON_NUMBER:
      return "number";
    case JSON_STRING:
      return "string";
    case JSON_ARRAY:
      return "array";
    case JSON_OBJECT:
      return "object";
    default:
      return "unknown";
  }
}

/**
 * Initialize the binary directory path for resolving relative paths
 * This should be called early in the program's execution
 */
void config_init_binary_dir(void) {
  if (g_logger) {
    TRACE_API("Entering config_init_binary_dir().");
  }
  
  if (g_binary_dir[0] != '\0') {
    /* Already initialized */
    if (g_logger) {
      LOG_DEBUG("Binary directory already initialized: %s", g_binary_dir);
      TRACE_API("Exiting config_init_binary_dir() - already initialized.");
    }
    return;
  }
  
  /* Get the path to the executable */
  char exe_path[PATH_MAX];
  if (g_logger) {
    LOG_DEBUG("Attempting to resolve binary directory from /proc/self/exe.");
  }
  
  ssize_t count = readlink("/proc/self/exe", exe_path, PATH_MAX - 1);
  if (count == -1) {
    /* Fallback to current directory if readlink fails */
    if (g_logger) {
      LOG_WARNING("Cannot read /proc/self/exe: %s", strerror(errno));
      LOG_DEBUG("Falling back to current working directory.");
    }
    
    if (getcwd(g_binary_dir, PATH_MAX - 1) == NULL) {
      /* Last resort, use a sensible default */
      if (g_logger) {
        LOG_WARNING("Cannot get current working directory: %s", strerror(errno));
        LOG_DEBUG("Falling back to base path resolution.");
      }
      /* Try JSONDB_BASE_PATH environment variable first */
      const char* base_path = getenv("JSONDB_BASE_PATH");
      if (base_path) {
        strncpy(g_binary_dir, base_path, PATH_MAX - 1);
      } else {
        /* Default to current directory if no base path is set */
        strncpy(g_binary_dir, ".", PATH_MAX - 1);
      }
    } else {
      LOG_INFO("Using current directory as binary directory: %s", g_binary_dir);
    }
    g_binary_dir[PATH_MAX - 1] = '\0';
    
    TRACE_API("Exiting config_init_binary_dir() - using fallback.");
    return;
  }
  
  /* Ensure null termination */
  exe_path[count] = '\0';
  if (g_logger) {
    LOG_DEBUG("Executable path: %s", exe_path);
  }
  
  /* Get the directory part */
  char* dir = dirname(exe_path);
  strncpy(g_binary_dir, dir, PATH_MAX - 1);
  g_binary_dir[PATH_MAX - 1] = '\0';
  
  LOG_INFO("Binary directory initialized: %s", g_binary_dir);
  TRACE_API("Exiting config_init_binary_dir() - success.");
}

/**
 * Get the binary directory path
 * @return Binary directory path
 */
const char* config_get_binary_dir(void) {
  TRACE_API("Entering config_get_binary_dir().");
  
  if (g_binary_dir[0] == '\0') {
    LOG_DEBUG("Binary directory not initialized, initializing now.");
    config_init_binary_dir();
  }
  
  TRACE_API("Exiting config_get_binary_dir(): returning '%s'", g_binary_dir);
  return g_binary_dir;
}

/**
 * Resolve a path that might be relative to the binary directory
 * @param path Path to resolve (absolute or relative)
 * @return Resolved path (must be freed by caller)
 */
static char* resolve_path(const char* path) {
  TRACE_API("Entering resolve_path(path='%s')", path ? path : "NULL");
  
  if (!path) {
    LOG_ERROR("NULL path provided to resolve_path().");
    TRACE_API("Exiting resolve_path() - NULL path.");
    return NULL;
  }
  
  /* If it's an absolute path, just duplicate it */
  if (path[0] == '/') {
    LOG_DEBUG("Path '%s' is absolute, returning duplicate", path);
    TRACE_API("Exiting resolve_path() - absolute path.");
    return strdup(path);
  }
  
  /* Make sure binary directory is initialized */
  if (g_binary_dir[0] == '\0') {
    LOG_DEBUG("Binary directory not initialized, initializing for path resolution.");
    config_init_binary_dir();
  }
  
  /* Handle relative paths with special care for paths containing ../ */
  char* working_path = malloc(PATH_MAX);
  if (!working_path) {
    LOG_ERROR("Cannot allocate memory for working path of '%s'", path);
    return NULL;
  }
  
  /* Start with the binary directory */
  strncpy(working_path, g_binary_dir, PATH_MAX - 1);
  working_path[PATH_MAX - 1] = '\0';
  
  /* Allocate space for the absolute path result */
  char* resolved_path = malloc(PATH_MAX);
  if (!resolved_path) {
    LOG_ERROR("Cannot allocate memory for path resolution of '%s'", path);
    TRACE_API("Exiting resolve_path() - memory allocation failure.");
    free(working_path);
    return NULL;
  }
  
  /* Use realpath to resolve the path properly */
  char temp_path[PATH_MAX];
  snprintf(temp_path, PATH_MAX - 1, "%s/%s", working_path, path);
  temp_path[PATH_MAX - 1] = '\0';
  
  LOG_DEBUG("Resolving combined path: '%s'", temp_path);
  
  /* Manually resolve the path to handle ../ properly */
  if (realpath(temp_path, resolved_path) == NULL) {
    LOG_WARNING("Could not resolve path with realpath: %s", strerror(errno));
    LOG_DEBUG("Using simple concatenation instead.");
    
    /* Fall back to simple concatenation if realpath fails */
    int bytes_written = snprintf(resolved_path, PATH_MAX - 1, "%s/%s", g_binary_dir, path);
    resolved_path[PATH_MAX - 1] = '\0';
    
    if (bytes_written < 0 || bytes_written >= PATH_MAX - 1) {
      LOG_ERROR("Path truncation detected in fallback for '%s'", path);
    }
  }
  
  free(working_path);
  
  LOG_DEBUG("Resolved '%s' to '%s'", path, resolved_path);
  
  /* Note: Error handling for possible path issues will be done by the caller */
  
  LOG_DEBUG("Resolved path '%s' to '%s'", path, resolved_path);
  TRACE_API("Exiting resolve_path() - success.");
  
  return resolved_path;
}

/* Internal helper functions */
/**
 * Trim whitespace from the beginning and end of a string
 * @param str String to trim (modified in place)
 * @return Pointer to the trimmed string
 */
static char* trim_whitespace(char* str) {
  TRACE_API("Entering trim_whitespace(str='%s')", str ? str : "NULL");
  
  if (!str) {
    TRACE_API("Exiting trim_whitespace() - NULL input.");
    return NULL;
  }
  
  /* Trim leading whitespace */
  char* start = str;
  while (isspace(*start)) start++;
  
  /* All whitespace */
  if (*start == 0) {
    TRACE_API("String contains only whitespace.");
    TRACE_API("Exiting trim_whitespace() - empty result.");
    return start;
  }
  
  /* Trim trailing whitespace */
  char* end = str + strlen(str) - 1;
  while (end > start && isspace(*end)) end--;
  
  /* Null terminate the trimmed string */
  end[1] = '\0';
  
  if (start != str || end != (str + strlen(str) - 1)) {
    TRACE_API("Trimmed whitespace: '%s' -> '%s'", str, start);
  } else {
    TRACE_API("No whitespace to trim: '%s'", str);
  }
  TRACE_API("Exiting trim_whitespace() - success.");
  
  return start;
}

/**
 * Parse a boolean value from a string
 * @param value String to parse ("true", "yes", "1", "on" are considered true)
 * @return 1 for true, 0 for false or invalid input
 */
static int parse_bool(const char* value) {
  if (g_logger) {
    TRACE_API("Entering parse_bool(value='%s')", value ? value : "NULL");
  }
  
  if (!value) {
    if (g_logger) {
      LOG_DEBUG("NULL value provided to parse_bool(), returning false.");
      TRACE_API("Exiting parse_bool() - NULL value.");
    }
    return 0;
  }
  
  char* trimmed = strdup(value);
  if (!trimmed) {
    if (g_logger) {
      LOG_ERROR("Cannot allocate memory in parse_bool().");
      TRACE_API("Exiting parse_bool() - memory allocation failure.");
    }
    return 0;
  }
  
  trimmed = trim_whitespace(trimmed);
  
  int result = 0;
  if (strcasecmp(trimmed, "true") == 0 ||
    strcasecmp(trimmed, "yes") == 0 ||
    strcasecmp(trimmed, "1") == 0 ||
    strcasecmp(trimmed, "on") == 0) {
    if (g_logger) {
      LOG_DEBUG("Parsed '%s' as TRUE", value);
    }
    result = 1;
  } else {
    if (g_logger) {
      LOG_DEBUG("Parsed '%s' as FALSE", value);
    }
    result = 0;
  }
  
  free(trimmed);
  
  if (g_logger) {
    TRACE_API("Exiting parse_bool() - returning %d", result);
  }
  return result;
}

/**
 * Load configuration from a JSON file
 * @param filepath Path to the JSON configuration file
 * @param config Pointer to the configuration structure to populate
 * @return 1 on success, 0 on failure
 */
int config_load_json(const char* filepath, server_config_t* config) {
  TRACE_API("Entering config_load_json(filepath='%s')", filepath ? filepath : "NULL");
  
  if (!filepath || !config) {
    LOG_ERROR("Invalid parameters for config_load_json: filepath=%p, config=%p", 
         (void*)filepath, (void*)config);
    TRACE_API("Exiting config_load_json() - invalid parameters.");
    return 0;
  }
  
  LOG_INFO("Loading configuration from JSON file: %s", filepath);
  LOG_DEBUG("Opening config file for reading.");
  
  FILE* file = fopen(filepath, "r");
  if (!file) {
    LOG_ERROR("Could not open config file: %s - %s", filepath, strerror(errno));
    TRACE_API("Exiting config_load_json() - failed to open file.");
    return 0;
  }
  
  /* Determine file size */
  TRACE_API("Getting file size.");
  
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);
  
  LOG_DEBUG("Config file size: %ld bytes", file_size);
  
  if (file_size <= 0) {
    LOG_WARNING("Config file is empty or could not determine size: %s", filepath);
    fclose(file);
    
    TRACE_API("Exiting config_load_json() - empty file.");
    return 0;
  }
  
  /* Read entire file into buffer */
  TRACE_API("Allocating memory for file contents (%ld bytes)", file_size + 1);
  
  char* json_buffer = (char*)malloc(file_size + 1);
  if (!json_buffer) {
    LOG_ERROR("Cannot allocate memory for config file (%ld bytes)", file_size + 1);
    TRACE_API("Exiting config_load_json() - memory allocation failure.");
    fclose(file);
    return 0;
  }
  
  TRACE_API("Reading file contents.");
  
  size_t read_size = fread(json_buffer, 1, file_size, file);
  fclose(file);
  
  /* Cast file_size to size_t to avoid signedness comparison warning */
  if (read_size != (size_t)file_size) {
    LOG_ERROR("Cannot read config file: %s (read %zu of %ld bytes)", 
         filepath, read_size, file_size);
    TRACE_API("Exiting config_load_json() - file read error.");
    free(json_buffer);
    return 0;
  }
  
  json_buffer[file_size] = '\0';
  
  TRACE_API("Parsing JSON content.");
  
  /* Parse JSON */
  json_value_t* json = json_parse(json_buffer);
  
  TRACE_API("Freeing file buffer.");
  
  free(json_buffer);
  
  if (!json) {
    LOG_ERROR("Cannot parse JSON in config file: %s", filepath);
    TRACE_API("Exiting config_load_json() - JSON parse error.");
    return 0;
  }
  
  if (json->type != JSON_OBJECT) {
    LOG_ERROR("Invalid JSON format in config file: %s (expected object, got %s)", 
         filepath, json_type_name(json->type));
    TRACE_API("Exiting config_load_json() - Invalid JSON format.");
    json_free(json);
    return 0;
  }
  
  LOG_INFO("read and parsed JSON configuration file: %s", filepath);
  LOG_DEBUG("Processing configuration sections.");
  
  /* Parse server section */
  LOG_DEBUG("Processing 'server' section.");
  TRACE_API("Looking for 'server' object in configuration.");
  
  json_value_t* server_section = json_object_get(json, "server");
  if (server_section) {
    if (server_section->type == JSON_OBJECT) {
      TRACE_API("Found 'server' section with %zu properties", json_object_size(server_section));
      
      /* Process port setting */
      TRACE_API("Looking for 'port' property in server section.");
      
      json_value_t* port_val = json_object_get(server_section, "port");
      if (port_val) {
        if (port_val->type == JSON_INTEGER) {
          config->port = (int)port_val->value.integer;
          LOG_DEBUG("Config: Set port to %d", config->port);
        } else {
          LOG_WARNING("Invalid type for 'port' in config (expected integer, got %s), using default: %d", 
                json_type_name(port_val->type), DEFAULT_PORT);
          config->port = DEFAULT_PORT;
        }
      } else {
        LOG_DEBUG("No 'port' specified in config, using default: %d", DEFAULT_PORT);
        config->port = DEFAULT_PORT;
      }
      
      /* Process host setting */
      if (g_logger) {
        TRACE_API("Looking for 'host' property in server section.");
      }
      
      json_value_t* host_val = json_object_get(server_section, "host");
      if (host_val) {
        if (host_val->type == JSON_STRING) {
          config->host = strdup(host_val->value.string);
          if (g_logger) {
            LOG_DEBUG("Config: Set host to '%s'", config->host);
          }
        } else {
          if (g_logger) {
            LOG_WARNING("Invalid type for 'host' in config (expected string, got %s), using default: %s", 
                  json_type_name(host_val->type), DEFAULT_HOST);
          }
          config->host = strdup(DEFAULT_HOST);
        }
      } else {
        if (g_logger) {
          LOG_DEBUG("No 'host' specified in config, using default: %s", DEFAULT_HOST);
        }
        config->host = strdup(DEFAULT_HOST);
      }
      
      /* Process max_connections setting */
      if (g_logger) {
        TRACE_API("Looking for 'max_connections' property in server section.");
      }
      
      json_value_t* max_conn_val = json_object_get(server_section, "max_connections");
      if (max_conn_val) {
        if (max_conn_val->type == JSON_INTEGER) {
          config->max_connections = (int)max_conn_val->value.integer;
          
          /* Check for unreasonable value */
          if (config->max_connections <= 0) {
            if (g_logger) {
              LOG_WARNING("Invalid value for 'max_connections' (%d), must be positive, using default: %d", 
                    config->max_connections, DEFAULT_MAX_CONNECTIONS);
            }
            config->max_connections = DEFAULT_MAX_CONNECTIONS;
          } else if (config->max_connections > 10000) {
            if (g_logger) {
              LOG_WARNING("Very high 'max_connections' value (%d) may consume excessive resources", 
                    config->max_connections);
            }
          } else {
            if (g_logger) {
              LOG_DEBUG("Config: Set max_connections to %d", config->max_connections);
            }
          }
        } else {
          if (g_logger) {
            LOG_WARNING("Invalid type for 'max_connections' in config (expected integer, got %s), using default: %d", 
                  json_type_name(max_conn_val->type), DEFAULT_MAX_CONNECTIONS);
          }
          config->max_connections = DEFAULT_MAX_CONNECTIONS;
        }
      } else {
        if (g_logger) {
          LOG_DEBUG("No 'max_connections' specified in config, using default: %d", DEFAULT_MAX_CONNECTIONS);
        }
        config->max_connections = DEFAULT_MAX_CONNECTIONS;
      }
    } else {
      if (g_logger) {
        LOG_WARNING("'server' section is not an object (found %s), using defaults", 
              json_type_name(server_section->type));
      }
      config->port = DEFAULT_PORT;
      config->host = strdup(DEFAULT_HOST);
      config->max_connections = DEFAULT_MAX_CONNECTIONS;
    }
  } else {
    if (g_logger) {
      LOG_INFO("No 'server' section found in config, using defaults.");
    }
    config->port = DEFAULT_PORT;
    config->host = strdup(DEFAULT_HOST);
    config->max_connections = DEFAULT_MAX_CONNECTIONS;
  }
  
  /* Parse database section */
  if (g_logger) {
    LOG_DEBUG("Processing 'database' section.");
    TRACE_API("Looking for 'database' object in configuration.");
  }
  
  json_value_t* db_section = json_object_get(json, "database");
  if (db_section) {
    if (db_section->type == JSON_OBJECT) {
      if (g_logger) {
        TRACE_API("Found 'database' section with %zu properties", json_object_size(db_section));
      }
      
      /* Process database path setting */
      if (g_logger) {
        TRACE_API("Looking for 'path' property in database section.");
      }
      
      json_value_t* path_val = json_object_get(db_section, "path");
      if (path_val) {
        if (path_val->type == JSON_STRING) {
          /* Free existing path if it exists */
          if (config->db_path) {
            if (g_logger) {
              TRACE_API("Freeing existing db_path: '%s'", config->db_path);
            }
            free(config->db_path);
          }
          
          config->db_path = buffer_pool_strdup(path_val->value.string);
          if (g_logger) {
            LOG_DEBUG("Config: Set db_path to '%s'", config->db_path);
            
            /* Special warning for default path in production */
            if (strcmp(config->db_path, DEFAULT_DB_PATH) == 0) {
              LOG_WARNING("Using default database path in configuration - "
                   "this may not be suitable for production use");
            }
          }
        } else {
          if (g_logger) {
            LOG_WARNING("Invalid type for 'path' in database section (expected string, got %s)", 
                  json_type_name(path_val->type));
          }
        }
      } else {
        if (g_logger) {
          LOG_INFO("No database path specified, using default: %s", DEFAULT_DB_PATH);
          
          /* Resolve the default path */
          char* resolved_path = resolve_path(DEFAULT_DB_PATH);
          if (resolved_path) {
            config->db_path = resolved_path;
            LOG_DEBUG("Resolved default database path to: %s", config->db_path);
          } else {
            LOG_ERROR("Cannot resolve default database path.");
            config->db_path = strdup(DEFAULT_DB_PATH);
          }
        }
      }
    } else {
      if (g_logger) {
        LOG_WARNING("'database' section is not an object (found %s), using defaults", 
              json_type_name(db_section->type));
      }
    }
  } else {
    if (g_logger) {
      LOG_INFO("No 'database' section found in config, using defaults.");
      
      /* Resolve the default path */
      char* resolved_path = resolve_path(DEFAULT_DB_PATH);
      if (resolved_path) {
        config->db_path = resolved_path;
        LOG_DEBUG("Resolved default database path to: %s", config->db_path);
      } else {
        LOG_ERROR("Cannot resolve default database path.");
        config->db_path = strdup(DEFAULT_DB_PATH);
      }
    }
  }
  
  /* RBAC section removed - RBAC is now stored in the database */
  /* Skip processing 'rbac' section as it's no longer needed */
  
  /* Parse JWT section */
  if (g_logger) {
    LOG_DEBUG("Processing 'jwt' section.");
    TRACE_API("Looking for 'jwt' object in configuration.");
  }
  
  json_value_t* jwt_section = json_object_get(json, "jwt");
  if (jwt_section) {
    if (jwt_section->type == JSON_OBJECT) {
      if (g_logger) {
        TRACE_API("Found 'jwt' section with %zu properties", json_object_size(jwt_section));
      }
      
      /* Process JWT secret setting */
      if (g_logger) {
        TRACE_API("Looking for 'secret' property in JWT section.");
      }
      
      json_value_t* secret_val = json_object_get(jwt_section, "secret");
      if (secret_val) {
        if (secret_val->type == JSON_STRING) {
          /* Free existing secret if it exists */
          if (config->jwt_secret) {
            if (g_logger) {
              TRACE_API("Freeing existing JWT secret.");
            }
            free(config->jwt_secret);
          }
          
          config->jwt_secret = strdup(secret_val->value.string);
          if (g_logger) {
            LOG_DEBUG("Config: Set JWT secret.");
            
            /* Security checks on JWT secret */
            if (secret_val->value.string && strlen(secret_val->value.string) < 16) {
              LOG_WARNING("JWT secret is too short (< 16 chars), this is a security risk.");
            }
            
            if (secret_val->value.string && strcmp(secret_val->value.string, DEFAULT_JWT_SECRET) == 0) {
              LOG_ERROR("Using default JWT secret in configuration - "
                  "this is a SEVERE SECURITY RISK in production!");
            }
          }
        } else {
          if (g_logger) {
            LOG_WARNING("Invalid type for 'secret' in JWT section (expected string, got %s)", 
                  json_type_name(secret_val->type));
          }
        }
      } else {
        if (g_logger) {
          LOG_WARNING("No JWT secret specified, using default (INSECURE for production).");
          
          /* Only in debug, never log secrets at INFO or above */
          TRACE_API("Using default JWT secret: %s", DEFAULT_JWT_SECRET);
        }
        
        config->jwt_secret = strdup(DEFAULT_JWT_SECRET);
      }
    } else {
      if (g_logger) {
        LOG_WARNING("'jwt' section is not an object (found %s), using defaults", 
              json_type_name(jwt_section->type));
      }
      
      config->jwt_secret = strdup(DEFAULT_JWT_SECRET);
    }
  } else {
    if (g_logger) {
      LOG_WARNING("No 'jwt' section found in config, using default secret (INSECURE for production).");
      config->jwt_secret = strdup(DEFAULT_JWT_SECRET);
    }
  }
  
  /* Parse SSL section */
  if (g_logger) {
    LOG_DEBUG("Processing 'ssl' section.");
    TRACE_API("Looking for 'ssl' object in configuration.");
  }
  
  json_value_t* ssl_section = json_object_get(json, "ssl");
  if (ssl_section) {
    if (ssl_section->type == JSON_OBJECT) {
      if (g_logger) {
        TRACE_API("Found 'ssl' section with %zu properties", json_object_size(ssl_section));
      }
      
      /* Process SSL enabled setting */
      if (g_logger) {
        TRACE_API("Looking for 'enabled' property in SSL section.");
      }
      
      json_value_t* enabled_val = json_object_get(ssl_section, "enabled");
      if (enabled_val) {
        int old_ssl = config->use_ssl;
        
        if (enabled_val->type == JSON_BOOLEAN) {
          config->use_ssl = enabled_val->value.boolean;
        } else if (enabled_val->type == JSON_INTEGER) {
          config->use_ssl = (enabled_val->value.integer != 0);
        } else if (enabled_val->type == JSON_STRING) {
          config->use_ssl = parse_bool(enabled_val->value.string);
        } else {
          if (g_logger) {
            LOG_WARNING("Invalid type for 'enabled' in SSL section (expected boolean, integer, or string, got %s), using default: %d", 
                  json_type_name(enabled_val->type), DEFAULT_SSL_ENABLED);
          }
          config->use_ssl = DEFAULT_SSL_ENABLED;
        }
        
        if (g_logger) {
          LOG_DEBUG("Config: Set SSL enabled from %d to %d", old_ssl, config->use_ssl);
          
          if (config->use_ssl) {
            LOG_INFO("SSL support enabled.");
          } else {
            LOG_INFO("SSL support disabled.");
          }
        }
      } else {
        if (g_logger) {
          LOG_DEBUG("No SSL enabled setting specified, using default: %d", DEFAULT_SSL_ENABLED);
          config->use_ssl = DEFAULT_SSL_ENABLED;
        }
      }
      
      /* Process SSL certificate file setting */
      if (g_logger) {
        TRACE_API("Looking for 'cert_file' property in SSL section.");
      }
      
      json_value_t* cert_val = json_object_get(ssl_section, "cert_file");
      if (cert_val) {
        if (cert_val->type == JSON_STRING) {
          /* Free existing cert path if it exists */
          if (config->cert_path) {
            if (g_logger) {
              TRACE_API("Freeing existing cert_path: '%s'", config->cert_path);
            }
            free(config->cert_path);
          }
          
          config->cert_path = strdup(cert_val->value.string);
          if (g_logger) {
            LOG_DEBUG("Config: Set SSL certificate file to '%s'", config->cert_path);
          }
        } else {
          if (g_logger) {
            LOG_WARNING("Invalid type for 'cert_file' in SSL section (expected string, got %s)", 
                  json_type_name(cert_val->type));
          }
        }
      } else {
        if (g_logger) {
          LOG_DEBUG("No SSL certificate file specified, using default: %s", DEFAULT_SSL_CERT_PATH);
        }
      }
      
      /* Process SSL private key file setting */
      if (g_logger) {
        TRACE_API("Looking for 'key_file' property in SSL section.");
      }
      
      json_value_t* key_val = json_object_get(ssl_section, "key_file");
      if (key_val) {
        if (key_val->type == JSON_STRING) {
          /* Free existing key path if it exists */
          if (config->key_path) {
            if (g_logger) {
              TRACE_API("Freeing existing key_path: '%s'", config->key_path);
            }
            free(config->key_path);
          }
          
          config->key_path = strdup(key_val->value.string);
          if (g_logger) {
            LOG_DEBUG("Config: Set SSL private key file to '%s'", config->key_path);
          }
        } else {
          if (g_logger) {
            LOG_WARNING("Invalid type for 'key_file' in SSL section (expected string, got %s)", 
                  json_type_name(key_val->type));
          }
        }
      } else {
        if (g_logger) {
          LOG_DEBUG("No SSL private key file specified, using default: %s", DEFAULT_SSL_KEY_PATH);
        }
      }
    } else {
      if (g_logger) {
        LOG_WARNING("'ssl' section is not an object (found %s), using defaults", 
              json_type_name(ssl_section->type));
      }
      
      config->use_ssl = DEFAULT_SSL_ENABLED;
      if (config->cert_path) free(config->cert_path);
      config->cert_path = strdup(DEFAULT_SSL_CERT_PATH);
      if (config->key_path) free(config->key_path);
      config->key_path = strdup(DEFAULT_SSL_KEY_PATH);
    }
  } else {
    if (g_logger) {
      LOG_INFO("No 'ssl' section found in config, using defaults.");
      LOG_DEBUG("Default SSL enabled: %d", config->use_ssl);
      LOG_DEBUG("Default SSL certificate: %s", config->cert_path);
      LOG_DEBUG("Default SSL private key: %s", config->key_path);
    }
  }
  
  /* Parse logging section */
  if (g_logger) {
    LOG_DEBUG("Processing 'logging' section.");
    TRACE_API("Looking for 'logging' object in configuration.");
  }
  
  json_value_t* logging_section = json_object_get(json, "logging");
  if (logging_section) {
    if (logging_section->type == JSON_OBJECT) {
      if (g_logger) {
        TRACE_API("Found 'logging' section with %zu properties", json_object_size(logging_section));
      }
      
      /* Process log level setting */
      if (g_logger) {
        TRACE_API("Looking for 'level' property in logging section.");
      }
      
      json_value_t* level_val = json_object_get(logging_section, "level");
      if (level_val) {
        if (level_val->type == JSON_STRING) {
          const char* level_str = level_val->value.string;
          int old_level = config->log_level;
          
          if (g_logger) {
            TRACE_API("Parsing log level string: '%s'", level_str);
          }
          
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
          } else {
            if (g_logger) {
              LOG_WARNING("Unknown log level '%s', using default: %d", 
                    level_str, DEFAULT_LOG_LEVEL);
            }
            config->log_level = DEFAULT_LOG_LEVEL;
          }
          
          if (g_logger) {
            LOG_DEBUG("Config: Set log_level from %d to %d", old_level, config->log_level);
            
            /* Special notices for various log levels */
            if (config->log_level == LOG_LEVEL_TRACE) {
              LOG_WARNING("Trace logging enabled - this will generate large log files "
                   "and may impact performance");
            } else if (config->log_level == LOG_LEVEL_NONE) {
              LOG_WARNING("Logging disabled - no operational visibility will be available.");
            }
          }
        } else {
          if (g_logger) {
            LOG_WARNING("Invalid type for 'level' in logging section (expected string, got %s), "
                 "using default: %d", 
                 json_type_name(level_val->type), DEFAULT_LOG_LEVEL);
          }
          config->log_level = DEFAULT_LOG_LEVEL;
        }
      } else {
        if (g_logger) {
          LOG_DEBUG("No log level specified, using default: %d", DEFAULT_LOG_LEVEL);
        }
        config->log_level = DEFAULT_LOG_LEVEL;
      }
      
      /* Process log file setting */
      if (g_logger) {
        TRACE_API("Looking for 'file' property in logging section.");
      }
      
      json_value_t* file_val = json_object_get(logging_section, "file");
      if (file_val) {
        if (file_val->type == JSON_STRING) {
          /* Free existing path if it exists */
          if (config->log_file) {
            if (g_logger) {
              TRACE_API("Freeing existing log_file: '%s'", config->log_file);
            }
            free(config->log_file);
          }
          
          /* Check if we need to create the directory */
          char* log_path = strdup(file_val->value.string);
          char* dir_end = strrchr(log_path, '/');
          if (dir_end) {
            *dir_end = '\0'; /* Temporarily terminate the string at the directory */
            
            /* Check if directory exists and create if needed */
            struct stat st = {0};
            if (stat(log_path, &st) == -1) {
              if (g_logger) {
                LOG_DEBUG("Log directory doesn't exist, will need to be created: %s", log_path);
              }
            }
            
            /* Restore the path */
            *dir_end = '/';
          }
          free(log_path);
          
          config->log_file = strdup(file_val->value.string);
          if (g_logger) {
            LOG_DEBUG("Config: Set log_file to '%s'", config->log_file);
          }
        } else {
          if (g_logger) {
            LOG_WARNING("Invalid type for 'file' in logging section (expected string, got %s)",
                  json_type_name(file_val->type));
          }
        }
      } else {
        if (g_logger) {
          LOG_DEBUG("No log file specified, using default: %s", DEFAULT_LOG_FILE);
          
          /* Resolve the default path */
          char* resolved_path = resolve_path(DEFAULT_LOG_FILE);
          if (resolved_path) {
            config->log_file = resolved_path;
            LOG_DEBUG("Resolved default log file path to: %s", config->log_file);
          } else {
            LOG_ERROR("Cannot resolve default log file path.");
            config->log_file = strdup(DEFAULT_LOG_FILE);
          }
        }
      }
    } else {
      if (g_logger) {
        LOG_WARNING("'logging' section is not an object (found %s), using defaults", 
              json_type_name(logging_section->type));
      }
      config->log_level = DEFAULT_LOG_LEVEL;
      
      /* Resolve the default path */
      char* resolved_path = resolve_path(DEFAULT_LOG_FILE);
      if (resolved_path) {
        config->log_file = resolved_path;
        if (g_logger) {
          LOG_DEBUG("Resolved default log file path to: %s", config->log_file);
        }
      } else {
        if (g_logger) {
          LOG_ERROR("Cannot resolve default log file path.");
        }
        config->log_file = strdup(DEFAULT_LOG_FILE);
      }
    }
  } else {
    if (g_logger) {
      LOG_INFO("No 'logging' section found in config, using defaults.");
      config->log_level = DEFAULT_LOG_LEVEL;
      
      /* Resolve the default path */
      char* resolved_path = resolve_path(DEFAULT_LOG_FILE);
      if (resolved_path) {
        config->log_file = resolved_path;
        LOG_DEBUG("Resolved default log file path to: %s", config->log_file);
      } else {
        LOG_ERROR("Cannot resolve default log file path.");
        config->log_file = strdup(DEFAULT_LOG_FILE);
      }
    }
  }
  
  /* Parse PID file setting */
  if (g_logger) {
    LOG_DEBUG("Processing 'pid_file' setting.");
    TRACE_API("Looking for 'pid_file' property in configuration.");
  }
  
  json_value_t* pid_file_val = json_object_get(json, "pid_file");
  if (pid_file_val) {
    if (pid_file_val->type == JSON_STRING) {
      /* Free existing path if it exists */
      if (config->pid_file) {
        if (g_logger) {
          TRACE_API("Freeing existing pid_file: '%s'", config->pid_file);
        }
        free(config->pid_file);
      }
      
      /* Check if we need to create the directory */
      char* pid_path = strdup(pid_file_val->value.string);
      char* dir_end = strrchr(pid_path, '/');
      if (dir_end) {
        *dir_end = '\0'; /* Temporarily terminate the string at the directory */
        
        /* Check if directory exists and create if needed */
        struct stat st = {0};
        if (stat(pid_path, &st) == -1) {
          if (g_logger) {
            LOG_DEBUG("PID directory doesn't exist, will need to be created: %s", pid_path);
          }
        }
        
        /* Restore the path */
        *dir_end = '/';
      }
      free(pid_path);
      
      config->pid_file = strdup(pid_file_val->value.string);
      if (g_logger) {
        LOG_DEBUG("Config: Set pid_file to '%s'", config->pid_file);
      }
    } else {
      if (g_logger) {
        LOG_WARNING("Invalid type for 'pid_file' (expected string, got %s), using default",
              json_type_name(pid_file_val->type));
      }
      
      /* Resolve the default path */
      char* resolved_path = resolve_path(DEFAULT_PID_FILE);
      if (resolved_path) {
        config->pid_file = resolved_path;
        if (g_logger) {
          LOG_DEBUG("Resolved default PID file path to: %s", config->pid_file);
        }
      } else {
        if (g_logger) {
          LOG_ERROR("Cannot resolve default PID file path.");
        }
        config->pid_file = strdup(DEFAULT_PID_FILE);
      }
    }
  } else {
    if (g_logger) {
      LOG_DEBUG("No PID file specified, using default: %s", DEFAULT_PID_FILE);
      
      /* Resolve the default path */
      char* resolved_path = resolve_path(DEFAULT_PID_FILE);
      if (resolved_path) {
        config->pid_file = resolved_path;
        LOG_DEBUG("Resolved default PID file path to: %s", config->pid_file);
      } else {
        LOG_ERROR("Cannot resolve default PID file path.");
        config->pid_file = strdup(DEFAULT_PID_FILE);
      }
    }
  }
  
  /* Parse verbose mode setting */
  if (g_logger) {
    LOG_DEBUG("Processing 'verbose_mode' setting.");
    TRACE_API("Looking for 'verbose_mode' property in configuration.");
  }
  
  json_value_t* foreground_val = json_object_get(json, "verbose_mode");
  if (foreground_val) {
    if (g_logger) {
      TRACE_API("Found 'verbose_mode' of type %s", json_type_name(foreground_val->type));
    }
    
    int old_mode = config->verbose_mode;
    
    if (foreground_val->type == JSON_BOOLEAN) {
      config->verbose_mode = foreground_val->value.boolean;
      if (g_logger) {
        TRACE_API("Parsing verbose_mode as boolean: %d", config->verbose_mode);
      }
    } else if (foreground_val->type == JSON_INTEGER) {
      config->verbose_mode = (foreground_val->value.integer != 0);
      if (g_logger) {
        TRACE_API("Parsing verbose_mode as integer: %ld -> %d", 
             foreground_val->value.integer, config->verbose_mode);
      }
    } else if (foreground_val->type == JSON_STRING) {
      config->verbose_mode = parse_bool(foreground_val->value.string);
      if (g_logger) {
        TRACE_API("Parsing verbose_mode as string: '%s' -> %d", 
             foreground_val->value.string, config->verbose_mode);
      }
    } else {
      if (g_logger) {
        LOG_WARNING("Invalid type for 'verbose_mode' (expected boolean, integer, or string, got %s), "
              "using default: %d", 
              json_type_name(foreground_val->type), DEFAULT_VERBOSE_MODE);
      }
      config->verbose_mode = DEFAULT_VERBOSE_MODE;
    }
    
    if (g_logger) {
      if (old_mode != config->verbose_mode) {
        LOG_DEBUG("Config: Changed verbose_mode from %d to %d", old_mode, config->verbose_mode);
        
        if (config->verbose_mode) {
          LOG_INFO("Server will run in verbose mode (not as daemon).");
        } else {
          LOG_INFO("Server will run in server mode (background).");
        }
      } else {
        LOG_DEBUG("Config: verbose_mode remains at %d", config->verbose_mode);
      }
    }
  } else {
    if (g_logger) {
      LOG_DEBUG("No verbose_mode specified, using default: %d", DEFAULT_VERBOSE_MODE);
      config->verbose_mode = DEFAULT_VERBOSE_MODE;
      
      if (config->verbose_mode) {
        LOG_INFO("Server will run in verbose mode (not as daemon).");
      } else {
        LOG_INFO("Server will run in server mode (background).");
      }
    }
  }
  
  /* Parse CORS settings */
  if (g_logger) {
    LOG_DEBUG("Processing 'cors' section.");
    TRACE_API("Looking for 'cors' object in configuration.");
  }
  
  json_value_t* cors_section = json_object_get(json, "cors");
  if (cors_section) {
    if (cors_section->type == JSON_OBJECT) {
      if (g_logger) {
        TRACE_API("Found 'cors' section with %zu properties", json_object_size(cors_section));
      }
      
      /* Initialize CORS configuration */
      if (g_logger) {
        TRACE_API("Initializing CORS configuration.");
      }
      init_cors_config(&config->cors);
      
      /* Process CORS enabled setting */
      if (g_logger) {
        TRACE_API("Looking for 'enabled' property in CORS section.");
      }
      
      json_value_t* enabled_val = json_object_get(cors_section, "enabled");
      if (enabled_val) {
        if (g_logger) {
          TRACE_API("Found 'enabled' of type %s", json_type_name(enabled_val->type));
        }
        
        int enabled = 0;
        if (enabled_val->type == JSON_BOOLEAN) {
          enabled = enabled_val->value.boolean;
          if (g_logger) {
            TRACE_API("Parsing CORS enabled as boolean: %d", enabled);
          }
        } else if (enabled_val->type == JSON_INTEGER) {
          enabled = (enabled_val->value.integer != 0);
          if (g_logger) {
            TRACE_API("Parsing CORS enabled as integer: %ld -> %d", 
                 enabled_val->value.integer, enabled);
          }
        } else if (enabled_val->type == JSON_STRING) {
          enabled = parse_bool(enabled_val->value.string);
          if (g_logger) {
            TRACE_API("Parsing CORS enabled as string: '%s' -> %d", 
                 enabled_val->value.string, enabled);
          }
        } else {
          if (g_logger) {
            LOG_WARNING("Invalid type for CORS 'enabled' (expected boolean, integer, or string, got %s), "
                  "using default: %d", 
                  json_type_name(enabled_val->type), DEFAULT_CORS_ENABLED);
          }
          enabled = DEFAULT_CORS_ENABLED;
        }
        
        config->cors.enabled = enabled;
        if (g_logger) {
          LOG_DEBUG("Config: Set CORS enabled to %d", config->cors.enabled);
          
          if (config->cors.enabled) {
            LOG_INFO("CORS support enabled.");
          } else {
            LOG_INFO("CORS support disabled.");
          }
        }
      } else {
        if (g_logger) {
          LOG_DEBUG("No CORS enabled setting specified, using default: %d", DEFAULT_CORS_ENABLED);
          config->cors.enabled = DEFAULT_CORS_ENABLED;
        }
      }
      
      /* Process allowed origins setting */
      if (g_logger) {
        TRACE_API("Looking for 'allowed_origins' property in CORS section.");
      }
      
      json_value_t* origins_val = json_object_get(cors_section, "allowed_origins");
      if (origins_val) {
        if (origins_val->type == JSON_ARRAY) {
          if (g_logger) {
            TRACE_API("Found 'allowed_origins' array with %zu items", 
                 json_array_size(origins_val));
          }
        
        /* Free existing origins if any */
        if (config->cors.allowed_origins) {
          if (g_logger) {
            TRACE_API("Freeing existing allowed origins (%d items)", 
                 config->cors.allowed_origins_count);
          }
          
          for (int i = 0; i < config->cors.allowed_origins_count; i++) {
            free(config->cors.allowed_origins[i]);
          }
          free(config->cors.allowed_origins);
          config->cors.allowed_origins = NULL;
          config->cors.allowed_origins_count = 0;
        }
        
        /* Allocate new origins array */
        size_t count = json_array_size(origins_val);
        if (g_logger) {
          TRACE_API("Allocating memory for %zu allowed origins", count);
        }
        
        /* Check for empty array */
        if (count == 0) {
          if (g_logger) {
            LOG_WARNING("Empty 'allowed_origins' array, CORS will not allow any origin.");
          }
          config->cors.allowed_origins = NULL;
          config->cors.allowed_origins_count = 0;
          
        } else {
          config->cors.allowed_origins = (char**)malloc(count * sizeof(char*));
          if (!config->cors.allowed_origins) {
            if (g_logger) {
              LOG_ERROR("Cannot allocate memory for CORS origins.");
            }
            /* Continue with other settings */
          } else {
            config->cors.allowed_origins_count = count;
            
            /* Copy origins */
            int wildcard_found = 0;
            for (size_t i = 0; i < count; i++) {
              json_value_t* origin = json_array_get(origins_val, i);
              if (origin && origin->type == JSON_STRING) {
                config->cors.allowed_origins[i] = strdup(origin->value.string);
                
                /* Check for wildcard origin */
                if (strcmp(origin->value.string, "*") == 0) {
                  wildcard_found = 1;
                  if (g_logger) {
                    LOG_WARNING("Wildcard (*) CORS origin can be a security risk.");
                  }
                }
                
                if (g_logger) {
                  LOG_DEBUG("Config: Added CORS allowed origin: '%s'", 
                       config->cors.allowed_origins[i]);
                }
              } else {
                if (g_logger) {
                  if (origin) {
                    LOG_WARNING("Invalid CORS origin at index %zu (expected string, got %s), "
                         "using wildcard '*'", 
                         i, json_type_name(origin->type));
                  } else {
                    LOG_WARNING("Invalid CORS origin at index %zu (NULL), using wildcard '*'", i);
                  }
                }
                config->cors.allowed_origins[i] = strdup("*");
                wildcard_found = 1;
              }
            }
            
            if (g_logger && wildcard_found) {
              LOG_INFO("CORS configured with wildcard origin - all origins will be allowed.");
            }
          }
        }
      } else {
        if (g_logger) {
          LOG_WARNING("Invalid type for 'allowed_origins' (expected array, got %s)", 
                json_type_name(origins_val->type));
        }
      }
      }
    } else {
      if (g_logger && config->cors.enabled) {
        LOG_WARNING("CORS is enabled but no allowed origins specified.");
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
    LOG_INFO("Configuration loaded successfully from %s", filepath);
  }
  
  return 1;
}

/* Load configuration from key=value file */
int config_load_keyvalue(const char* filepath, server_config_t* config) {
  if (g_logger) {
    TRACE_API("Entering config_load_keyvalue(filepath='%s')", filepath ? filepath : "NULL");
  }
  
  if (!filepath || !config) {
    if (g_logger) {
      LOG_ERROR("Invalid parameters for config_load_keyvalue: filepath=%p, config=%p", 
          (void*)filepath, (void*)config);
      TRACE_API("Exiting config_load_keyvalue() - invalid parameters.");
    } else {
      LOG_ERROR("Invalid parameters for config_load_keyvalue.");
    }
    return 0;
  }
  
  FILE* file = fopen(filepath, "r");
  if (!file) {
    if (g_logger) {
      LOG_ERROR("Could not open config file: %s", filepath);
    } else {
      LOG_ERROR("Could not open config file: %s", filepath);
    }
    return 0;
  }
  
  if (g_logger) {
    LOG_INFO("Loading configuration from key-value file: %s", filepath);
  } else {
    LOG_INFO("Loading configuration from key-value file: %s", filepath);
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
    } else if (strcasecmp(key, "jwt_secret") == 0) {
      config->jwt_secret = strdup(value);
      if (g_logger) {
        LOG_DEBUG("Config: Set JWT secret.");
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
    } else if (strcasecmp(key, "verbose_mode") == 0) {
      config->verbose_mode = parse_bool(value);
      if (g_logger) {
        LOG_DEBUG("Config: Set verbose_mode to %d", config->verbose_mode);
      }
    } else if (strcasecmp(key, "js_enabled") == 0) {
      config->js_enabled = parse_bool(value);
      if (g_logger) {
        LOG_DEBUG("Config: Set js_enabled to %d", config->js_enabled);
      }
    } else if (strcasecmp(key, "use_ssl") == 0 || strcasecmp(key, "ssl_enabled") == 0) {
      config->use_ssl = parse_bool(value);
      if (g_logger) {
        LOG_DEBUG("Config: Set use_ssl to %d", config->use_ssl);
      }
    } else if (strcasecmp(key, "cert_path") == 0 || strcasecmp(key, "ssl_cert") == 0) {
      if (config->cert_path) free(config->cert_path);
      config->cert_path = strdup(value);
      if (g_logger) {
        LOG_DEBUG("Config: Set cert_path to '%s'", config->cert_path);
      }
    } else if (strcasecmp(key, "key_path") == 0 || strcasecmp(key, "ssl_key") == 0) {
      if (config->key_path) free(config->key_path);
      config->key_path = strdup(value);
      if (g_logger) {
        LOG_DEBUG("Config: Set key_path to '%s'", config->key_path);
      }
    }
    /* CORS settings could be handled here but would require more complex parsing */
  }
  
  fclose(file);
  
  if (g_logger) {
    LOG_INFO("Configuration loaded successfully from %s", filepath);
    TRACE_API("Exiting config_load_keyvalue() - success.");
  } else {
    LOG_INFO("Configuration loaded successfully from %s", filepath);
  }
  
  return 1;
}

/* Detect config file type and load it */
int config_load(const char* filepath, server_config_t* config) {
  if (g_logger) {
    TRACE_API("Entering config_load(filepath='%s')", filepath ? filepath : "NULL");
  }
  
  if (!filepath || !config) {
    if (g_logger) {
      LOG_ERROR("Invalid parameters for config_load: filepath=%p, config=%p", 
          (void*)filepath, (void*)config);
      TRACE_API("Exiting config_load() - invalid parameters.");
    } else {
      LOG_ERROR("Invalid parameters for config_load.");
    }
    return 0;
  }
  
  /* Determine file extension */
  const char* ext = strrchr(filepath, '.');
  if (!ext) {
    /* Default to key=value if no extension */
    if (g_logger) {
      LOG_DEBUG("No file extension detected, assuming key=value format for '%s'", filepath);
    }
    return config_load_keyvalue(filepath, config);
  }
  
  if (strcasecmp(ext, ".json") == 0) {
    if (g_logger) {
      LOG_DEBUG("Detected .json extension, loading as JSON format.");
      TRACE_API("Exiting config_load() - delegating to config_load_json.");
    }
    return config_load_json(filepath, config);
  } else if (strcasecmp(ext, ".conf") == 0) {
    if (g_logger) {
      LOG_DEBUG("Detected .conf extension, loading as key=value format.");
      TRACE_API("Exiting config_load() - delegating to config_load_keyvalue.");
    }
    return config_load_keyvalue(filepath, config);
  } else {
    /* Unknown extension, try key=value format */
    if (g_logger) {
      LOG_WARNING("Unknown file extension '%s', assuming key=value format", ext);
      TRACE_API("Exiting config_load() - delegating to config_load_keyvalue.");
    }
    return config_load_keyvalue(filepath, config);
  }
}

/* Free configuration resources */
void config_free(server_config_t* config) {
  if (g_logger) {
    TRACE_API("Entering config_free(config=%p)", (void*)config);
  }
  
  if (!config) {
    if (g_logger) {
      TRACE_API("Exiting config_free() - NULL config.");
    }
    return;
  }
  
  /* Free string resources */
  if (config->host) free(config->host);
  if (config->db_path) free(config->db_path);
  /* JDBX is the only storage backend - no field to free */
  if (config->jwt_secret) free(config->jwt_secret);
  if (config->pid_file) free(config->pid_file);
  if (config->log_file) free(config->log_file);
  if (config->web_root) free(config->web_root);
  
  /* Free SSL resources */
  if (config->cert_path) free(config->cert_path);
  if (config->key_path) free(config->key_path);
  if (config->ssl_context) {
    ssl_context_free(config->ssl_context);
    config->ssl_context = NULL;
  }
  
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
  
  if (g_logger) {
    LOG_DEBUG("Configuration resources freed.");
    TRACE_API("Exiting config_free() - success.");
  }
}

/**
 * Initialize configuration with default values from config_defaults.h
 * @param config Pointer to the configuration structure to initialize
 */
void config_init_defaults(server_config_t* config) {
  if (g_logger) {
    TRACE_API("Entering config_init_defaults(config=%p)", (void*)config);
  }
  
  if (!config) {
    if (g_logger) {
      LOG_ERROR("NULL config provided to config_init_defaults().");
      TRACE_API("Exiting config_init_defaults() - NULL config.");
    }
    return;
  }
  
  /* Clear any existing configuration */
  config_free(config);
  
  /* Ensure binary directory is initialized for path resolution */
  config_init_binary_dir();
  
  /* Server settings */
  config->port = DEFAULT_PORT;
  config->host = strdup(DEFAULT_HOST);
  config->max_connections = DEFAULT_MAX_CONNECTIONS;
  
  /* Runtime settings */
  config->verbose_mode = 0; /* Default to non-verbose mode */
  config->log_level = DEFAULT_LOG_LEVEL;
  config->js_enabled = DEFAULT_JS_ENABLED;
  
  /* File paths (resolved relative to binary directory) */
  config->db_path = resolve_path(DEFAULT_DB_PATH);
  /* JDBX is the only storage backend - no need to set */
  config->pid_file = resolve_path(DEFAULT_PID_FILE);
  config->log_file = resolve_path(DEFAULT_LOG_FILE);
  config->web_root = resolve_path(DEFAULT_WEB_ROOT);
  
  /* Security settings */
  config->jwt_secret = strdup(DEFAULT_JWT_SECRET);
  
  /* Initialize CORS configuration */
  init_cors_config(&config->cors);
  
  /* Thread pool settings */
  config->thread_pool_min = DEFAULT_THREAD_POOL_MIN;
  config->thread_pool_max = DEFAULT_THREAD_POOL_MAX;
  config->thread_pool_queue_size = DEFAULT_THREAD_POOL_QUEUE_SIZE;
  config->thread_pool_idle_timeout = DEFAULT_THREAD_POOL_IDLE_TIMEOUT;

  /* SSL settings - MATCH EXACT STRUCT FIELD ORDER */
  config->use_ssl = DEFAULT_SSL_ENABLED;
  config->cert_path = strdup(DEFAULT_SSL_CERT_PATH);
  config->key_path = strdup(DEFAULT_SSL_KEY_PATH);
  config->ssl_context = NULL;

  /* Cache settings */
  config->cache_enabled = DEFAULT_CACHE_ENABLED;
  config->cache_max_size = DEFAULT_CACHE_SIZE;
  config->cache_ttl = DEFAULT_CACHE_TTL;

  /* Metrics settings */
  config->metrics_enabled = DEFAULT_METRICS_ENABLED;
  config->metrics_retention = DEFAULT_METRICS_RETENTION;

  /* Adaptive indexing settings */
  config->index_query_threshold = DEFAULT_INDEX_QUERY_THRESHOLD;
  config->index_time_threshold = DEFAULT_INDEX_TIME_THRESHOLD;
  config->index_query_threshold_system = DEFAULT_INDEX_QUERY_THRESHOLD_SYSTEM;
  config->index_time_threshold_system = DEFAULT_INDEX_TIME_THRESHOLD_SYSTEM;
  config->index_startup_delay = DEFAULT_INDEX_STARTUP_DELAY;
  config->index_check_interval = DEFAULT_INDEX_CHECK_INTERVAL;

  /* Additional settings */
  config->cors.enabled = DEFAULT_CORS_ENABLED;
  config->cors.allow_credentials = DEFAULT_CORS_ALLOW_CREDENTIALS;
  config->cors.max_age = DEFAULT_CORS_MAX_AGE;
  
  if (g_logger) {
    LOG_INFO("Configuration initialized with default values.");
    LOG_DEBUG("Default port: %d", config->port);
    LOG_DEBUG("Default host: %s", config->host);
    LOG_DEBUG("Default DB path: %s", config->db_path);
    LOG_DEBUG("Default PID file: %s", config->pid_file);
    LOG_DEBUG("Default log file: %s", config->log_file);
    LOG_DEBUG("Default web root: %s", config->web_root);
    
    /* Security-related defaults - only log at trace level */
    TRACE_API("Default JWT secret length: %zu", strlen(config->jwt_secret));
    TRACE_API("Default CORS enabled: %d", config->cors.enabled);
    TRACE_API("Default SSL enabled: %d", config->use_ssl);
    LOG_DEBUG("Default SSL certificate: %s", config->cert_path);
    LOG_DEBUG("Default SSL private key: %s", config->key_path);
    
    TRACE_API("Exiting config_init_defaults() - success.");
  }
}

/* Database configuration functions are now implemented in database_config.c */