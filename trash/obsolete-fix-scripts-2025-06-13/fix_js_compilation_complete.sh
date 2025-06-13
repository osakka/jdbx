#!/bin/bash
# Complete fix for JavaScript conditional compilation in main.c
# This script tries a different approach by fixing all potential issues with JavaScript flags

# Location 1: Fix the JavaScript header includes and stub implementations
sed -i '7,32s/#ifndef DISABLE_JS\n#include "jsondb\/js\/js_api.h"\n#include "jsondb\/js\/js_engine.h"\n#else\n\/\* Stub definitions when JavaScript is disabled \*\//#ifndef DISABLE_JS\n#include "jsondb\/js\/js_api.h"\n#include "jsondb\/js\/js_engine.h"\n#else\n\/\* Stub definitions when JavaScript is disabled \*\//' /home/claude-3/project/src/core/main.c

# Location 2: Fix the JS engine access for conditional compilation
sed -i '71,76s/\/\* Access to JS engine \*\/\n#ifndef DISABLE_JS\nextern js_engine_t\* g_js_engine;\n#endif/\/\* Access to JS engine \*\/\n#ifndef DISABLE_JS\nextern js_engine_t\* g_js_engine;\n#endif/' /home/claude-3/project/src/core/main.c

# Location 3: Fix cleanly the script execution code to handle both cases
cat > /tmp/js_script_section.txt << 'EOL'
    /* If running in script mode, initialize JavaScript now if enabled */
    if (js_file) {
        /* Check if JavaScript is enabled in config */
        if (!config.js_enabled) {
            fprintf(stderr, "Error: JavaScript is disabled in configuration\n");
            return 1;
        }

#ifndef DISABLE_JS
        /* Initialize JavaScript engine for script execution */
        printf("Initializing JavaScript engine for script execution...\n");
        if (g_logger) {
            LOG_INFO("Initializing JavaScript engine for script execution...");
        }

        js_api_init(g_database);
        printf("JavaScript engine initialized\n");
        
        if (g_logger) {
            LOG_INFO("JavaScript engine initialized");
        }

        /* Use our enhanced JavaScript file utilities to find the file */
        char js_file_abs[PATH_MAX] = {0};

        /* Log the search attempt */
        if (g_logger) {
            LOG_INFO("Searching for JavaScript file: %s", js_file);
        } else {
            printf("Searching for JavaScript file: %s\n", js_file);
        }

        /* Try to find the file with our enhanced utility */
        if (!js_file_find(js_file, js_file_abs, sizeof(js_file_abs))) {
            fprintf(stderr, "JavaScript file not found: %s\n", js_file);

            if (g_logger) {
                LOG_ERROR("JavaScript file not found: %s", js_file);
                /* Log detailed search paths */
                js_file_log_not_found(js_file, LOG_LEVEL_ERROR);
            } else {
                /* The file paths searched were not found (we'll need to log manually) */
                fprintf(stderr, "Checked common paths but file was not found.\n");
                fprintf(stderr, "Try using an absolute path or check file permissions.\n");
            }
            return 1; /* Exit since we couldn't find the file */
        }

        printf("Executing JavaScript file: %s (resolved to: %s)\n", js_file, js_file_abs);
        if (g_logger) {
            LOG_INFO("Executing JavaScript file: %s (resolved to: %s)", js_file, js_file_abs);
        }

        /* Execute JavaScript file using the JS engine */
        if (g_js_engine) {
            if (!js_execute_file(g_js_engine, js_file_abs)) {
                fprintf(stderr, "Failed to execute JavaScript file: %s (resolved to: %s)\n", js_file, js_file_abs);
                if (g_logger) {
                    LOG_ERROR("Failed to execute JavaScript file: %s (resolved to: %s)", js_file, js_file_abs);
                }
                return 1;
            }
        } else {
            fprintf(stderr, "JavaScript engine not initialized, cannot execute file\n");
            if (g_logger) {
                LOG_ERROR("JavaScript engine not initialized, cannot execute file");
            }
            return 1;
        }
        
        printf("JavaScript execution complete\n");
        if (g_logger) {
            LOG_INFO("JavaScript execution complete");
        }
#else
        /* JavaScript support is disabled */
        printf("JavaScript support is disabled at compile time\n");
        if (g_logger) {
            LOG_ERROR("JavaScript support is disabled at compile time");
        }
        return 1;
#endif
        /* Exit normally to ensure proper cleanup */
        return 0;
    }
