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
    /* Fork the process */
    pid_t pid = fork();
    
    if (pid < 0) {
        fprintf(stderr, "Failed to fork process: %s\n", strerror(errno));
        return -1;
    }
    
    if (pid > 0) {
        /* Parent process - should exit */
        printf("Server started in background (PID: %d)\n", pid);
        return 1;
    }
    
    /* Child process continues */
    
    /* Create a new session */
    if (setsid() < 0) {
        fprintf(stderr, "Failed to create new session: %s\n", strerror(errno));
        return -1;
    }
    
    /* Close standard file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    /* Redirect standard file descriptors to /dev/null */
    int fd = open("/dev/null", O_RDWR);
    if (fd < 0) {
        return -1;  /* Cannot log error as stdout/stderr are closed */
    }
    
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    
    if (fd > STDERR_FILENO) {
        close(fd);
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