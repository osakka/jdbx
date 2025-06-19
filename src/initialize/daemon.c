#include "init.h"
#include "utils/daemonize.h"
#include "utils/ssl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>

/* Initialize daemon process if in daemon mode */
init_status_t init_daemon(server_config_t* config) {
  if (!config) {
    INIT_LOG_FAILURE("DAEMON", "NULL server configuration");
    return INIT_DAEMON_ERROR;
  }
  
  /* Check if running in verbose (foreground) mode */
  if (config->verbose_mode) {
    INIT_LOG_PROGRESS("DAEMON", "Running in foreground mode, daemon not initialized");
    
    /* In foreground mode, we're the server process */
    init_set_process_type(PROCESS_TYPE_SERVER);
    
    /* Add detailed logs in debug mode */
    if (g_logger && g_logger->log_level >= LOG_LEVEL_DEBUG) {
      char cwd[PATH_MAX];
      if (getcwd(cwd, sizeof(cwd)) != NULL) {
        LOG_DEBUG("[DAEMON] Verbose mode enabled, PID: %d, working directory: %s", 
            getpid(), cwd);
      }
    }
    
    /* Write PID file for foreground mode */
    if (config->pid_file) {
      INIT_LOG_PROGRESS("DAEMON", "Writing PID file for foreground mode: %s", config->pid_file);
      FILE* pid_fp = fopen(config->pid_file, "w");
      if (pid_fp) {
        fprintf(pid_fp, "%d\n", getpid());
        fclose(pid_fp);
        INIT_LOG_SUCCESS("DAEMON", "PID file written: %s (PID: %d)", config->pid_file, getpid());
      } else {
        INIT_LOG_FAILURE("DAEMON", "Failed to write PID file '%s': %s (errno=%d)", 
               config->pid_file, strerror(errno), errno);
        /* Non-fatal error, continue */
      }
    }
    
    return INIT_OK;
  }
  
  /* Running in daemon mode */
  INIT_LOG_PROGRESS("DAEMON", "Starting in daemon mode");
  
  /* Add detailed logs in debug mode */
  if (g_logger && g_logger->log_level >= LOG_LEVEL_DEBUG) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
      LOG_DEBUG("[DAEMON] Pre-daemonize state: PID=%d, working directory: %s", getpid(), cwd);
    }
    LOG_DEBUG("[DAEMON] PID file path: %s", config->pid_file ? config->pid_file : "(none).");
    LOG_DEBUG("[DAEMON] Logger file path: %s", config->log_file ? config->log_file : "(none).");
  }
  
  /* Mark this as the parent process that will fork the daemon */
  init_set_process_type(PROCESS_TYPE_DAEMON_PARENT);
  
  /* Use daemonize_process function - our proven approach from testing */
  int daemonize_result = daemonize_process(config->pid_file);
  
  if (daemonize_result < 0) {
    INIT_LOG_FAILURE("DAEMON", "Failed to daemonize process: %s (errno=%d)", 
            strerror(errno), errno);
    return INIT_DAEMON_ERROR;
  }
  
  if (daemonize_result > 0) {
    /* This is the parent process, it should exit without cleanup */
    INIT_LOG_PROGRESS("DAEMON", "Daemon started with PID %d", daemonize_result);
    
    /* Return special status code to indicate parent should exit */
    return INIT_DAEMON_PARENT_EXIT;
  }
  
  /* This is the final daemon process after double fork */
  init_set_process_type(PROCESS_TYPE_SERVER);
  
  /* Reinitialize SSL random number generator after fork */
  if (config->use_ssl) {
    ssl_reinit_after_fork();
    INIT_LOG_PROGRESS("DAEMON", "SSL random number generator reinitialized after fork");
  }
  
  /* This is the child (daemon) process */
  if (g_logger && g_logger->log_level >= LOG_LEVEL_DEBUG) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
      LOG_DEBUG("[DAEMON] After daemonization: PID=%d, working directory: %s", getpid(), cwd);
    }
  }
  
  /* Reinitialize logger if file logging was specified, otherwise stdio is closed */
  if (config->log_file && !config->verbose_mode) {
    /* Close and reopen the logger to work properly in daemon context */
    if (g_logger) {
      if (g_logger->log_level >= LOG_LEVEL_DEBUG) {
        LOG_DEBUG("[DAEMON] Reinitializing logger to work in daemon context.");
      }
      logger_close();
    }
    
    if (!logger_init(config->log_file, config->log_level)) {
      /* Can't log in this case - but also won't reach main process anymore */
      return INIT_LOGGER_ERROR;
    }
    
    if (g_logger && g_logger->log_level >= LOG_LEVEL_DEBUG) {
      LOG_DEBUG("[DAEMON] Logger reinitialized with file: %s", config->log_file);
    }
  }
  
  INIT_LOG_SUCCESS("DAEMON", "Daemon process initialized (PID: %d)", getpid());
  
  return INIT_OK;
}