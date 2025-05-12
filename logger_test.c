#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/include/utils/logger.h"

int main(int argc, char *argv[]) {
    const char* log_file = "./test_logs/logger_test.log";
    
    // Initialize logger with different log levels based on argument
    log_level_t level = LOG_LEVEL_INFO;
    if (argc > 1) {
        if (strcmp(argv[1], "error") == 0) {
            level = LOG_LEVEL_ERROR;
        } else if (strcmp(argv[1], "warning") == 0) {
            level = LOG_LEVEL_WARNING;
        } else if (strcmp(argv[1], "info") == 0) {
            level = LOG_LEVEL_INFO;
        } else if (strcmp(argv[1], "debug") == 0) {
            level = LOG_LEVEL_DEBUG;
        } else if (strcmp(argv[1], "trace") == 0) {
            level = LOG_LEVEL_TRACE;
        }
    }
    
    printf("Initializing logger with level: %d\n", level);
    if (!logger_init(log_file, level)) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }
    
    // Test logging at different levels
    printf("Writing log messages at different levels...\n");
    LOG_ERROR("This is an ERROR message test");
    LOG_WARNING("This is a WARNING message test");
    LOG_INFO("This is an INFO message test");
    LOG_DEBUG("This is a DEBUG message test");
    LOG_TRACE("This is a TRACE message test");
    
    // Close logger
    logger_close();
    printf("Logger test completed. Check %s for log output.\n", log_file);
    
    return 0;
}