EOL

# Find beginning and end of script execution section to replace
start_line=$(grep -n "\/\* If running in script mode" /home/claude-3/project/src/core/main.c | head -1 | cut -d: -f1)
end_line=$(tail -n +$start_line /home/claude-3/project/src/core/main.c | grep -n "return 0;" | head -1 | cut -d: -f1)
end_line=$((start_line + end_line))

# Replace the section
sed -i "${start_line},${end_line}d" /home/claude-3/project/src/core/main.c
sed -i "${start_line}r /tmp/js_script_section.txt" /home/claude-3/project/src/core/main.c

# Location 4: Fix foreground mode JS initialization
cat > /tmp/js_foreground_section.txt << 'EOL'
        /* Initialize JavaScript engine in the main process, if enabled */
        if (g_server_config->js_enabled) {
#ifndef DISABLE_JS
            printf("Initializing JavaScript engine...\n");
            if (g_logger) {
                LOG_INFO("Initializing JavaScript engine in foreground mode...");
            }
            
            js_api_init(g_database);

            printf("JavaScript engine initialized\n");
            if (g_logger) {
                LOG_INFO("JavaScript engine initialized");
            }
#else
            printf("JavaScript support is disabled at compile time\n");
            if (g_logger) {
                LOG_INFO("JavaScript support is disabled at compile time");
            }
#endif
        } else {
            printf("JavaScript support is disabled in configuration\n");
            if (g_logger) {
                LOG_INFO("JavaScript support is disabled in configuration");
            }
        }
EOL

# Find foreground mode section
foreground_start=$(grep -n "\/\* Initialize JavaScript engine in the main process" /home/claude-3/project/src/core/main.c | head -1 | cut -d: -f1)
foreground_end=$(tail -n +$foreground_start /home/claude-3/project/src/core/main.c | grep -n "LOG_INFO(\"JavaScript support is disabled in configuration\");" | head -1 | cut -d: -f1)
foreground_end=$((foreground_start + foreground_end + 2))  # +2 for the closing brace and newline

# Replace the section
sed -i "${foreground_start},${foreground_end}d" /home/claude-3/project/src/core/main.c
sed -i "${foreground_start}r /tmp/js_foreground_section.txt" /home/claude-3/project/src/core/main.c

# Location 5: Fix daemon mode JS initialization
cat > /tmp/js_daemon_section.txt << 'EOL'
        /* Initialize JavaScript engine after fork to avoid concurrency issues, if enabled */
        if (g_server_config->js_enabled) {
#ifndef DISABLE_JS
            printf("Initializing JavaScript engine in daemon process...\n");
            if (g_logger) {
                LOG_INFO("Initializing JavaScript engine in daemon process...");
            }
            
            js_api_init(g_database);

            printf("JavaScript engine initialized\n");
            if (g_logger) {
                LOG_INFO("JavaScript engine initialized");
            }
#else
            printf("JavaScript support is disabled at compile time\n");
            if (g_logger) {
                LOG_INFO("JavaScript support is disabled at compile time");
            }
#endif
        } else {
            printf("JavaScript support is disabled in configuration\n");
            if (g_logger) {
                LOG_INFO("JavaScript support is disabled in configuration");
            }
        }
EOL

# Find daemon mode section
daemon_start=$(grep -n "\/\* Initialize JavaScript engine after fork" /home/claude-3/project/src/core/main.c | head -1 | cut -d: -f1)
daemon_end=$(tail -n +$daemon_start /home/claude-3/project/src/core/main.c | grep -n "LOG_INFO(\"JavaScript support is disabled in configuration\");" | head -1 | cut -d: -f1)
daemon_end=$((daemon_start + daemon_end + 2))  # +2 for the closing brace and newline

# Replace the section
sed -i "${daemon_start},${daemon_end}d" /home/claude-3/project/src/core/main.c
sed -i "${daemon_start}r /tmp/js_daemon_section.txt" /home/claude-3/project/src/core/main.c

# Remove any duplicated closing braces that might be causing errors
sed -i 's/    }    }/    }/' /home/claude-3/project/src/core/main.c

echo "Fixed main.c file complete. Now running build test..."