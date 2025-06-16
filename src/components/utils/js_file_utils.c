#include "utils/js_file_utils.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"

/* Ensure USE_QUICKJS is defined when JavaScript is enabled */
#if !defined(DISABLE_JS) && !defined(USE_QUICKJS)
#define USE_QUICKJS
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h> /* For PATH_MAX */
#include <time.h>  /* For caching timestamps */

/* strlcpy and strlcat implementations if not available in libc */
#if !defined(HAVE_STRLCPY) && !defined(__FreeBSD__) && !defined(__OpenBSD__) && !defined(__NetBSD__) && !defined(__APPLE__)
/**
 * strlcpy - Copy a %NUL terminated string into a sized buffer
 * @dest: Where to copy the string to
 * @src: Where to copy the string from
 * @size: Size of destination buffer
 *
 * Compatible with *BSD: the result is always a valid
 * NUL-terminated string that fits in the buffer (unless,
 * of course, the buffer size is zero). It does not pad
 * out the result like strncpy() does.
 *
 * Returns the length of the source string (regardless of
 * whether it fits in the destination buffer or not).
 */
static size_t strlcpy(char *dest, const char *src, size_t size)
{
  size_t ret = strlen(src);

  if (size) {
    size_t len = (ret >= size) ? size - 1 : ret;
    memcpy(dest, src, len);
    dest[len] = '\0';
  }

  return ret;
}

/**
 * strlcat - Append a %NUL terminated string into a sized buffer
 * @dest: Where to append the string to
 * @src: Where to copy the string from
 * @size: Size of destination buffer
 *
 * Compatible with *BSD: the result is always a valid
 * NUL-terminated string that fits in the buffer (unless,
 * of course, the buffer size is zero). It does not pad
 * out the result like strncat() does.
 *
 * Returns the total length of the source string plus the
 * initial length of the destination string (regardless of
 * whether all of the source string could be copied or not).
 */
static size_t __attribute__((unused)) strlcat(char *dest, const char *src, size_t size)
{
  size_t dsize = strlen(dest);
  size_t len = strlen(src);
  size_t ret = dsize + len;

  if (dsize < size) {
    size_t copy_len = size - dsize - 1;
    if (len < copy_len)
      copy_len = len;
    memcpy(dest + dsize, src, copy_len);
    dest[dsize + copy_len] = '\0';
  }

  return ret;
}
#endif

/* Global known search paths */
static char *search_paths[JS_FILE_MAX_SEARCH_PATHS];
static size_t num_search_paths = 0;

/* Cache of recently resolved paths */
typedef struct {
  char original_path[PATH_MAX];
  char resolved_path[PATH_MAX];
  time_t timestamp;
} js_file_cache_entry_t;

#define JS_FILE_CACHE_SIZE 128
static js_file_cache_entry_t file_cache[JS_FILE_CACHE_SIZE];
static size_t cache_used = 0;
static char cache_file_path[PATH_MAX] = "./js_file_cache.dat";
static int cache_modified = 0;

/* Add a path to the list of known search paths */
static void add_search_path(const char *path) {
  if (num_search_paths < JS_FILE_MAX_SEARCH_PATHS) {
    search_paths[num_search_paths] = BUFFER_STRDUP(path);
    num_search_paths++;
  }
}

/* Initialize search paths */
static void init_search_paths() {
  if (num_search_paths > 0) {
    return; /* Already initialized */
  }
  
  /* Add current directory */
  add_search_path(".");
  
  /* Add standard JS directories */
  add_search_path("./js");
  add_search_path("./src/js");
  add_search_path("./scripts");
  
  /* Add functions directory */
  add_search_path("./functions");
  add_search_path("./src/js/functions");
  
  /* Add validators and transformers */
  add_search_path("./validators");
  add_search_path("./src/js/validators");
  add_search_path("./transformers");
  add_search_path("./src/js/transformers");
  
  /* Check environment variable for additional paths */
  char *js_path = getenv("JDBX_JS_PATH");
  if (js_path) {
    /* Split by colon */
    char *path = strtok(js_path, ":");
    while (path) {
      add_search_path(path);
      path = strtok(NULL, ":");
    }
  }
}

/* Load the path cache from disk */
static void load_cache() {
  FILE *f = fopen(cache_file_path, "rb");
  if (!f) {
    return;
  }
  
  if (fread(&cache_used, sizeof(cache_used), 1, f) != 1) {
    fclose(f);
    return;
  }
  
  if (cache_used > JS_FILE_CACHE_SIZE) {
    cache_used = JS_FILE_CACHE_SIZE;
  }
  
  if (fread(file_cache, sizeof(js_file_cache_entry_t), cache_used, f) != cache_used) {
    cache_used = 0;
  }
  
  fclose(f);
}

/* Save the path cache to disk */
void js_file_save_cache() {
  if (!cache_modified) {
    return;
  }
  
  FILE *f = fopen(cache_file_path, "wb");
  if (!f) {
    LOG_WARNING("Cannot save JavaScript file path cache to %s: %s", 
        cache_file_path, strerror(errno));
    return;
  }
  
  if (fwrite(&cache_used, sizeof(cache_used), 1, f) != 1 ||
    fwrite(file_cache, sizeof(js_file_cache_entry_t), cache_used, f) != cache_used) {
    LOG_WARNING("Cannot write JavaScript file cache data.");
  }
  
  fclose(f);
  cache_modified = 0;
}

