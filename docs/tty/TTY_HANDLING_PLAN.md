# TTY Handling Deprecation Plan

## Current Issues

1. The current dual-mode system (foreground/daemon) creates inconsistencies in:
   - Log handling
   - Error reporting
   - Socket binding sequence
   - Signal handling

2. Direct console output bypasses the logging framework, leading to:
   - Inconsistent error messages
   - Missed log entries in the central log file
   - Different behavior between modes

## Implementation Plan

### 1. Modify Command Line Processing

- Remove `-f/--foreground` option from command line arguments
- Add a `-v/--verbose` option which only affects log verbosity, not daemon behavior
- Always daemonize except when:
  - Running simple commands like `-h/--help` or `-v/--version`
  - Using JavaScript execution mode with `-j/--js-file`

### 2. Consolidate TTY Handling Early

- Move the daemonization code to execute much earlier in main()
- Perform daemonization immediately after basic command line parsing
- Ensure fork() and detachment from TTY happens before any significant initialization

### 3. Standardize Logging Setup

- Initialize the logging system immediately after daemonization
- Direct all output through the logging framework
- Ensure proper log file paths are established before any other initialization
- Option to have a log file for stdout/stderr redirection as well as the main log

### 4. Implement Predictable Socket Handling

- Create all sockets after daemonization and logging setup
- Use a single, consistent code path for socket binding
- Preserve socket descriptors properly during the process

### 5. Standardize Signal Handling

- Use a consistent approach for signal handling in all modes
- Set up signal handlers only after daemonization is complete

### 6. Implementation Sequence

1. Update command line processing to remove foreground mode
2. Restructure main.c to perform daemonization early:
   ```c
   int main(int argc, char* argv[]) {
       // Parse essential command line arguments
       
       // Handle immediate-exit cases like --help, --version
       
       // Immediately daemonize for all normal operations
       daemonize_server();
       
       // Initialize logging system
       setup_logging();
       
       // From here on, all output goes through the logging system
       // Continue with normal initialization...
   }
   ```

3. Implement a new daemonization function:
   ```c
   int daemonize_server() {
       // Fork process
       // Create new session
       // Close standard file descriptors
       // Redirect to /dev/null
       // Write PID file
       return 0; // success
   }
   ```

4. Update the logging system initialization:
   ```c
   void setup_logging() {
       // Initialize logger with proper file paths
       // Set log levels based on verbosity
       // Ensure all subsystems use the logging framework
   }
   ```

5. Update error handling to use the logging framework exclusively:
   ```c
   // Replace all fprintf(stderr, ...) with:
   if (g_logger) {
       LOG_ERROR(...);
   }
   ```

## Benefits

1. **Consistency**: Single operational mode with predictable behavior
2. **Better Debugging**: All messages captured in logs
3. **Improved Reliability**: Early detachment from TTY prevents orphaned processes
4. **Simplified Code**: Fewer conditionals and code paths
5. **Better Error Recovery**: All errors properly logged to persistent storage

## Testing Plan

1. Verify daemon operation with various command line combinations
2. Ensure all error conditions are properly logged
3. Test socket binding and thread synchronization
4. Verify proper process detachment from terminal
5. Confirm all output is correctly captured in logs

## Backward Compatibility

- Add a deprecation warning if `-f/--foreground` is used
- Maintain support for existing logging paths and configurations
- Ensure existing scripts that use the server continue to work