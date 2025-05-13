#!/bin/bash
# test_js_compilation_modes.sh
# Script to test JavaScript conditional compilation in both build modes

# ANSI color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== JavaScript Conditional Compilation Test Script ===${NC}"
echo "This script will test building with and without JavaScript support."

# Create a simple test file
TEST_FILE="/home/claude-3/project/test_js_modes.c"

cat > "$TEST_FILE" << 'EOF'
#include <stdio.h>
#include "jsondb/core/server.h"  /* For http types */
#include "jsondb/api/api.h"      /* For API context types */
#include "jsondb/js/js_engine.h"
#include "jsondb/js/js_api.h"
#include "jsondb/database/database.h"

int main() {
    printf("Testing JavaScript conditional compilation...\n");

#ifndef DISABLE_JS
    printf("JavaScript is ENABLED in this build.\n");

    // Test JavaScript initialization
    database_t *db = NULL;  // Just for testing, normally would be initialized

    // Initialize JavaScript
    js_api_init(db);

    // Try to evaluate a simple script (will fail due to null engine, but tests compilation)
    char *result = NULL;
    js_engine_eval(g_js_engine, "1+1", &result);

    // Clean up
    js_api_cleanup();

    printf("Successfully compiled and called JavaScript functions.\n");
#else
    printf("JavaScript is DISABLED in this build.\n");

    // Test stub functions
    database_t *db = NULL;  // Just for testing

    // These should be no-ops when DISABLE_JS is defined
    js_api_init(db);
    js_api_cleanup();

    printf("Successfully called JavaScript stub functions.\n");
#endif

    return 0;
}
EOF

echo -e "${YELLOW}Created test file: $TEST_FILE${NC}"

# Function to perform build test
test_js_build() {
    local js_flag=$1
    local js_status=$2
    local compile_flags="$3"
    
    echo -e "\n${BLUE}--- Testing build with JavaScript $js_status ---${NC}"
    
    # Compile test file
    echo -e "${YELLOW}Compiling test file...${NC}"
    # Only compile, don't try to link with libraries we don't have
    gcc -Wall -Iinclude $compile_flags -c "$TEST_FILE" -o test_js_${js_flag}.o 2>&1

    # If compilation succeeds, we've verified the conditional compilation
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Compilation succeeded!${NC}"
        echo -e "${GREEN}✓ Verified JavaScript conditional compilation for $js_status mode${NC}"
        # We're just testing compilation, not execution
        touch test_js_$js_flag
        return 0
    else
        echo -e "${RED}✗ Compilation failed!${NC}"
        return 1
    fi
}

# Test with JavaScript enabled (default)
test_js_build "enabled" "ENABLED" ""

# Test with JavaScript disabled
test_js_build "disabled" "DISABLED" "-DDISABLE_JS"

echo -e "\n${BLUE}=== JavaScript Conditional Compilation Test Summary ===${NC}"
echo "If both tests passed, the conditional compilation is working correctly."
echo "You should also run full builds with both modes to ensure complete compatibility."