#!/bin/bash
#
# Comprehensive JavaScript conditional compilation fix
# This script fixes all JavaScript-related conditional compilation issues
# across the codebase, ensuring compatibility with both JS-enabled and JS-disabled builds.
#

echo "Starting comprehensive JavaScript conditional compilation fix..."

# Define HTTP constants in js_api.c if they don't exist
if grep -q "HTTP_NOT_IMPLEMENTED" src/js/js_api.c; then
    echo "HTTP_NOT_IMPLEMENTED already defined in js_api.c"
else
    echo "Adding HTTP_NOT_IMPLEMENTED definition to js_api.c"
    sed -i '/#include <limits.h>/a\\n/* Define HTTP_NOT_IMPLEMENTED if not available */\n#ifndef HTTP_NOT_IMPLEMENTED\n#define HTTP_NOT_IMPLEMENTED 501\n#endif' src/js/js_api.c
fi

# Ensure proper conditional compilation in all JavaScript-related files
JS_FILES=(
    "src/core/main.c"
    "src/js/js_api.c"
    "src/js/js_engine.c"
    "src/api/transaction_api.c"
    "src/api/index_api.c"
    "src/api/schema_api.c"
    "src/core/api.c"
)

# Process each file
for FILE in "${JS_FILES[@]}"; do
    echo "Processing $FILE..."
    
    if [ ! -f "$FILE" ]; then
        echo "Warning: $FILE does not exist, skipping"
        continue
    fi
    
    # Replace #ifdef USE_QUICKJS or similar with #ifndef DISABLE_JS
    sed -i 's/#ifdef USE_QUICKJS/#ifndef DISABLE_JS/g' "$FILE"
    
    # Add proper comments to endif blocks
    sed -i 's/#endif \+\/\* \+USE_QUICKJS \+\*\//#endif \/* DISABLE_JS *\//g' "$FILE"
    sed -i 's/#endif$/#endif \/* DISABLE_JS *\//g' "$FILE"
    
    # Fix missing void casts in stub functions to prevent unused parameter warnings
    sed -i '/#else \/* JavaScript functionality disabled \*\//,/#endif \/* DISABLE_JS \*\//s/\([a-zA-Z_][a-zA-Z0-9_]*\) \*\{0,1\}\([a-zA-Z_][a-zA-Z0-9_]*\)) {/\1 *\2) {\n    (void)\2; \/* Avoid unused parameter warning *\//g' "$FILE"
    
    echo "Completed processing $FILE"
done

# Specific fixes for main.c
echo "Applying specific fixes to main.c..."
if [ -f "src/core/main.c" ]; then
    # Ensure proper stub definitions when JS is disabled
    if ! grep -q "typedef void js_engine_t;" "src/core/main.c"; then
        echo "Adding stub definitions to main.c when JS is disabled"
        sed -i '/#else \/* JavaScript functionality disabled \*\//a\\n    /* Stub definitions when JavaScript is disabled */\n    typedef void js_engine_t;\n    void js_api_init(database_t* db) {\n        /* No-op implementation when JS is disabled */\n        (void)db; /* Avoid unused parameter warning */\n    }\n    void js_api_cleanup() {\n        /* No-op implementation when JS is disabled */\n    }' "src/core/main.c"
    fi
fi

echo "Comprehensive JavaScript conditional compilation fix completed!"