#!/bin/bash
# Comprehensive fix for JavaScript conditional compilation in main.c

echo "Fixing JavaScript conditional compilation issues in main.c..."

# Create a backup of main.c
cp /home/claude-3/project/src/core/main.c /home/claude-3/project/src/core/main.c.bak.fix

# Fix 1: Clean up the JavaScript file execution section
sed -i '1465,1498s/            }/        }/' /home/claude-3/project/src/core/main.c
sed -i '1480s/            }/    }/' /home/claude-3/project/src/core/main.c

# Fix 2: Ensure proper JS engine initialization in daemon mode
sed -i '1639s/            js_api_init(g_database);/        js_api_init(g_database);/' /home/claude-3/project/src/core/main.c
sed -i '1595s/            js_api_init(g_database);/        js_api_init(g_database);/' /home/claude-3/project/src/core/main.c

# Fix 3: Ensure correct declaration pattern for g_server_config
sed -i '1511s/g_server_config =/g_server_config = (server_config_t*)/' /home/claude-3/project/src/core/main.c

echo "Fixes applied to main.c. Run a build test to verify."