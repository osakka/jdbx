#ifndef JDBX_LOGGER_H
#define JDBX_LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>

/* Log levels */
typedef enum {
    LOG_LEVEL_NONE = 0,     /* No logging */
    LOG_LEVEL_ERROR = 1,    /* Critical errors that prevent operation */
    LOG_LEVEL_WARNING = 2,  /* Important issues needing attention */
    LOG_LEVEL_INFO = 3,     /* Key operational events (default production) */
    LOG_LEVEL_DEBUG = 4,    /* Detailed troubleshooting information */
    LOG_LEVEL_TRACE = 5     /* Very detailed execution flow */
} log_level_t;

/* Trace functionality categories */
typedef enum {
    TRACE_NONE = 0,
    TRACE_DATABASE = 1,
    TRACE_RBAC = 2,
    TRACE_API = 4,
    TRACE_AUTH = 8,
    TRACE_TRANSACTION = 16,
    TRACE_BINARY = 32,
    TRACE_JAVASCRIPT = 64,
    TRACE_NETWORK = 128,
    TRACE_METRICS = 256,
    TRACE_MEMORY = 512,
    TRACE_ALL = 0xFFFF
} trace_category_t;

/* Global logger configuration */
typedef struct {
    FILE* log_file;                 /* File handle for logging */
    log_level_t log_level;          /* Current log level */
    char log_file_path[256];        /* Path to log file */
    int include_timestamp;          /* Whether to include timestamp in logs */
    int include_level;              /* Whether to include level in logs */
    int include_source;             /* Whether to include source info in logs */
    int include_process_info;       /* Whether to include PID:TID */
    trace_category_t trace_mask;    /* Enabled trace categories */
    pthread_mutex_t lock;           /* Lock for thread safety */
} logger_config_t;

/* Global logger instance */
extern logger_config_t* g_logger;

/* Initialize the logger */
int logger_init(const char* log_file_path, log_level_t level);

/* Set log level */
void logger_set_level(log_level_t level);

/* Set trace categories */
void logger_set_trace_mask(trace_category_t mask);

/* Get current log level */
log_level_t logger_get_level();

/* Get current trace mask */
trace_category_t logger_get_trace_mask();

/* Parse log level from string */
log_level_t logger_parse_level(const char* level_str);

/* Parse trace categories from string */
trace_category_t logger_parse_trace(const char* trace_str);

/* Get log level string */
const char* logger_level_string(log_level_t level);

/* Check if trace category is enabled */
int logger_trace_enabled(trace_category_t category);

/* Free logger resources */
void logger_close();

/* Log a message with source information */
void logger_log(log_level_t level, const char* file, int line, 
                const char* function, const char* format, ...);

/* Log a trace message with category */
void logger_trace(trace_category_t category, const char* file, int line,
                  const char* function, const char* format, ...);

/* Convenience macros for logging */
#define LOG_ERROR(...)   logger_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_WARNING(...) logger_log(LOG_LEVEL_WARNING, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_INFO(...)    logger_log(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_DEBUG(...)   logger_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_TRACE(...)   logger_log(LOG_LEVEL_TRACE, __FILE__, __LINE__, __func__, __VA_ARGS__)

/* Trace macros by functionality - only active when category enabled */
#define TRACE_DB(...)       logger_trace(TRACE_DATABASE, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define TRACE_RBAC(...)     logger_trace(TRACE_RBAC, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define TRACE_API(...)      logger_trace(TRACE_API, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define TRACE_AUTH(...)     logger_trace(TRACE_AUTH, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define TRACE_TXN(...)      logger_trace(TRACE_TRANSACTION, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define TRACE_BIN(...)      logger_trace(TRACE_BINARY, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define TRACE_JS(...)       logger_trace(TRACE_JAVASCRIPT, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define TRACE_NET(...)      logger_trace(TRACE_NETWORK, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define TRACE_METRICS(...)  logger_trace(TRACE_METRICS, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define TRACE_MEMORY(...)   logger_trace(TRACE_MEMORY, __FILE__, __LINE__, __func__, __VA_ARGS__)

#endif /* JDBX_LOGGER_H */