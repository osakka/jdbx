#include "utils/daemonize.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

extern logger_config_t* g_logger;

/**
 * Daemonize the current process
 * 
 * This function will:
 * 1. Fork the process
 * 2. Create a new session
 * 3. Close standard file descriptors
 * 4. Redirect standard file descriptors to /dev/null
 * 5. Write PID file if provided
 * 
 * @param pid_file Path to PID file to write, or NULL to skip
 * @return 0 on success, -1 on failure, 1 if parent process (should exit)
 */
int daemonize_process(const char* pid_file) {
    /* First fork - separate from current session */
    pid_t pid = fork();
    
    if (pid < 0) {
        fprintf(stderr, "Failed to fork process (first fork): %s\n", strerror(errno));
        return -1;
    }
    
    if (pid > 0) {
        /* Parent process from first fork - should exit */
        printf("Server initializing daemon process (PID: %d)\n", pid);
        return 1;
    }
    
    /* Child process continues - now we're in the intermediate process */
    
    /* Create a new session with no controlling terminal */
    if (setsid() < 0) {
        fprintf(stderr, "Failed to create new session: %s\n", strerror(errno));
        return -1;
    }
    
    /* IMPORTANT: Record the current PID for logging */
    pid_t daemon_pid = getpid();
    printf("DAEMONIZE DEBUG: Intermediate process PID is %d (after setsid)\n", daemon_pid);
    fflush(stdout);
    
    /* Second fork - fully detach from terminal */
    pid = fork();
    
    if (pid < 0) {
        fprintf(stderr, "Failed to fork process (second fork): %s\n", strerror(errno));
        return -1;
    }
    
    if (pid > 0) {
        /* Intermediate process exits */
        _exit(0); /* Use _exit to avoid flushing buffers or calling atexit handlers */
    }
    
    /* Final daemon process continues */
    daemon_pid = getpid();
    printf("DAEMONIZE DEBUG: Final daemon process PID is %d\n", daemon_pid);
    fflush(stdout);
    
    /* Reset file mode creation mask to ensure proper permissions */
    umask(0);
    
    /* Change working directory to root to avoid keeping directories mounted */
    if (chdir("/") < 0) {
        fprintf(stderr, "Failed to change working directory: %s\n", strerror(errno));
        /* Not fatal, continue execution */
    }
    
    /* Save original stdout/stderr for debug messages */
    int original_stdout = dup(STDOUT_FILENO);
    int original_stderr = dup(STDERR_FILENO);
    
    /* Close all standard file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    /* Redirect standard file descriptors to /dev/null */
    int fd = open("/dev/null", O_RDWR);
    if (fd < 0) {
        if (original_stdout > 0) {
            FILE* saved_stdout = fdopen(original_stdout, "w");
            if (saved_stdout) {
                fprintf(saved_stdout, "DAEMONIZE ERROR: Failed to open /dev/null: %s\n", strerror(errno));
                fflush(saved_stdout);
                fclose(saved_stdout);
            }
        }
        return -1;  /* Error opening /dev/null */
    }
    
    /* Ensure stdin, stdout, stderr point to /dev/null */
    if (fd != STDIN_FILENO) {
        dup2(fd, STDIN_FILENO);
    }
    
    if (fd != STDOUT_FILENO) {
        dup2(fd, STDOUT_FILENO);
    }
    
    if (fd != STDERR_FILENO) {
        dup2(fd, STDERR_FILENO);
    }
    
    /* Close the original fd if it's not one of the standard descriptors */
    if (fd > STDERR_FILENO) {
        close(fd);
    }
    
    /* Log success to original stdout and then close it */
    if (original_stdout > 0) {
        FILE* saved_stdout = fdopen(original_stdout, "w");
        if (saved_stdout) {
            fprintf(saved_stdout, "DAEMONIZE DEBUG: Process successfully daemonized (PID: %d)\n", daemon_pid);
            fflush(saved_stdout);
            fclose(saved_stdout);
        }
    }
    
    /* Close original stderr if it was duplicated */
    if (original_stderr > 0) {
        close(original_stderr);
    }
    
    /* Write PID to file if provided */
    if (pid_file) {
        FILE* pid_fp = fopen(pid_file, "w");
        if (pid_fp) {
            fprintf(pid_fp, "%d\n", getpid());
            fclose(pid_fp);
        }
    }
    
    return 0;
}

/**
 * Remove PID file if it exists and contains the current PID
 * 
 * @param pid_file Path to PID file
 */
void cleanup_pid_file(const char* pid_file) {
    if (!pid_file) {
        return;
    }
    
    /* Check if file exists */
    if (access(pid_file, F_OK) != 0) {
        return;
    }
    
    /* Read PID from file */
    FILE* pid_fp = fopen(pid_file, "r");
    if (!pid_fp) {
        return;
    }
    
    pid_t file_pid;
    if (fscanf(pid_fp, "%d", &file_pid) != 1) {
        fclose(pid_fp);
        return;
    }
    
    fclose(pid_fp);
    
    /* Only remove if it contains our PID */
    if (file_pid == getpid()) {
        unlink(pid_file);
        
        if (g_logger) {
            LOG_INFO("Removed PID file: %s", pid_file);
        }
    }
}