#!/bin/bash
# Final fix for JavaScript conditional compilation in main.c

echo "Creating a final fix for JavaScript conditional compilation in main.c..."

# Create a backup of main.c
cp /home/claude-3/project/src/core/main.c /home/claude-3/project/src/core/main.c.bak.final

# Fix 1: Replace the entire problematic JS execution section with a proper implementation
cat > /tmp/js_section_fixed.c << 'EOL'
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

# Apply the fixed JS execution section to main.c
sed -i '/\/\* If running in script mode, initialize JavaScript now if enabled \*\//,/\/\* Exit normally to ensure proper cleanup \*\//c\    /* If running in script mode, initialize JavaScript now if enabled */\n    if (js_file) {\n        /* Check if JavaScript is enabled in config */\n        if (!config.js_enabled) {\n            fprintf(stderr, "Error: JavaScript is disabled in configuration\\n");\n            return 1;\n        }\n\n#ifndef DISABLE_JS\n        /* Initialize JavaScript engine for script execution */\n        printf("Initializing JavaScript engine for script execution...\\n");\n        if (g_logger) {\n            LOG_INFO("Initializing JavaScript engine for script execution...");\n        }\n\n        js_api_init(g_database);\n        printf("JavaScript engine initialized\\n");\n        \n        if (g_logger) {\n            LOG_INFO("JavaScript engine initialized");\n        }\n\n        /* Use our enhanced JavaScript file utilities to find the file */\n        char js_file_abs[PATH_MAX] = {0};\n\n        /* Log the search attempt */\n        if (g_logger) {\n            LOG_INFO("Searching for JavaScript file: %s", js_file);\n        } else {\n            printf("Searching for JavaScript file: %s\\n", js_file);\n        }\n\n        /* Try to find the file with our enhanced utility */\n        if (!js_file_find(js_file, js_file_abs, sizeof(js_file_abs))) {\n            fprintf(stderr, "JavaScript file not found: %s\\n", js_file);\n\n            if (g_logger) {\n                LOG_ERROR("JavaScript file not found: %s", js_file);\n                /* Log detailed search paths */\n                js_file_log_not_found(js_file, LOG_LEVEL_ERROR);\n            } else {\n                /* The file paths searched were not found (we'\''ll need to log manually) */\n                fprintf(stderr, "Checked common paths but file was not found.\\n");\n                fprintf(stderr, "Try using an absolute path or check file permissions.\\n");\n            }\n            return 1; /* Exit since we couldn'\''t find the file */\n        }\n\n        printf("Executing JavaScript file: %s (resolved to: %s)\\n", js_file, js_file_abs);\n        if (g_logger) {\n            LOG_INFO("Executing JavaScript file: %s (resolved to: %s)", js_file, js_file_abs);\n        }\n\n        /* Execute JavaScript file using the JS engine */\n        if (g_js_engine) {\n            if (!js_execute_file(g_js_engine, js_file_abs)) {\n                fprintf(stderr, "Failed to execute JavaScript file: %s (resolved to: %s)\\n", js_file, js_file_abs);\n                if (g_logger) {\n                    LOG_ERROR("Failed to execute JavaScript file: %s (resolved to: %s)", js_file, js_file_abs);\n                }\n                return 1;\n            }\n        } else {\n            fprintf(stderr, "JavaScript engine not initialized, cannot execute file\\n");\n            if (g_logger) {\n                LOG_ERROR("JavaScript engine not initialized, cannot execute file");\n            }\n            return 1;\n        }\n        \n        printf("JavaScript execution complete\\n");\n        if (g_logger) {\n            LOG_INFO("JavaScript execution complete");\n        }\n#else\n        /* JavaScript support is disabled */\n        printf("JavaScript support is disabled at compile time\\n");\n        if (g_logger) {\n            LOG_ERROR("JavaScript support is disabled at compile time");\n        }\n        return 1;\n#endif\n        /* Exit normally to ensure proper cleanup */\n        return 0;\n    }' /home/claude-3/project/src/core/main.c

# Fix 2: Fix the foreground mode JS initialization
sed -i '/if (g_server_config->foreground_mode)/,/printf("Server started in foreground mode. Press Ctrl+C to stop.\\n");/s/#ifndef DISABLE_JS\n            printf("Initializing JavaScript engine...\\n");\n            if (g_logger) {\n                LOG_INFO("Initializing JavaScript engine in foreground mode...");\n            }\n\n        js_api_init(g_database);/#ifndef DISABLE_JS\n            printf("Initializing JavaScript engine...\\n");\n            if (g_logger) {\n                LOG_INFO("Initializing JavaScript engine in foreground mode...");\n            }\n            js_api_init(g_database);/' /home/claude-3/project/src/core/main.c

# Fix 3: Fix the daemon mode JS initialization
sed -i '/if (g_server_config->js_enabled)/,/LOG_INFO("JavaScript support is disabled in configuration");/s/#ifndef DISABLE_JS\n            printf("Initializing JavaScript engine in daemon process...\\n");\n            if (g_logger) {\n                LOG_INFO("Initializing JavaScript engine in daemon process...");\n            }\n\n        js_api_init(g_database);/#ifndef DISABLE_JS\n            printf("Initializing JavaScript engine in daemon process...\\n");\n            if (g_logger) {\n                LOG_INFO("Initializing JavaScript engine in daemon process...");\n            }\n            js_api_init(g_database);/' /home/claude-3/project/src/core/main.c

echo "Done fixing main.c. Running a build test to verify the changes."