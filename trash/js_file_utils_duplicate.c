#include "utils/js_file_utils.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h> /* For PATH_MAX */
#include <time.h>  /* For caching timestamps */

/* Simple key-value entry for caching resolved paths */
typedef struct js_path_cache_entry {
  char original_path[PATH_MAX];
  char resolved_path[PATH_MAX];
  time_t timestamp;
  struct js_path_cache_entry* next;
} js_path_cache_entry_t;

/* Global cache */
static js_path_cache_entry_t* g_js_path_cache = NULL;

/* Cache expiration in seconds (24 hours by default) */
static const time_t CACHE_EXPIRATION = 24 * 60 * 60;

/* Path to the cache file - default to project root */
static char g_cache_file_path[PATH_MAX] = "var/data/jsondb/js_path_cache.json";

/* Common directories to search for JavaScript files */
static const char* common_js_dirs[] = {
  "functions",
  "validators",
  "transforms",
  "tests/js",
  "examples/js_extensions",
  NULL /* Null-terminated array */
};

/* Paths where the file was searched but not found */
static char search_paths[JS_FILE_MAX_SEARCH_PATHS][PATH_MAX];
static int search_path_count = 0;

/* Set the cache file path */
void js_file_set_cache_path(const char* path) {
  if (path) {
    strncpy(g_cache_file_path, path, sizeof(g_cache_file_path) - 1);
    g_cache_file_path[sizeof(g_cache_file_path) - 1] = '\0';

    if (g_logger) {
      LOG_INFO("JavaScript path resolution cache file set to: %s", g_cache_file_path);
    }
  }
}

/* Free the cache memory */
static void free_cache() {
  js_path_cache_entry_t* current = g_js_path_cache;
  js_path_cache_entry_t* next = NULL;

  while (current) {
    next = current->next;
    free(current);
    current = next;
  }

  g_js_path_cache = NULL;
}

/* Add or update an entry in the cache */
static void cache_add_or_update(const char* original_path, const char* resolved_path) {
  if (!original_path || !resolved_path) {
    return;
  }

  /* First check if the entry already exists */
  js_path_cache_entry_t* current = g_js_path_cache;
  while (current) {
    if (strcmp(current->original_path, original_path) == 0) {
      /* Update existing entry */
      strncpy(current->resolved_path, resolved_path, sizeof(current->resolved_path) - 1);
      current->resolved_path[sizeof(current->resolved_path) - 1] = '\0';
      current->timestamp = time(NULL);
      return;
    }
    current = current->next;
  }

  /* Create a new entry */
  js_path_cache_entry_t* new_entry = (js_path_cache_entry_t*)malloc(sizeof(js_path_cache_entry_t));
  if (!new_entry) {
    /* Memory allocation failed */
    if (g_logger) {
      LOG_ERROR("Out of memory");
    }
    return;
  }

  /* Initialize the new entry */
  strncpy(new_entry->original_path, original_path, sizeof(new_entry->original_path) - 1);
  new_entry->original_path[sizeof(new_entry->original_path) - 1] = '\0';

  strncpy(new_entry->resolved_path, resolved_path, sizeof(new_entry->resolved_path) - 1);
  new_entry->resolved_path[sizeof(new_entry->resolved_path) - 1] = '\0';

  new_entry->timestamp = time(NULL);
  new_entry->next = g_js_path_cache;
  g_js_path_cache = new_entry;
}

