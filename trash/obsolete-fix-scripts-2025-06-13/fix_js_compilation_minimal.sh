#!/bin/bash

# Minimal fix for JavaScript conditional compilation issues in the codebase
# This script makes minimal changes to main.c to support conditional compilation

echo "Fixing JavaScript conditional compilation issues (minimal approach)..."

# First, let's restore the original main.c file from backup
if [ -f src/core/main.c.bak ]; then
    echo "Restoring original main.c from backup..."
    cp src/core/main.c.bak src/core/main.c
else
    echo "No backup found, creating one first..."
    cp src/core/main.c src/core/main.c.bak
fi

# Add stub definitions at the top of the file
sed -i '/#include "jsondb\/js\/js_api.h"/i \/* JavaScript dependencies *\/' src/core/main.c
sed -i '/#include "jsondb\/js\/js_api.h"/i \#ifndef DISABLE_JS' src/core/main.c
sed -i '/#include "jsondb\/js\/js_engine.h"/a \#else\n/* Stub definitions when JavaScript is disabled */\ntypedef void js_engine_t;\nvoid js_api_init(database_t* db) { \n    /* No-op implementation when JS is disabled */\n    (void)db; /* Avoid unused parameter warning */\n}\nvoid js_api_cleanup() { \n    /* No-op implementation when JS is disabled */\n}\nint js_execute_file(js_engine_t* engine, const char* filename) {\n    /* No-op implementation when JS is disabled */\n    (void)engine; /* Avoid unused parameter warning */\n    (void)filename; /* Avoid unused parameter warning */\n    return 0;\n}\nvoid js_engine_free(js_engine_t* engine) {\n    /* No-op implementation when JS is disabled */\n    (void)engine; /* Avoid unused parameter warning */\n}\njs_engine_t* g_js_engine = NULL;\n#endif' src/core/main.c

# Wrap g_js_engine declaration with conditional
sed -i '/extern js_engine_t\* g_js_engine;/i \#ifndef DISABLE_JS' src/core/main.c
sed -i '/extern js_engine_t\* g_js_engine;/a \#endif' src/core/main.c

# Wrap all js_api_init calls with conditionals
sed -i 's/js_api_init(g_database);/#ifndef DISABLE_JS\n        js_api_init(g_database);\n#endif/' src/core/main.c

# Wrap js_execute_file calls with conditionals
sed -i '/if (g_js_engine) {/i \#ifndef DISABLE_JS' src/core/main.c
sed -i '/if (g_js_engine) {/a \#else\n            fprintf(stderr, "JavaScript support is disabled\\n");\n            if (g_logger) {\n                LOG_INFO("JavaScript support is disabled");\n            }\n            return 1;\n#endif' src/core/main.c

# Wrap cleanup of js_engine with conditionals
sed -i '/js_engine_t\* engine = g_js_engine;/i \#ifndef DISABLE_JS' src/core/main.c
sed -i '/js_engine_free(engine);/a \#endif' src/core/main.c

echo "Applied minimal JavaScript conditional compilation fixes"
echo "Original file is backed up at src/core/main.c.bak"