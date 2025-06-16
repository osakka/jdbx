#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#include "core/server.h"
#include "utils/logger.h"

/**
 * @file config_loader.h
 * @brief Configuration loading and management for the JDBX server
 */

/* Global configuration structure */
extern server_config_t* g_server_config;

/**
 * Initialize binary directory path detection
 * Should be called early in the program's execution
 */
void config_init_binary_dir(void);

/**
 * Get the binary directory path
 * @return Path to the directory containing the executable
 */
const char* config_get_binary_dir(void);

/**
 * Get the auto-detected base path (parent of binary directory)
 * @return Path to the installation base directory
 */
const char* config_get_base_path(void);

/**
 * Construct full path from base path and relative components
 * @param relative_path Relative path components (e.g., "build/var/jdbx")
 * @return Dynamically allocated full path (caller must free)
 */
char* config_construct_path(const char* relative_path);

/**
 * Get the var directory path (auto-detected or from environment)
 * @return Dynamically allocated var directory path (caller must free)
 */
char* config_get_var_dir(void);

/**
 * Get the web root directory path (auto-detected or from environment)
 * @return Dynamically allocated web root path (caller must free)
 */
char* config_get_web_root(void);

/**
 * Load secure bootstrap admin credentials from environment variables
 * 
 * CRITICAL SECURITY: Bootstrap admin credentials MUST be configured via
 * environment variables for production deployments.
 * 
 * Required Environment Variables:
 * - JDBX_BOOTSTRAP_ADMIN_USER: Initial admin username
 * - JDBX_BOOTSTRAP_ADMIN_PASS: Initial admin password (minimum 12 characters)
 * - JDBX_DEFAULT_ADMIN_EMAIL: Admin email address
 * 
 * @param admin_user Output parameter for admin username (caller must free with BUFFER_FREE)
 * @param admin_pass Output parameter for admin password (caller must free with BUFFER_FREE)
 * @param admin_email Output parameter for admin email (caller must free with BUFFER_FREE)
 * @return 0 on success with secure credentials, -1 on insecure fallback
 */
int config_load_bootstrap_admin_credentials(char** admin_user, char** admin_pass, char** admin_email);

/**
 * Load configuration from file (auto-detects format)
 * @param filepath Path to the configuration file
 * @param config Pointer to the configuration structure
 * @return 1 on success, 0 on failure
 */
int config_load(const char* filepath, server_config_t* config);

/**
 * Load configuration from JSON file
 * @param filepath Path to the JSON configuration file
 * @param config Pointer to the configuration structure
 * @return 1 on success, 0 on failure
 */
int config_load_json(const char* filepath, server_config_t* config);

/**
 * Load configuration from key=value file
 * @param filepath Path to the key=value configuration file
 * @param config Pointer to the configuration structure
 * @return 1 on success, 0 on failure
 */
int config_load_keyvalue(const char* filepath, server_config_t* config);

/**
 * Free configuration resources
 * @param config Pointer to the configuration structure
 */
void config_free(server_config_t* config);

/**
 * Initialize configuration with default values
 * Uses centralized defaults from config_defaults.h
 * @param config Pointer to the configuration structure
 */
void config_init_defaults(server_config_t* config);

/* Forward declarations for database types */
typedef struct database database_t;
typedef struct json_value json_value_t;

/**
 * Load configuration from database
 * @param db Pointer to the database
 * @return JSON configuration object (caller must free) or NULL
 */
json_value_t* config_load_from_database(database_t* db);

/**
 * Save configuration to database
 * @param db Pointer to the database
 * @param config JSON configuration object
 * @return 0 on success, -1 on failure
 */
int config_save_to_database(database_t* db, json_value_t* config);

/**
 * Apply database configuration settings to server config
 * @param config Server configuration structure
 * @param db Database to load configuration from
 * @return 0 on success, -1 on failure
 */
int config_apply_database_settings(server_config_t* config, database_t* db);

/**
 * Convert server configuration to JSON
 * @param config Server configuration structure
 * @return JSON representation (caller must free)
 */
json_value_t* config_to_json(server_config_t* config);

/**
 * Register configuration change callback
 * @param callback Function to call on configuration changes
 * @param user_data User data to pass to callback
 * @return 0 on success, -1 on failure
 */
int config_register_callback(void (*callback)(const char*, json_value_t*, json_value_t*), void* user_data);

/**
 * Cleanup configuration subsystem
 */
void config_cleanup(void);

#endif /* CONFIG_LOADER_H */