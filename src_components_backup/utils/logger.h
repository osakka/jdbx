#ifndef JSONDB_LOGGER_H
#define JSONDB_LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>

/* Log levels */
typedef enum {
    LOG_LEVEL_NONE = 0,     /* No logging */
    LOG_LEVEL_ERROR = 1,    /* Critical errors */
    LOG_LEVEL_WARNING = 2,  /* Warnings */
    LOG_LEVEL_INFO = 3,     /* Informational messages */
    LOG_LEVEL_DEBUG = 4,    /* Debug messages */
    LOG_LEVEL_TRACE = 5     /* Detailed tracing */
} log_level_t;

/* Global logger configuration */
typedef struct {
    FILE* log_file;                 /* File handle for logging */
    log_level_t log_level;          /* Current log level */
    char log_file_path[256];        /* Path to log file */
    int include_timestamp;          /* Whether to include timestamp in logs */
    int include_level;              /* Whether to include level in logs */
    int include_source;             /* Whether to include source info in logs */
    pthread_mutex_t lock;           /* Lock for thread safety */
} logger_config_t;

/* Global logger instance */
extern logger_config_t* g_logger;

/* Initialize the logger */
int logger_init(const char* log_file_path, log_level_t level);

/* Set log level */
void logger_set_level(log_level_t level);

/* Free logger resources */
void logger_close();

/* Log a message with source information */
void logger_log(log_level_t level, const char* file, int line, 
                const char* function, const char* format, ...);

/* Convenience macros for logging */
#define LOG_ERROR(...)   logger_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_WARNING(...) logger_log(LOG_LEVEL_WARNING, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_INFO(...)    logger_log(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_DEBUG(...)   logger_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_TRACE(...)   logger_log(LOG_LEVEL_TRACE, __FILE__, __LINE__, __func__, __VA_ARGS__)

#endif /* JSONDB_LOGGER_H */