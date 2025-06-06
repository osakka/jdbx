#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#include "core/server.h"
#include "utils/logger.h"

/**
 * @file config_loader.h
 * @brief Configuration loading and management for the JSONdb server
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

/**
 * Load configuration overrides from database
 * This provides the highest priority configuration (after defaults, env, flags)
 * @param config Pointer to the configuration structure
 * @return 1 on success, 0 on failure (non-fatal - database config is optional)
 */
int config_load_from_database(server_config_t* config);

/**
 * Save configuration changes to database
 * @param config Pointer to the configuration structure  
 * @return 1 on success, 0 on failure
 */
int config_save_to_database(const server_config_t* config);

#endif /* CONFIG_LOADER_H */