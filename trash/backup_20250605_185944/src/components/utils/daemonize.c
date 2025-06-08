#include "utils/daemonize.h"
#include "utils/logger.h"
#include "init.h"
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
 * Helper macro for logging daemon messages
 * Uses debug level for detailed steps in debug mode,
 * but only logs important events at info level
 */
#define DAEMON_LOG(level, message, ...) \
  if (g_logger) { \
    LOG_##level("[DAEMON] " message, ##__VA_ARGS__); \
  } else { \
    fprintf(stderr, "[DAEMON] " message "\n", ##__VA_ARGS__); \
  }

/**
 * Helper macro for debug level daemon logging
 * Only logs when in debug mode (LOG_LEVEL_DEBUG or higher)
 */
#define DAEMON_DEBUG(message, ...) \
  if (g_logger && g_logger->log_level >= LOG_LEVEL_DEBUG) { \
    LOG_DEBUG("[DAEMON] " message, ##__VA_ARGS__); \
  } else if (!g_logger) { \
    fprintf(stderr, "[DAEMON:DEBUG] " message "\n", ##__VA_ARGS__); \
  }

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
  DAEMON_LOG(INFO, "Starting daemonization process");
  DAEMON_DEBUG("PID file path: %s", pid_file ? pid_file : "(none)");
  
  /* First fork - separate from current session */
  DAEMON_DEBUG("Performing first fork");
  pid_t pid = fork();
  
  if (pid < 0) {
    DAEMON_LOG(ERROR, "Failed to fork process (first fork): %s (errno=%d)", 
         strerror(errno), errno);
    return -1;
  }
  
  if (pid > 0) {
    /* Parent process from first fork - should exit */
    DAEMON_LOG(INFO, "Server initializing daemon process (PID: %d)", pid);
    return 1;
  }
  
  /* Child process continues - now we're in the intermediate process */
  DAEMON_DEBUG("First fork successful, now in intermediate process (PID: %d)", getpid());
  
  /* Mark this as the intermediate daemon child process */
  init_set_process_type(PROCESS_TYPE_DAEMON_CHILD);
  
  /* Create a new session with no controlling terminal */
  DAEMON_DEBUG("Creating new session with setsid()");
  if (setsid() < 0) {
    DAEMON_LOG(ERROR, "Failed to create new session: %s (errno=%d)", 
         strerror(errno), errno);
    return -1;
  }
  
  /* Record the current PID for logging */
  pid_t daemon_pid = getpid();
  DAEMON_DEBUG("Intermediate process PID is %d (after setsid)", daemon_pid);
  
  /* Second fork - fully detach from terminal */
  DAEMON_DEBUG("Performing second fork");
  pid = fork();
  
  if (pid < 0) {
    DAEMON_LOG(ERROR, "Failed to fork process (second fork): %s (errno=%d)", 
         strerror(errno), errno);
    return -1;
  }
  
  if (pid > 0) {
    /* Intermediate process exits */
    DAEMON_DEBUG("Second fork successful, intermediate process exiting");
    _exit(0); /* Use _exit to avoid flushing buffers or calling atexit handlers */
  }
  
  /* Final daemon process continues */
  daemon_pid = getpid();
  DAEMON_DEBUG("Final daemon process PID is %d", daemon_pid);
  
  /* Reset file mode creation mask to ensure proper permissions */
  DAEMON_DEBUG("Resetting file mode creation mask (umask)");
  umask(0);
  
  /* IMPORTANT: Do NOT change working directory to root - maintaining original directory */
  /* This is the key fix - comment out the chdir("/") call that was causing socket binding issues */
  DAEMON_DEBUG("Skipping chdir to / to maintain original working directory");
  
  /* Save original stdout for debugging */
  int original_stdout = dup(STDOUT_FILENO);
  
  /* Get current working directory for logging */
  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    DAEMON_DEBUG("Maintaining working directory: %s", cwd);
  }
  
  /* Write to logger if available before closing stdin/stdout/stderr */
  if (g_logger) {
    LOG_INFO("[DAEMON] Final daemon process (PID: %d) starting in directory: %s", 
        daemon_pid, cwd);
  }
  
  /* Close all standard file descriptors */
  DAEMON_DEBUG("Closing standard file descriptors");
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
  
  /* Redirect standard file descriptors to /dev/null */
  DAEMON_DEBUG("Redirecting standard file descriptors to /dev/null");
  int fd = open("/dev/null", O_RDWR);
  if (fd < 0) {
    /* Can't log to stderr anymore, but logger might still work */
    if (g_logger) {
      LOG_ERROR("[DAEMON] Failed to open /dev/null: %s (errno=%d)", 
          strerror(errno), errno);
    }
    return -1; /* Error opening /dev/null */
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
      fprintf(saved_stdout, "[DAEMON] Process successfully daemonized (PID: %d)\n", daemon_pid);
      fclose(saved_stdout);
    }
  }
  
  /* Write PID to file if provided */
  if (pid_file) {
    DAEMON_DEBUG("Writing PID file: %s", pid_file);
    FILE* pid_fp = fopen(pid_file, "w");
    if (pid_fp) {
      fprintf(pid_fp, "%d\n", getpid());
      fclose(pid_fp);
      DAEMON_LOG(INFO, "PID file written: %s (PID: %d)", pid_file, getpid());
    } else {
      DAEMON_LOG(ERROR, "Failed to write PID file '%s': %s (errno=%d)", 
           pid_file, strerror(errno), errno);
    }
  }
  
  DAEMON_LOG(INFO, "Process successfully daemonized (PID: %d)", daemon_pid);
  return 0;
}

/**
 * Remove PID file if it exists and contains the current PID
 * 
 * @param pid_file Path to PID file
 */
void cleanup_pid_file(const char* pid_file) {
  if (!pid_file) {
    DAEMON_DEBUG("No PID file specified for cleanup");
    return;
  }
  
  DAEMON_DEBUG("Checking PID file for cleanup: %s", pid_file);
  
  /* Check if file exists */
  if (access(pid_file, F_OK) != 0) {
    DAEMON_DEBUG("PID file does not exist: %s", pid_file);
    return;
  }
  
  /* Read PID from file */
  FILE* pid_fp = fopen(pid_file, "r");
  if (!pid_fp) {
    DAEMON_DEBUG("Failed to open PID file for reading: %s", pid_file);
    return;
  }
  
  pid_t file_pid;
  if (fscanf(pid_fp, "%d", &file_pid) != 1) {
    DAEMON_DEBUG("Failed to read valid PID from file: %s", pid_file);
    fclose(pid_fp);
    return;
  }
  
  fclose(pid_fp);
  
  /* Only remove if it contains our PID */
  if (file_pid == getpid()) {
    DAEMON_DEBUG("Removing PID file containing current process PID");
    if (unlink(pid_file) == 0) {
      DAEMON_LOG(INFO, "Removed PID file: %s", pid_file);
    } else {
      DAEMON_LOG(WARNING, "Failed to remove PID file '%s': %s (errno=%d)", 
           pid_file, strerror(errno), errno);
    }
  } else {
    DAEMON_DEBUG("PID file contains different PID (%d), not removing", file_pid);
  }
}