/* Find a cached path, returns NULL if not found or expired */
static const char* cache_find(const char* original_path) {
  if (!original_path) {
    return NULL;
  }

  /* Get current time for expiration check */
  time_t now = time(NULL);

  js_path_cache_entry_t* current = g_js_path_cache;
  while (current) {
    if (strcmp(current->original_path, original_path) == 0) {
      /* Check if entry is expired */
      if (now - current->timestamp > CACHE_EXPIRATION) {
        /* Entry is expired, remove it */
        if (g_logger) {
          LOG_DEBUG("JavaScript path cache entry expired: %s", original_path);
        }
        return NULL;
      }

      /* Check if the resolved file still exists */
      struct stat st;
      if (stat(current->resolved_path, &st) != 0 || !S_ISREG(st.st_mode)) {
        /* File no longer exists or is not a regular file */
        if (g_logger) {
          LOG_DEBUG("JavaScript path cache entry invalid (file no longer exists): %s", original_path);
        }
        return NULL;
      }

      /* Valid cache entry found */
      if (g_logger) {
        LOG_DEBUG("JavaScript path cache hit: %s -> %s", original_path, current->resolved_path);
      }
      return current->resolved_path;
    }
    current = current->next;
  }

  /* Not found in cache */
  return NULL;
}

/* Load the cache from file */
static void cache_load() {
  FILE* file = fopen(g_cache_file_path, "r");
  if (!file) {
    /* Cache file doesn't exist, that's okay */
    return;
  }

  char line[PATH_MAX * 2 + 64]; /* original_path|resolved_path|timestamp */
  char original_path[PATH_MAX];
  char resolved_path[PATH_MAX];
  time_t timestamp;

  /* First free any existing cache */
  free_cache();

  /* Parse each line in the format: original_path|resolved_path|timestamp */
  while (fgets(line, sizeof(line), file)) {
    /* Remove newline */
    size_t len = strlen(line);
    if (len > 0 && line[len-1] == '\n') {
      line[len-1] = '\0';
    }

    /* Parse line */
    if (sscanf(line, "%[^|]|%[^|]|%ld", original_path, resolved_path, &timestamp) == 3) {
      /* Create a new entry */
      js_path_cache_entry_t* entry = (js_path_cache_entry_t*)malloc(sizeof(js_path_cache_entry_t));
      if (!entry) {
        continue; /* Memory allocation failed */
      }

      /* Initialize the entry */
      strncpy(entry->original_path, original_path, sizeof(entry->original_path) - 1);
      entry->original_path[sizeof(entry->original_path) - 1] = '\0';

      strncpy(entry->resolved_path, resolved_path, sizeof(entry->resolved_path) - 1);
      entry->resolved_path[sizeof(entry->resolved_path) - 1] = '\0';

      entry->timestamp = timestamp;

      /* Add to the cache */
      entry->next = g_js_path_cache;
      g_js_path_cache = entry;
    }
  }

  fclose(file);

  if (g_logger) {
    /* Count entries */
    int count = 0;
    js_path_cache_entry_t* current = g_js_path_cache;
    while (current) {
      count++;
      current = current->next;
    }

    LOG_INFO("Loaded %d entries from JavaScript path cache: %s", count, g_cache_file_path);
  }
}

