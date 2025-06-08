#ifndef PATH_NORMALIZER_H
#define PATH_NORMALIZER_H

#include "core/server.h"

/**
 * @file path_normalizer.h
 * @brief Functions for normalizing paths to absolute
 */

/**
 * Normalize a path to be absolute
 * 
 * If the path is already absolute, it is returned as-is.
 * If it's relative, it's converted to absolute based on base_dir.
 * If base_dir is NULL, current working directory is used.
 * If path is NULL, NULL is returned.
 * 
 * @param path Path to normalize
 * @param base_dir Base directory for relative paths
 * @return Newly allocated absolute path (caller must free)
 */
char* normalize_path(const char* path, const char* base_dir);

/**
 * Normalize all paths in server configuration to absolute paths
 * 
 * @param config Server configuration to normalize
 * @param base_dir Base directory for relative paths
 * @return 1 on success, 0 on failure
 */
int normalize_config_paths(server_config_t* config, const char* base_dir);

/**
 * Load paths from environment variables
 * 
 * @param config Server configuration to update
 * @return 1 on success, 0 on failure
 */
int load_paths_from_env(server_config_t* config);

#endif /* PATH_NORMALIZER_H */