#ifndef DAEMONIZE_H
#define DAEMONIZE_H

/**
 * @file daemonize.h
 * @brief Functions for daemonizing the server process
 */

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
int daemonize_process(const char* pid_file);

/**
 * Remove PID file if it exists and contains the current PID
 * 
 * @param pid_file Path to PID file
 */
void cleanup_pid_file(const char* pid_file);

#endif /* DAEMONIZE_H */