/* Add an entry to the path cache */
static void cache_add(const char *original, const char *resolved) {
  size_t idx;
  
  /* Check if we already have this entry */
  for (idx = 0; idx < cache_used; idx++) {
    if (strcmp(file_cache[idx].original_path, original) == 0) {
      /* Update existing entry */
      strlcpy(file_cache[idx].resolved_path, resolved, sizeof(file_cache[idx].resolved_path));
      file_cache[idx].timestamp = time(NULL);
      cache_modified = 1;
      return;
    }
  }
  
  /* Handle cache full */
  if (cache_used >= JS_FILE_CACHE_SIZE) {
    /* Find oldest entry to replace */
    time_t oldest = file_cache[0].timestamp;
    idx = 0;
    
    for (size_t i = 1; i < cache_used; i++) {
      if (file_cache[i].timestamp < oldest) {
        oldest = file_cache[i].timestamp;
        idx = i;
      }
    }
  } else {
    idx = cache_used++;
  }
  
  /* Add new entry */
  strlcpy(file_cache[idx].original_path, original, sizeof(file_cache[idx].original_path));
  strlcpy(file_cache[idx].resolved_path, resolved, sizeof(file_cache[idx].resolved_path));
  file_cache[idx].timestamp = time(NULL);
  cache_modified = 1;
}

/* Look up a path in the cache */
static int cache_lookup(const char *original, char *resolved, size_t resolved_size) {
  /* Initialize cache if needed */
  static int cache_initialized = 0;
  if (!cache_initialized) {
    load_cache();
    cache_initialized = 1;
  }
  
  for (size_t i = 0; i < cache_used; i++) {
    if (strcmp(file_cache[i].original_path, original) == 0) {
      strlcpy(resolved, file_cache[i].resolved_path, resolved_size);
      return 1;
    }
  }
  
  return 0;
}

/* Set the cache file path */
void js_file_set_cache_path(const char* path) {
  if (path) {
    strlcpy(cache_file_path, path, sizeof(cache_file_path));
  }
}

/**
 * Make a path absolute if it is not already
 * 
 * @param path The path to convert
 * @param resolved_path Buffer to store the resolved path
 * @param path_size Size of the resolved_path buffer
 * @return 1 on success, 0 on failure
 */
int make_path_absolute(const char* path, char* resolved_path, size_t path_size) {
  if (!path || !resolved_path || path_size == 0) {
    return 0;
  }
  
  /* If already absolute, just copy it */
  if (path[0] == '/') {
    strlcpy(resolved_path, path, path_size);
    return 1;
  }
  
  /* Get current working directory */
  char cwd[PATH_MAX];
  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    LOG_ERROR("get current working directory: %s", strerror(errno));
    return 0;
  }
  
  /* Build absolute path */
  snprintf(resolved_path, path_size, "%s/%s", cwd, path);
  return 1;
}

/* Find a JavaScript file by searching in common locations */
int js_file_find(const char* filename, char* resolved_path, size_t path_size) {
  if (!filename || !resolved_path || path_size == 0) {
    return 0;
  }
  
  /* Initialize search paths if needed */
  init_search_paths();
  
  /* Check if this is an absolute path */
  if (filename[0] == '/') {
    struct stat st;
    if (stat(filename, &st) == 0 && S_ISREG(st.st_mode)) {
      strlcpy(resolved_path, filename, path_size);
      return 1;
    }
    return 0;
  }
  
  /* Check cache first */
  if (cache_lookup(filename, resolved_path, path_size)) {
    /* Verify the file still exists */
    struct stat st;
    if (stat(resolved_path, &st) == 0 && S_ISREG(st.st_mode)) {
      return 1;
    }
  }
  
  /* Look in all known search paths */
  for (size_t i = 0; i < num_search_paths; i++) {
    char test_path[PATH_MAX];
    snprintf(test_path, sizeof(test_path), "%s/%s", search_paths[i], filename);
    
    struct stat st;
    if (stat(test_path, &st) == 0 && S_ISREG(st.st_mode)) {
      /* Found it! */
      strlcpy(resolved_path, test_path, path_size);
      
      /* Update cache */
      cache_add(filename, test_path);
      
      return 1;
    }
  }
  
  /* Not found */
  return 0;
}

/* Log detailed information about file not found error */
void js_file_log_not_found(const char* original_path, log_level_t level) {
  if (level == LOG_LEVEL_NONE) {
    level = LOG_LEVEL_ERROR;
  }
  
  logger_log(level, __FILE__, __LINE__, __func__, 
       "JavaScript file not found: %s", original_path);
  
  logger_log(level, __FILE__, __LINE__, __func__, 
       "Searched in the following locations:");
  
  for (size_t i = 0; i < num_search_paths; i++) {
    char test_path[PATH_MAX];
    snprintf(test_path, sizeof(test_path), "%s/%s", search_paths[i], original_path);
    
    logger_log(level, __FILE__, __LINE__, __func__, " - %s", test_path);
  }
}