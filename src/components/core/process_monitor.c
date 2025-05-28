#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <dirent.h>

/* Simple process monitor for debugging server process issues */

#define MAX_PATH_LEN 256

/* Process state structure */
typedef struct {
  pid_t pid;
  time_t start_time;
  time_t last_update;
  char status[64];
  char executable[MAX_PATH_LEN];
  int socket_fd;
  int port;
  int has_bound;
  int is_listening;
  int is_daemon;
  char error_message[256];
} process_monitor_t;

/* Global variables */
static process_monitor_t g_process = {0};
static int g_log_fd = -1;
static char g_log_path[MAX_PATH_LEN] = "/tmp/process_monitor.log";
static int g_enabled = 0;

/* Initialize process monitor */
int process_monitor_init(const char* log_path) {
  /* Clear the monitor */
  memset(&g_process, 0, sizeof(g_process));
  g_process.pid = getpid();
  g_process.start_time = time(NULL);
  g_process.last_update = g_process.start_time;
  g_process.socket_fd = -1;
  g_process.port = -1;
  g_process.has_bound = 0;
  g_process.is_listening = 0;
  
  /* Get the executable path */
  char proc_path[64];
  snprintf(proc_path, sizeof(proc_path), "/proc/%d/exe", g_process.pid);
  
  ssize_t len = readlink(proc_path, g_process.executable, sizeof(g_process.executable) - 1);
  if (len > 0) {
    g_process.executable[len] = '\0';
  } else {
    strcpy(g_process.executable, "unknown");
  }
  
  /* Set the process status */
  strcpy(g_process.status, "initializing");
  
  /* Set log path if provided */
  if (log_path) {
    strncpy(g_log_path, log_path, sizeof(g_log_path) - 1);
  }
  
  /* Open debug log file */
  g_log_fd = open(g_log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (g_log_fd < 0) {
    fprintf(stderr, "Error: Failed to open process monitor log: %s\n", strerror(errno));
    return 0;
  }
  
  /* Write initial log entry */
  char log_msg[512];
  snprintf(log_msg, sizeof(log_msg),
      "[%lu] Process monitor initialized for PID %d (executable: %s)\n",
      (unsigned long)g_process.start_time, g_process.pid, g_process.executable);
  write(g_log_fd, log_msg, strlen(log_msg));
  
  g_enabled = 1;
  return 1;
}

/* Log a message to the process monitor log */
void process_monitor_log(const char* format, ...) {
  if (!g_enabled || g_log_fd < 0) return;
  
  char buffer[1024];
  va_list args;
  va_start(args, format);
  
  time_t now = time(NULL);
  int prefix_len = snprintf(buffer, sizeof(buffer), "[%lu] ", (unsigned long)now);
  
  vsnprintf(buffer + prefix_len, sizeof(buffer) - prefix_len, format, args);
  va_end(args);
  
  /* Add newline if needed */
  size_t msg_len = strlen(buffer);
  if (msg_len > 0 && buffer[msg_len-1] != '\n') {
    if (msg_len < sizeof(buffer) - 1) {
      buffer[msg_len] = '\n';
      buffer[msg_len+1] = '\0';
      msg_len++;
    }
  }
  
  write(g_log_fd, buffer, msg_len);
  g_process.last_update = now;
}

/* Update process status */
void process_monitor_set_status(const char* status, const char* error_msg) {
  if (!g_enabled) return;
  
  strncpy(g_process.status, status, sizeof(g_process.status) - 1);
  
  if (error_msg) {
    strncpy(g_process.error_message, error_msg, sizeof(g_process.error_message) - 1);
  } else {
    g_process.error_message[0] = '\0';
  }
  
  g_process.last_update = time(NULL);
  
  process_monitor_log("Process status updated to: %s%s%s", 
            status,
            error_msg ? " (error: " : "",
            error_msg ? error_msg : "");
}

/* Set socket information */
void process_monitor_set_socket(int socket_fd, int port) {
  if (!g_enabled) return;
  
  g_process.socket_fd = socket_fd;
  g_process.port = port;
  
  process_monitor_log("Socket information updated: fd=%d, port=%d", socket_fd, port);
}

/* Update socket binding status */
void process_monitor_set_bound(int has_bound) {
  if (!g_enabled) return;
  
  g_process.has_bound = has_bound;
  process_monitor_log("Socket bind status updated: %s", has_bound ? "bound" : "not bound");
}

/* Update socket listening status */
void process_monitor_set_listening(int is_listening) {
  if (!g_enabled) return;
  
  g_process.is_listening = is_listening;
  process_monitor_log("Socket listen status updated: %s", is_listening ? "listening" : "not listening");
}

/* Set daemon mode */
void process_monitor_set_daemon(int is_daemon) {
  if (!g_enabled) return;
  
  g_process.is_daemon = is_daemon;
  process_monitor_log("Process daemon mode updated: %s", is_daemon ? "daemon" : "foreground");
}

/* Check socket state */
void process_monitor_check_socket() {
  if (!g_enabled || g_process.socket_fd < 0) return;
  
  process_monitor_log("Checking socket state for fd=%d, port=%d", g_process.socket_fd, g_process.port);
  
  /* Check socket error state */
  int socket_error = 0;
  socklen_t error_len = sizeof(socket_error);
  
  if (getsockopt(g_process.socket_fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_len) < 0) {
    process_monitor_log("Failed to check socket error: %s (errno=%d)", strerror(errno), errno);
  } else if (socket_error != 0) {
    process_monitor_log("Socket has error: %s (errno=%d)", strerror(socket_error), socket_error);
  } else {
    process_monitor_log("Socket is valid (no errors)");
  }
  
  /* Check if socket is in listening state */
  int acceptconn = 0;
  socklen_t acceptconn_len = sizeof(acceptconn);
  
  if (getsockopt(g_process.socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &acceptconn, &acceptconn_len) < 0) {
    process_monitor_log("Failed to check listen state: %s (errno=%d)", strerror(errno), errno);
  } else {
    g_process.is_listening = acceptconn;
    process_monitor_log("Socket listen state: %s", acceptconn ? "LISTENING" : "NOT LISTENING");
  }
  
  /* Use netstat to verify port is bound */
  if (g_process.port > 0) {
    char cmd[256];
    char output[1024] = {0};
    FILE *fp;
    
    snprintf(cmd, sizeof(cmd), 
        "netstat -tuln | grep ':%d ' 2>/dev/null || ss -tuln | grep ':%d ' 2>/dev/null", 
        g_process.port, g_process.port);
        
    fp = popen(cmd, "r");
    if (fp) {
      if (fread(output, 1, sizeof(output) - 1, fp) > 0) {
        output[sizeof(output) - 1] = '\0';
        process_monitor_log("Port %d is visible in netstat/ss: %s", g_process.port, output);
        g_process.has_bound = 1;
      } else {
        process_monitor_log("Port %d is NOT visible in netstat/ss", g_process.port);
        g_process.has_bound = 0;
      }
      pclose(fp);
    }
  }
}

/* Print process status report */
void process_monitor_report() {
  if (!g_enabled) return;
  
  time_t now = time(NULL);
  
  process_monitor_log("Process Monitor Status Report:");
  process_monitor_log(" PID: %d", g_process.pid);
  process_monitor_log(" Executable: %s", g_process.executable);
  process_monitor_log(" Status: %s", g_process.status);
  process_monitor_log(" Start Time: %lu (%ld seconds ago)", 
            (unsigned long)g_process.start_time, 
            now - g_process.start_time);
  process_monitor_log(" Last Update: %lu (%ld seconds ago)", 
            (unsigned long)g_process.last_update, 
            now - g_process.last_update);
  process_monitor_log(" Socket FD: %d", g_process.socket_fd);
  process_monitor_log(" Port: %d", g_process.port);
  process_monitor_log(" Socket Bound: %s", g_process.has_bound ? "Yes" : "No");
  process_monitor_log(" Socket Listening: %s", g_process.is_listening ? "Yes" : "No");
  process_monitor_log(" Daemon Mode: %s", g_process.is_daemon ? "Yes" : "No");
  
  if (g_process.error_message[0] != '\0') {
    process_monitor_log(" Error: %s", g_process.error_message);
  }
  
  /* Check system process info */
  char proc_status[MAX_PATH_LEN];
  snprintf(proc_status, sizeof(proc_status), "/proc/%d/status", g_process.pid);
  
  FILE* status_fp = fopen(proc_status, "r");
  if (status_fp) {
    char line[256];
    
    process_monitor_log("Process status from /proc:");
    
    while (fgets(line, sizeof(line), status_fp)) {
      /* Only log relevant lines */
      if (strncmp(line, "State:", 6) == 0 ||
        strncmp(line, "Pid:", 4) == 0 ||
        strncmp(line, "PPid:", 5) == 0 ||
        strncmp(line, "VmSize:", 7) == 0 ||
        strncmp(line, "VmRSS:", 6) == 0 ||
        strncmp(line, "Threads:", 8) == 0) {
        
        line[strcspn(line, "\n")] = '\0'; /* Remove newline */
        process_monitor_log("  %s", line);
      }
    }
    
    fclose(status_fp);
  }
  
  /* Check socket state */
  process_monitor_check_socket();
  
  /* Check for open file descriptors */
  char fd_path[MAX_PATH_LEN];
  char fd_link[MAX_PATH_LEN];
  snprintf(fd_path, sizeof(fd_path), "/proc/%d/fd", g_process.pid);
  
  DIR* fd_dir = opendir(fd_path);
  if (fd_dir) {
    struct dirent* entry;
    
    process_monitor_log("Open file descriptors:");
    
    while ((entry = readdir(fd_dir)) != NULL) {
      if (entry->d_name[0] == '.') continue;
      
      /* Ensure path won't truncate by limiting the filename length */
      char name_buf[64];
      strncpy(name_buf, entry->d_name, sizeof(name_buf)-1);
      name_buf[sizeof(name_buf)-1] = '\0';
      snprintf(fd_path, sizeof(fd_path), "/proc/%d/fd/%s", g_process.pid, name_buf);
      ssize_t len = readlink(fd_path, fd_link, sizeof(fd_link) - 1);
      
      if (len > 0) {
        fd_link[len] = '\0';
        process_monitor_log("  FD %s -> %s", entry->d_name, fd_link);
      }
    }
    
    closedir(fd_dir);
  }
}

/* Cleanup process monitor */
void process_monitor_cleanup() {
  if (!g_enabled) return;
  
  process_monitor_log("Process monitor cleanup");
  
  if (g_log_fd >= 0) {
    close(g_log_fd);
    g_log_fd = -1;
  }
  
  g_enabled = 0;
}

/* Signal handler for monitoring process signals */
void process_monitor_signal_handler(int signum) {
  if (!g_enabled) return;
  
  process_monitor_log("Received signal %d (%s)", signum, strsignal(signum));
  
  /* Generate a final status report before exiting on terminating signals */
  if (signum == SIGTERM || signum == SIGINT || signum == SIGQUIT) {
    process_monitor_set_status("terminating", NULL);
    process_monitor_report();
    process_monitor_cleanup();
    
    /* Re-raise the signal with default handler */
    signal(signum, SIG_DFL);
    raise(signum);
  }
}

/* Install signal handlers */
void process_monitor_install_handlers() {
  if (!g_enabled) return;
  
  process_monitor_log("Installing signal handlers");
  
  signal(SIGTERM, process_monitor_signal_handler);
  signal(SIGINT, process_monitor_signal_handler);
  signal(SIGQUIT, process_monitor_signal_handler);
  signal(SIGHUP, process_monitor_signal_handler);
  signal(SIGUSR1, process_monitor_signal_handler);
  signal(SIGUSR2, process_monitor_signal_handler);
}

/* Test function for standalone use */
#ifdef PROCESS_MONITOR_TEST
int main() {
  /* Initialize process monitor */
  process_monitor_init("/tmp/process_test.log");
  
  /* Install signal handlers */
  process_monitor_install_handlers();
  
  /* Update status */
  process_monitor_set_status("starting", NULL);
  
  /* Create a test socket */
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    process_monitor_set_status("error", strerror(errno));
    process_monitor_report();
    process_monitor_cleanup();
    return 1;
  }
  
  /* Set socket info */
  process_monitor_set_socket(sockfd, 9999);
  
  /* Set socket options */
  int opt = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  
  /* Bind to a test port */
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(9999);
  
  if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    process_monitor_set_status("bind_error", strerror(errno));
    process_monitor_report();
    close(sockfd);
    process_monitor_cleanup();
    return 1;
  }
  
  process_monitor_set_bound(1);
  
  /* Listen on socket */
  if (listen(sockfd, 5) < 0) {
    process_monitor_set_status("listen_error", strerror(errno));
    process_monitor_report();
    close(sockfd);
    process_monitor_cleanup();
    return 1;
  }
  
  process_monitor_set_listening(1);
  process_monitor_set_status("running", NULL);
  
  /* Print status report */
  process_monitor_report();
  
  /* Simulate running */
  printf("Process monitor running. Press Ctrl+C to exit.\n");
  
  /* Main loop */
  for (int i = 0; i < 60; i++) {
    sleep(1);
    if (i % 10 == 0) {
      process_monitor_log("Still running (%d seconds)", i);
      process_monitor_check_socket();
    }
  }
  
  /* Cleanup */
  process_monitor_set_status("shutting_down", NULL);
  process_monitor_report();
  close(sockfd);
  process_monitor_cleanup();
  
  return 0;
}
#endif /* PROCESS_MONITOR_TEST */