/* Save the cache to file */
static void cache_save() {
  /* Create directory if it doesn't exist */
  char dir_path[PATH_MAX];
  strncpy(dir_path, g_cache_file_path, sizeof(dir_path) - 1);
  dir_path[sizeof(dir_path) - 1] = '\0';

  /* Find the last slash to get directory path */
  char* last_slash = strrchr(dir_path, '/');
  if (last_slash) {
    *last_slash = '\0'; /* Truncate at the last slash */

    /* Create directory hierarchy */
    char tmp[PATH_MAX];
    char* p = dir_path;

    /* Skip leading slash if present */
    if (*p == '/') {
      p++;
    }

    while (*p) {
      /* Find next slash or end of string */
      char* next = strchr(p, '/');
      if (next) {
        *next = '\0';
      }

      /* Build path so far */
      if (*dir_path == '/') {
        /* Use strncat for safer concatenation */
        tmp[0] = '/';
        tmp[1] = '\0';
        strncat(tmp, dir_path, sizeof(tmp) - 2); /* -2 for '/' and null terminator */
        tmp[sizeof(tmp) - 1] = '\0';
      } else {
        /* For this case, we can safely use strncpy since we're just copying the path */
        strncpy(tmp, dir_path, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
      }

      /* Create directory */
      struct stat st;
      if (stat(tmp, &st) != 0) {
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
          if (g_logger) {
            LOG_ERROR("create directory for JavaScript path cache: %s", tmp);
          }
          return;
        }
      }

      /* Restore slash */
      if (next) {
        *next = '/';
        p = next + 1;
      } else {
        break;
      }
    }
  }

  /* Open file for writing */
  FILE* file = fopen(g_cache_file_path, "w");
  if (!file) {
    if (g_logger) {
      LOG_ERROR("open JavaScript path cache file for writing: %s", g_cache_file_path);
    }
    return;
  }

  /* Write each entry to the file */
  int count = 0;
  js_path_cache_entry_t* current = g_js_path_cache;
  time_t now = time(NULL);

  while (current) {
    /* Skip expired entries */
    if (now - current->timestamp <= CACHE_EXPIRATION) {
      /* Write in the format: original_path|resolved_path|timestamp */
      fprintf(file, "%s|%s|%ld\n", current->original_path, current->resolved_path, current->timestamp);
      count++;
    }
    current = current->next;
  }

  fclose(file);

  if (g_logger) {
    LOG_INFO("Saved %d entries to JavaScript path cache: %s", count, g_cache_file_path);
  }
}

/* Helper function to safely combine paths */
static int safe_path_join(char* dest, size_t dest_size, const char* first, const char* second, const char* third) {
  size_t required_len = strlen(first) + 1; /* +1 for null terminator */

  if (second) {
    required_len += strlen(second) + 1; /* +1 for separator */
  }

  if (third) {
    required_len += strlen(third) + 1; /* +1 for separator */
  }

  if (required_len > dest_size) {
    /* Path would be too long for buffer */
    return 0;
  }

  /* Safe to build path */
  dest[0] = '\0'; /* Start with empty string */
  strncat(dest, first, dest_size - 1);

  if (second) {
    strncat(dest, "/", dest_size - strlen(dest) - 1);
    strncat(dest, second, dest_size - strlen(dest) - 1);
  }

  if (third) {
    strncat(dest, "/", dest_size - strlen(dest) - 1);
    strncat(dest, third, dest_size - strlen(dest) - 1);
  }

  return 1;
}

