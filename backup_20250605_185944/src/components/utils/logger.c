#include "utils/logger.h"
#include <sys/stat.h>
#include <errno.h>
#include <libgen.h> /* For dirname() */
#include <unistd.h> /* For getpid() */
#include <sys/syscall.h> /* For gettid() */
#include <strings.h> /* For strcasecmp() */

/* Global logger instance */
logger_config_t* g_logger = NULL;

/* String representations of log levels */
static const char* log_level_strings[] = {
  "NONE",
  "ERROR",
  "WARNING",
  "INFO",
  "DEBUG",
  "TRACE"
};

/* String representations of trace categories */
static const struct {
  trace_category_t flag;
  const char* name;
} trace_categories[] = {
  {TRACE_DATABASE, "database"},
  {TRACE_RBAC, "rbac"},
  {TRACE_API, "api"},
  {TRACE_AUTH, "auth"},
  {TRACE_TRANSACTION, "transaction"},
  {TRACE_BINARY, "binary"},
  {TRACE_JAVASCRIPT, "javascript"},
  {TRACE_NETWORK, "network"},
  {TRACE_METRICS, "metrics"},
  {TRACE_MEMORY, "memory"},
  {TRACE_ALL, "all"}
};

/* Create directory if it doesn't exist */
static int ensure_directory_exists(const char* path) {
  char path_copy[256];
  strncpy(path_copy, path, sizeof(path_copy) - 1);
  path_copy[sizeof(path_copy) - 1] = '\0';
  
  char* dir = dirname(path_copy);
  struct stat st;
  
  if (stat(dir, &st) == 0) {
    return 1; /* Directory exists */
  }
  
  /* Create directory with read/write/execute permissions */
  if (mkdir(dir, 0755) == 0) {
    return 1; /* Directory created */
  }
  
  /* Try using system to create nested directories */
  char cmd[512];
  snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dir);
  int result = system(cmd);
  
  return (result == 0);
}

/* Initialize the logger */
int logger_init(const char* log_file_path, log_level_t level) {
  /* Free existing logger if it exists */
  if (g_logger) {
    logger_close();
  }
  
  /* Allocate memory for the logger */
  g_logger = (logger_config_t*)malloc(sizeof(logger_config_t));
  if (!g_logger) {
    fprintf(stderr, "Out of memory");
    return 0; /* Memory allocation failed */
  }
  
  /* Initialize mutex */
  if (pthread_mutex_init(&g_logger->lock, NULL) != 0) {
    fprintf(stderr, "Failed to initialize logger mutex\n");
    free(g_logger);
    g_logger = NULL;
    return 0;
  }
  
  /* Set default configuration */
  g_logger->log_level = level;
  g_logger->include_timestamp = 1;
  g_logger->include_level = 1;
  g_logger->include_source = 1;
  g_logger->include_process_info = 1;
  g_logger->trace_mask = TRACE_NONE;
  
  /* Check if we should use console logging (stdout/stderr) */
  if (log_file_path == NULL) {
    /* Use stdout for console mode */
    g_logger->log_file = stdout;
    g_logger->log_file_path[0] = '\0'; /* Empty path indicates console logging */
    
    /* Log initialization message */
    logger_log(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, 
         "Logger initialized with level %s (console mode)", log_level_strings[level]);
    
    return 1;
  }
  
  /* Store log file path */
  strncpy(g_logger->log_file_path, log_file_path, sizeof(g_logger->log_file_path) - 1);
  g_logger->log_file_path[sizeof(g_logger->log_file_path) - 1] = '\0';
  
  /* Ensure log directory exists */
  if (!ensure_directory_exists(log_file_path)) {
    fprintf(stderr, "Error: Failed to create log directory for %s: %s\n", 
        log_file_path, strerror(errno));
    pthread_mutex_destroy(&g_logger->lock);
    free(g_logger);
    g_logger = NULL;
    return 0;
  }
  
  /* Open log file */
  g_logger->log_file = fopen(log_file_path, "a");
  if (!g_logger->log_file) {
    fprintf(stderr, "Error: Failed to open log file %s: %s\n", 
        log_file_path, strerror(errno));
    pthread_mutex_destroy(&g_logger->lock);
    free(g_logger);
    g_logger = NULL;
    return 0;
  }
  
  /* Log initialization message */
  logger_log(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, 
       "Logger initialized with level %s", log_level_strings[level]);
  
  return 1;
}

