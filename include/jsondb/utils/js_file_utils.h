#ifndef JSONDB_JS_FILE_UTILS_H
#define JSONDB_JS_FILE_UTILS_H

#include "utils/logger.h"
#include <stddef.h>

/* Maximum number of search paths to track for error reporting */
#define JS_FILE_MAX_SEARCH_PATHS 20

/* Find a JavaScript file by searching in common locations */
int js_file_find(const char* filename, char* resolved_path, size_t path_size);

/* Log detailed information about file not found error */
void js_file_log_not_found(const char* original_path, log_level_t level);

/* Set the cache file path */
void js_file_set_cache_path(const char* path);

/* Save cache to disk - automatically called on program exit */
void js_file_save_cache(void);

#endif /* JSONDB_JS_FILE_UTILS_H */