/* Find a JavaScript file by searching in common locations */
int js_file_find(const char* filename, char* resolved_path, size_t path_size) {
  if (!filename || !resolved_path || path_size <= 0) {
    return 0;
  }

  /* Reset search path tracking */
  search_path_count = 0;
  memset(search_paths, 0, sizeof(search_paths));

  /* Static initialization flag for lazy loading the cache */
  static int cache_initialized = 0;

  /* Initialize cache first time this function is called */
  if (!cache_initialized) {
    cache_load();
    cache_initialized = 1;
  }

  /* First check the cache */
  const char* cached_path = cache_find(filename);
  if (cached_path) {
    /* Found in cache - verify the file still exists */
    struct stat st;
    if (stat(cached_path, &st) == 0 && S_ISREG(st.st_mode)) {
      /* Copy the resolved path */
      strncpy(resolved_path, cached_path, path_size - 1);
      resolved_path[path_size - 1] = '\0';

      if (g_logger) {
        LOG_DEBUG("Using cached JavaScript file path: %s -> %s", filename, cached_path);
      }

      return 1;
    } else {
      /* Cache entry is invalid - file no longer exists */
      if (g_logger) {
        LOG_DEBUG("Cached JavaScript file no longer exists: %s", cached_path);
      }
    }
  }

  /* First check if the file exists as-is (might be an absolute path) */
  struct stat st;
  if (stat(filename, &st) == 0 && S_ISREG(st.st_mode)) {
    /* File exists and is a regular file */
    strncpy(resolved_path, filename, path_size - 1);
    resolved_path[path_size - 1] = '\0';

    /* Add to cache */
    cache_add_or_update(filename, resolved_path);

    return 1;
  }

  /* Add original path to search history */
  if (search_path_count < JS_FILE_MAX_SEARCH_PATHS) {
    strncpy(search_paths[search_path_count], filename, PATH_MAX - 1);
    search_paths[search_path_count][PATH_MAX - 1] = '\0';
    search_path_count++;
  }

  /* Get current working directory for relative paths */
  char cwd[PATH_MAX];
  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    if (g_logger) {
      LOG_ERROR("get current working directory: %s", strerror(errno));
    }
    return 0;
  }

  /* Try with current working directory */
  char test_path[PATH_MAX];
  if (!safe_path_join(test_path, sizeof(test_path), cwd, filename, NULL)) {
    if (g_logger) {
      LOG_ERROR("Path too long for buffer: %s/%s", cwd, filename);
    }
    return 0;
  }

  if (stat(test_path, &st) == 0 && S_ISREG(st.st_mode)) {
    /* File found in current directory */
    strncpy(resolved_path, test_path, path_size - 1);
    resolved_path[path_size - 1] = '\0';

    /* Add to cache */
    cache_add_or_update(filename, resolved_path);

    return 1;
  }

  /* Add to search history */
  if (search_path_count < JS_FILE_MAX_SEARCH_PATHS) {
    strncpy(search_paths[search_path_count], test_path, PATH_MAX - 1);
    search_paths[search_path_count][PATH_MAX - 1] = '\0';
    search_path_count++;
  }

  /* Add .js extension if not already present */
  char filename_with_ext[PATH_MAX];
  strncpy(filename_with_ext, filename, PATH_MAX - 1);
  filename_with_ext[PATH_MAX - 1] = '\0';

  size_t name_len = strlen(filename_with_ext);
  if (name_len < 3 ||
    filename_with_ext[name_len - 3] != '.' ||
    filename_with_ext[name_len - 2] != 'j' ||
    filename_with_ext[name_len - 1] != 's') {
    /* Extension not present, append it */
    if (name_len + 3 < PATH_MAX) {
      strcat(filename_with_ext, ".js");
    }
  }

  /* Try with just the filename + extension */
  if (stat(filename_with_ext, &st) == 0 && S_ISREG(st.st_mode)) {
    /* File exists with extension */
    strncpy(resolved_path, filename_with_ext, path_size - 1);
    resolved_path[path_size - 1] = '\0';

    /* Add to cache */
    cache_add_or_update(filename, resolved_path);

    return 1;
  }

  /* Add to search history */
  if (search_path_count < JS_FILE_MAX_SEARCH_PATHS) {
    strncpy(search_paths[search_path_count], filename_with_ext, PATH_MAX - 1);
    search_paths[search_path_count][PATH_MAX - 1] = '\0';
    search_path_count++;
  }

  /* Try with current directory + extension */
  if (!safe_path_join(test_path, sizeof(test_path), cwd, filename_with_ext, NULL)) {
    if (g_logger) {
      LOG_ERROR("Path too long for buffer: %s/%s", cwd, filename_with_ext);
    }
    return 0;
  }
  if (stat(test_path, &st) == 0 && S_ISREG(st.st_mode)) {
    /* File found in current directory with extension */
    strncpy(resolved_path, test_path, path_size - 1);
    resolved_path[path_size - 1] = '\0';

    /* Add to cache */
    cache_add_or_update(filename, resolved_path);

    return 1;
  }

  /* Add to search history */
  if (search_path_count < JS_FILE_MAX_SEARCH_PATHS) {
    strncpy(search_paths[search_path_count], test_path, PATH_MAX - 1);
    search_paths[search_path_count][PATH_MAX - 1] = '\0';
    search_path_count++;
  }

  /* Try all common directories */
  for (int i = 0; common_js_dirs[i] != NULL; i++) {
    /* Try without extension using safe path join */
    if (safe_path_join(test_path, sizeof(test_path), cwd, common_js_dirs[i], filename)) {
      if (stat(test_path, &st) == 0 && S_ISREG(st.st_mode)) {
        /* File found in common directory */
        strncpy(resolved_path, test_path, path_size - 1);
        resolved_path[path_size - 1] = '\0';

        /* Add to cache */
        cache_add_or_update(filename, resolved_path);

        return 1;
      }

      /* Add to search history */
      if (search_path_count < JS_FILE_MAX_SEARCH_PATHS) {
        strncpy(search_paths[search_path_count], test_path, PATH_MAX - 1);
        search_paths[search_path_count][PATH_MAX - 1] = '\0';
        search_path_count++;
      }
    } else {
      if (g_logger) {
        LOG_WARNING("Path too long when searching for %s in %s/%s",
            filename, cwd, common_js_dirs[i]);
      } else {
        fprintf(stderr, "Warning: Path too long when searching for %s in %s/%s\n",
            filename, cwd, common_js_dirs[i]);
      }
    }

    /* Try with extension using safe path join */
    if (safe_path_join(test_path, sizeof(test_path), cwd, common_js_dirs[i], filename_with_ext)) {

      if (stat(test_path, &st) == 0 && S_ISREG(st.st_mode)) {
        /* File found in common directory with extension */
        strncpy(resolved_path, test_path, path_size - 1);
        resolved_path[path_size - 1] = '\0';

        /* Add to cache */
        cache_add_or_update(filename, resolved_path);

        return 1;
      }

      /* Add to search history */
      if (search_path_count < JS_FILE_MAX_SEARCH_PATHS) {
        strncpy(search_paths[search_path_count], test_path, PATH_MAX - 1);
        search_paths[search_path_count][PATH_MAX - 1] = '\0';
        search_path_count++;
      }
    } else {
      fprintf(stderr, "Warning: Path too long when searching for %s in %s/%s\n",
          filename_with_ext, cwd, common_js_dirs[i]);
    }
  }

  /* Try project-root-relative paths using make_path_absolute */
  extern void make_path_absolute(const char* rel_path, char* abs_path, size_t abs_path_size);

  /* Try without extension */
  make_path_absolute(filename, test_path, sizeof(test_path));
  if (stat(test_path, &st) == 0 && S_ISREG(st.st_mode)) {
    /* File found using make_path_absolute */
    strncpy(resolved_path, test_path, path_size - 1);
    resolved_path[path_size - 1] = '\0';

    /* Add to cache */
    cache_add_or_update(filename, resolved_path);

    return 1;
  }

  /* Add to search history */
  if (search_path_count < JS_FILE_MAX_SEARCH_PATHS) {
    strncpy(search_paths[search_path_count], test_path, PATH_MAX - 1);
    search_paths[search_path_count][PATH_MAX - 1] = '\0';
    search_path_count++;
  }

  /* Try with extension */
  make_path_absolute(filename_with_ext, test_path, sizeof(test_path));
  if (stat(test_path, &st) == 0 && S_ISREG(st.st_mode)) {
    /* File found using make_path_absolute with extension */
    strncpy(resolved_path, test_path, path_size - 1);
    resolved_path[path_size - 1] = '\0';

    /* Add to cache */
    cache_add_or_update(filename, resolved_path);

    return 1;
  }

  /* Add to search history */
  if (search_path_count < JS_FILE_MAX_SEARCH_PATHS) {
    strncpy(search_paths[search_path_count], test_path, PATH_MAX - 1);
    search_paths[search_path_count][PATH_MAX - 1] = '\0';
    search_path_count++;
  }

  /* If we get here, file not found in any location */
  return 0;
}