/* Set log level */
void logger_set_level(log_level_t level) {
  if (g_logger) {
    pthread_mutex_lock(&g_logger->lock);
    log_level_t old_level = g_logger->log_level;
    g_logger->log_level = level;
    pthread_mutex_unlock(&g_logger->lock);
    
    logger_log(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, 
         "Log level changed from %s to %s", 
         log_level_strings[old_level], log_level_strings[level]);
  }
}

/* Set trace categories */
void logger_set_trace_mask(trace_category_t mask) {
  if (g_logger) {
    pthread_mutex_lock(&g_logger->lock);
    g_logger->trace_mask = mask;
    pthread_mutex_unlock(&g_logger->lock);
    
    logger_log(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, 
         "Trace mask updated to 0x%04x", mask);
  }
}

/* Get current log level */
log_level_t logger_get_level() {
  if (!g_logger) return LOG_LEVEL_NONE;
  
  pthread_mutex_lock(&g_logger->lock);
  log_level_t level = g_logger->log_level;
  pthread_mutex_unlock(&g_logger->lock);
  
  return level;
}

/* Get current trace mask */
trace_category_t logger_get_trace_mask() {
  if (!g_logger) return TRACE_NONE;
  
  pthread_mutex_lock(&g_logger->lock);
  trace_category_t mask = g_logger->trace_mask;
  pthread_mutex_unlock(&g_logger->lock);
  
  return mask;
}

/* Parse log level from string */
log_level_t logger_parse_level(const char* level_str) {
  if (!level_str) return LOG_LEVEL_INFO;
  
  for (size_t i = 0; i < sizeof(log_level_strings) / sizeof(log_level_strings[0]); i++) {
    if (strcasecmp(level_str, log_level_strings[i]) == 0) {
      return (log_level_t)i;
    }
  }
  return LOG_LEVEL_INFO; /* Default fallback */
}

/* Parse trace categories from string */
trace_category_t logger_parse_trace(const char* trace_str) {
  if (!trace_str) return TRACE_NONE;
  
  trace_category_t mask = TRACE_NONE;
  char* str_copy = strdup(trace_str);
  char* token = strtok(str_copy, ",|");
  
  while (token) {
    /* Trim whitespace */
    while (*token == ' ' || *token == '\t') token++;
    
    for (size_t i = 0; i < sizeof(trace_categories) / sizeof(trace_categories[0]); i++) {
      if (strcasecmp(token, trace_categories[i].name) == 0) {
        mask |= trace_categories[i].flag;
        break;
      }
    }
    token = strtok(NULL, ",|");
  }
  
  free(str_copy);
  return mask;
}

/* Get log level string */
const char* logger_level_string(log_level_t level) {
  if (level < sizeof(log_level_strings) / sizeof(log_level_strings[0])) {
    return log_level_strings[level];
  }
  return "UNKNOWN";
}

/* Check if trace category is enabled */
int logger_trace_enabled(trace_category_t category) {
  if (!g_logger) return 0;
  
  pthread_mutex_lock(&g_logger->lock);
  int enabled = (g_logger->log_level >= LOG_LEVEL_TRACE) && 
                ((g_logger->trace_mask & category) != 0);
  pthread_mutex_unlock(&g_logger->lock);
  
  return enabled;
}

/* Free logger resources */
void logger_close() {
  if (!g_logger) {
    return;
  }
  
  /* Log closure message if possible */
  if (g_logger->log_file) {
    logger_log(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, "Logger shutting down");
    
    /* Don't close stdout/stderr */
    if (g_logger->log_file != stdout && g_logger->log_file != stderr) {
      fclose(g_logger->log_file);
    }
    g_logger->log_file = NULL;
  }
  
  /* Destroy mutex and free memory */
  pthread_mutex_destroy(&g_logger->lock);
  free(g_logger);
  g_logger = NULL;
}

/* Extract filename from path */
static const char* get_filename(const char* path) {
  const char* filename = strrchr(path, '/');
  if (filename) {
    return filename + 1;
  }
  return path;
}

