#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "core/server.h"

/**
 * @file environment.h
 * @brief Environment configuration and path handling
 */

/**
 * Ensure a path is absolute
 * 
 * If the path is already absolute, it is returned as-is.
 * If it's relative, it's converted to absolute based on base_dir.
 * If base_dir is NULL, current working directory is used.
 * If path is NULL, NULL is returned.
 * 
 * @param path Path to check/convert
 * @param base_dir Base directory for relative paths
 * @return Newly allocated absolute path (caller must free)
 */
char* ensure_absolute_path(const char* path, const char* base_dir);

/**
 * Normalize all paths in server configuration to absolute paths
 * 
 * @param config Server configuration to normalize
 * @param base_dir Base directory for relative paths
 * @return 1 on success, 0 on failure
 */
int normalize_config_paths(server_config_t* config, const char* base_dir);

/**
 * Load configuration from environment variables
 * 
 * @param config Server configuration to update
 * @return 1 on success, 0 on failure
 */
int load_environment_config(server_config_t* config);

#endif /* ENVIRONMENT_H */