/* Save the cache to disk - can be called manually or via atexit */
void js_file_save_cache(void) {
  static int already_called = 0;

  /* Prevent double execution during shutdown */
  if (already_called) {
    return;
  }
  already_called = 1;

  /* Save cache to file */
  cache_save();
}

/* Log detailed information about file not found error */
void js_file_log_not_found(const char* original_path, log_level_t level) {
  if (!g_logger) {
    /* Fall back to stderr if logger not initialized */
    fprintf(stderr, "JavaScript file not found: %s\n", original_path);
    fprintf(stderr, "Searched in the following locations:\n");
    for (int i = 0; i < search_path_count; i++) {
      fprintf(stderr, " - %s\n", search_paths[i]);
    }
    return;
  }
  
  /* Log using the logger system */
  if (level <= g_logger->log_level) {
    char message[4096] = {0}; /* Larger buffer for detailed message */
    char* p = message;
    int remaining = sizeof(message) - 1;
    
    /* Check if there's enough space for the initial message */
    int n;
    if (strlen(original_path) + 50 <= (size_t)remaining) {
      /* Should be safe as we just checked the length requirement */
      n = snprintf(p, remaining, "JavaScript file not found: %s\nSearched in:\n", original_path);
    } else {
      /* Not enough space, truncate the path */
      n = snprintf(p, remaining, "JavaScript file not found: ...\nSearched in:\n");
    }
    if (n > 0 && n < remaining) {
      p += n;
      remaining -= n;
    }
    
    for (int i = 0; i < search_path_count && remaining > 0; i++) {
      /* Only print if we have enough space for at least part of the path */
      if (remaining > 10) { /* Need space for at least " - " + a few chars + "\n" + null terminator */
        /* Create a safer approach that doesn't rely on snprintf for possibly long strings */
        const char* prefix = " - ";
        size_t prefix_len = strlen(prefix);

        /* Copy the prefix first */
        if (prefix_len + 1 <= (size_t)remaining) { /* +1 for null terminator */
          memcpy(p, prefix, prefix_len);
          p += prefix_len;
          remaining -= prefix_len;

          /* Now copy as much of the search path as will fit, leaving room for newline and null */
          size_t path_len = strlen(search_paths[i]);
          size_t remaining_size = (remaining > 2) ? (size_t)(remaining - 2) : 0;
          size_t copy_len = (path_len < remaining_size) ? path_len : remaining_size;

          memcpy(p, search_paths[i], copy_len);
          p += copy_len;
          remaining -= copy_len;

          /* Add newline if room */
          if (remaining >= 2) { /* Room for newline and null */
            *p++ = '\n';
            remaining--;
          }

          /* Null terminate */
          *p = '\0';

          /* Set n to the length we wrote (to match snprintf behavior) */
          n = prefix_len + copy_len + (remaining >= 2 ? 1 : 0);
        } else {
          /* Not even room for the prefix */
          if (remaining >= 2) {
            *p++ = '.';
            *p = '\0';
            n = 1;
          } else {
            n = 0;
          }
          break;
        }
      } else {
        /* Not enough space left - manually copy without snprintf */
        if (remaining >= 5) { /* Room for "...\n" and null */
          memcpy(p, "...\n", 4);
          p += 4;
          *p = '\0';
          n = 4;
        } else {
          /* Not even room for ellipsis */
          *p = '\0';
          n = 0;
        }
        break;
      }
      if (n > 0 && n < remaining) {
        p += n;
        remaining -= n;
      }
    }
    
    /* Use appropriate log level */
    switch (level) {
      case LOG_LEVEL_ERROR:
        LOG_ERROR("%s", message);
        break;
      case LOG_LEVEL_WARNING:
        LOG_WARNING("%s", message);
        break;
      case LOG_LEVEL_INFO:
        LOG_INFO("%s", message);
        break;
      case LOG_LEVEL_DEBUG:
        LOG_DEBUG("%s", message);
        break;
      case LOG_LEVEL_TRACE:
        LOG_TRACE("%s", message);
        break;
      default:
        LOG_ERROR("%s", message);
        break;
    }
  }
}