/* Log a message with source information */
void logger_log(log_level_t level, const char* file, int line, 
        const char* function, const char* format, ...) {
  /* Check if logger is initialized */
  if (!g_logger) {
    return;
  }
  
  /* Check if this message should be logged based on level */
  if (level > g_logger->log_level) {
    return;
  }
  
  pthread_mutex_lock(&g_logger->lock);
  
  /* Determine output stream for console mode */
  FILE* output = g_logger->log_file;
  
  /* In console mode, direct errors and warnings to stderr, others to stdout */
  if (g_logger->log_file_path[0] == '\0') { /* Console mode */
    if (level <= LOG_LEVEL_WARNING) { /* ERROR and WARNING go to stderr */
      output = stderr;
    } else {
      output = stdout;
    }
  }
  
  /* Format: timestamp [processid:threadid] [level] functionname.filename(without extension) line_num: short message */
  
  /* Add timestamp if enabled */
  if (g_logger->include_timestamp) {
    time_t now;
    struct tm* time_info;
    char timestamp[30];
    
    time(&now);
    time_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", time_info);
    
    fprintf(output, "%s ", timestamp);
  }
  
  /* Add process information if enabled */
  if (g_logger->include_process_info) {
    pid_t pid = getpid();
    long tid = syscall(SYS_gettid);
    fprintf(output, "[%d:%ld] ", pid, tid);
  }
  
  /* Add log level if enabled */
  if (g_logger->include_level) {
    fprintf(output, "[%s] ", log_level_strings[level]);
  }
  
  /* Add source information if enabled - functionname.filename line_num: */
  if (g_logger->include_source) {
    const char* filename = get_filename(file);
    /* Remove .c extension for cleaner output */
    char clean_filename[64];
    strncpy(clean_filename, filename, sizeof(clean_filename) - 1);
    clean_filename[sizeof(clean_filename) - 1] = '\0';
    char* ext = strrchr(clean_filename, '.');
    if (ext && strcmp(ext, ".c") == 0) {
      *ext = '\0';
    }
    fprintf(output, "%s.%s %d: ", function, clean_filename, line);
  }
  
  /* Format and write the actual log message */
  va_list args;
  va_start(args, format);
  vfprintf(output, format, args);
  va_end(args);
  
  /* Add newline if not already present */
  if (format[0] == '\0' || format[strlen(format) - 1] != '\n') {
    fprintf(output, "\n");
  }
  
  /* Flush to ensure log is written immediately */
  fflush(output);
  
  pthread_mutex_unlock(&g_logger->lock);
}

/* Log a trace message with category */
void logger_trace(trace_category_t category, const char* file, int line,
                  const char* function, const char* format, ...) {
  /* Check if logger is initialized and trace is enabled for this category */
  if (!g_logger || g_logger->log_level < LOG_LEVEL_TRACE || 
      (g_logger->trace_mask & category) == 0) {
    return;
  }
  
  pthread_mutex_lock(&g_logger->lock);
  
  /* Determine output stream */
  FILE* output = g_logger->log_file;
  if (g_logger->log_file_path[0] == '\0') { /* Console mode */
    output = stdout;
  }
  
  /* Add timestamp */
  if (g_logger->include_timestamp) {
    time_t now;
    struct tm* time_info;
    char timestamp[30];
    
    time(&now);
    time_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", time_info);
    
    fprintf(output, "[%s] ", timestamp);
  }
  
  /* Add trace level */
  fprintf(output, "[TRACE] ");
  
  /* Add process information */
  if (g_logger->include_process_info) {
    pid_t pid = getpid();
    long tid = syscall(SYS_gettid);
    fprintf(output, "[%d:%ld] ", pid, tid);
  }
  
  /* Add source information */
  if (g_logger->include_source) {
    const char* filename = get_filename(file);
    char clean_filename[64];
    strncpy(clean_filename, filename, sizeof(clean_filename) - 1);
    clean_filename[sizeof(clean_filename) - 1] = '\0';
    char* ext = strrchr(clean_filename, '.');
    if (ext && strcmp(ext, ".c") == 0) {
      *ext = '\0';
    }
    fprintf(output, "[%s:%d:%s] ", clean_filename, line, function);
  }
  
  /* Format and write the actual log message */
  va_list args;
  va_start(args, format);
  vfprintf(output, format, args);
  va_end(args);
  
  /* Add newline if not already present */
  if (format[0] == '\0' || format[strlen(format) - 1] != '\n') {
    fprintf(output, "\n");
  }
  
  /* Flush to ensure log is written immediately */
  fflush(output);
  
  pthread_mutex_unlock(&g_logger->lock);
}