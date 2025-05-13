#include "utils/logger.h"
#include <sys/stat.h>
#include <errno.h>
#include <libgen.h> /* For dirname() */

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
        return 1; /* Directory created successfully */
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
        fprintf(stderr, "Failed to allocate memory for logger\n");
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
    if (g_logger->log_file_path[0] == '\0') {  /* Console mode */
        if (level <= LOG_LEVEL_WARNING) {  /* ERROR and WARNING go to stderr */
            output = stderr;
        } else {
            output = stdout;
        }
    }
    
    /* Add timestamp if enabled */
    if (g_logger->include_timestamp) {
        time_t now;
        struct tm* time_info;
        char timestamp[30];
        
        time(&now);
        time_info = localtime(&now);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", time_info);
        
        fprintf(output, "[%s] ", timestamp);
    }
    
    /* Add log level if enabled */
    if (g_logger->include_level) {
        fprintf(output, "[%s] ", log_level_strings[level]);
    }
    
    /* Add source information if enabled */
    if (g_logger->include_source) {
        fprintf(output, "[%s:%d:%s] ", 
                get_filename(file), line, function);
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