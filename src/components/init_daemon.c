#include "init.h"
#include "utils/daemonize.h"
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
        
        /* Write PID file for foreground mode */
        if (config->pid_file) {
            INIT_LOG_PROGRESS("DAEMON", "Writing PID file for foreground mode: %s", config->pid_file);
            FILE* pid_fp = fopen(config->pid_file, "w");
            if (pid_fp) {
                fprintf(pid_fp, "%d\n", getpid());
                fclose(pid_fp);
                INIT_LOG_SUCCESS("DAEMON", "PID file written: %s (PID: %d)", config->pid_file, getpid());
            } else {
                INIT_LOG_FAILURE("DAEMON", "Failed to write PID file '%s': %s", 
                             config->pid_file, strerror(errno));
                /* Non-fatal error, continue */
            }
        }
        
        return INIT_OK;
    }
    
    /* Running in daemon mode */
    INIT_LOG_PROGRESS("DAEMON", "Starting in daemon mode");
    
    /* Use daemonize_process function - our proven approach from testing */
    int daemonize_result = daemonize_process(config->pid_file);
    
    if (daemonize_result < 0) {
        INIT_LOG_FAILURE("DAEMON", "Failed to daemonize process: %s", strerror(errno));
        return INIT_DAEMON_ERROR;
    }
    
    if (daemonize_result > 0) {
        /* This is the parent process, it should exit without cleanup */
        INIT_LOG_PROGRESS("DAEMON", "Daemon started with PID %d", daemonize_result);
        
        /* Return special status code to indicate parent should exit */
        return INIT_DAEMON_PARENT_EXIT;
    }
    
    /* This is the child (daemon) process */
    
    /* Reinitialize logger if file logging was specified, otherwise stdio is closed */
    if (config->log_file && !config->verbose_mode) {
        /* Close and reopen the logger to work properly in daemon context */
        if (g_logger) {
            logger_close();
        }
        
        if (!logger_init(config->log_file, config->log_level)) {
            /* Can't log in this case - but also won't reach main process anymore */
            return INIT_LOGGER_ERROR;
        }
    }
    
    INIT_LOG_SUCCESS("DAEMON", "Daemon process initialized successfully (PID: %d)", getpid());
    
    return INIT_OK;
}