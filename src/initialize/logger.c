#include "init.h"
#include "utils/logger.h"
#include "utils/config_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Initialize logger system */
init_status_t init_logger(server_config_t* config) {
  if (!config) {
    fprintf(stderr, "FAILURE: NULL server configuration\n");
    return INIT_LOGGER_ERROR;
  }
  
  /* Already initialized? */
  if (g_logger) {
    INIT_LOG_PROGRESS("LOGGER", "Logger already initialized, updating configuration");
    g_logger->log_level = config->log_level;
    return INIT_OK;
  }
  
  
  if (config->verbose_mode) {
    /* In foreground mode, direct logs to stdout/stderr */
    
    if (!logger_init(NULL, config->log_level)) {
      fprintf(stderr, "FAILURE: Failed to initialize console logger\n");
      return INIT_LOGGER_ERROR;
    }
    
    if (g_logger) {
      g_logger->include_timestamp = 1; /* Include timestamps in console output */
      g_logger->include_level = 1;   /* Include log level in console output */
      LOG_INFO("Console logger initialized with level %d", config->log_level);
    } else {
      fprintf(stderr, "FAILURE: Logger global variable not set\n");
      return INIT_LOGGER_ERROR;
    }
  } else if (config->log_file) {
    /* In daemon mode with specified log file */
    
    if (!logger_init(config->log_file, config->log_level)) {
      fprintf(stderr, "FAILURE: Failed to initialize file logger: %s\n",
          config->log_file);
      return INIT_LOGGER_ERROR;
    }
    
    if (g_logger) {
      LOG_INFO("File logger initialized: %s", config->log_file);
    } else {
      fprintf(stderr, "FAILURE: Logger global variable not set\n");
      return INIT_LOGGER_ERROR;
    }
  } else {
    /* In daemon mode without specified log file, use default */
    char* var_dir = config_get_var_dir();
    char* default_log_path = NULL;
    
    if (var_dir) {
      default_log_path = malloc(strlen(var_dir) + strlen(DEFAULT_LOG_FILE_BASENAME) + 2);
      if (default_log_path) {
        sprintf(default_log_path, "%s/%s", var_dir, DEFAULT_LOG_FILE_BASENAME);
      }
      free(var_dir);
    }
    
    if (!default_log_path) {
      default_log_path = strdup(DEFAULT_LOG_FILE_BASENAME);
    }
    
    if (!logger_init(default_log_path, config->log_level)) {
      fprintf(stderr, "FAILURE: Failed to initialize default logger: %s\n",
          default_log_path);
      free(default_log_path);
      return INIT_LOGGER_ERROR;
    }
    
    if (g_logger) {
      LOG_INFO("Default file logger initialized: %s", default_log_path);
    }
    
    free(default_log_path);
  }

  /* Now we can use the standardized logging macros */
  INIT_LOG_SUCCESS("LOGGER", "Logger initialized");

  return INIT_OK;
}