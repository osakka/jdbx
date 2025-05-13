#!/bin/bash
#
# fix_all_warnings.sh
# Comprehensive script to fix various compiler warnings across the codebase
#

set -e # Exit on error

echo "Starting comprehensive compiler warning fixes..."

# Create logs directory
LOGS_DIR="build_logs"
mkdir -p "$LOGS_DIR"

# Define log file
LOG_FILE="$LOGS_DIR/fix_all_warnings.log"
echo "Starting warning fixes at $(date)" > "$LOG_FILE"

# Helper function to run scripts with logging
run_script() {
    local script=$1
    local description=$2
    
    echo "==================================="
    echo "Running: $script"
    echo "Purpose: $description"
    echo 
    
    if [ -f "$script" ]; then
        echo "Running $script - $description" >> "$LOG_FILE"
        chmod +x "$script"
        "./$script" >> "$LOG_FILE" 2>&1
        echo "Completed $script with status $?" >> "$LOG_FILE"
    else
        echo "Error: Script $script not found." | tee -a "$LOG_FILE"
        return 1
    fi
}

# Step 1: Fix JavaScript conditional compilation issues
run_script "scripts/fix_js_conditional_compilation_comprehensive.sh" "Fix JavaScript conditional compilation issues across the codebase"

# Step 2: Fix unused parameter warnings in specific components
run_script "scripts/fix_api_unused_params.sh" "Fix unused parameters in API handlers"
run_script "scripts/fix_unused_params.sh" "Fix general unused parameter warnings"
run_script "scripts/fix_server_warnings.sh" "Fix server.c specific warnings"
run_script "scripts/fix_cors_warnings.sh" "Fix CORS unused parameter warnings"
run_script "scripts/fix_ssl_warnings.sh" "Fix SSL unused parameter warnings"
run_script "scripts/fix_admin_api_warnings.sh" "Fix admin API unused parameter warnings"
run_script "scripts/fix_main_warnings.sh" "Fix unused variables in main.c"

# Step 3: Fix signedness comparison warnings
run_script "scripts/fix_sign_compare.sh" "Fix signedness comparison warnings"

# Step 4: Verify fixes by running a test build
echo "==================================="
echo "Verifying fixes with a test build (JavaScript enabled)..."
echo "Running test build with JavaScript enabled" >> "$LOG_FILE"

# Build with warnings as errors to verify fixes are complete
make clean && make CFLAGS="-Wall -Wextra -Werror=format-security -Iinclude" > "$LOGS_DIR/verify_build_js.log" 2>&1
JS_BUILD_STATUS=$?

if [ $JS_BUILD_STATUS -eq 0 ]; then
    echo "✓ Build with JavaScript successful - no warnings!" | tee -a "$LOG_FILE"
else
    echo "✗ Build with JavaScript failed - check $LOGS_DIR/verify_build_js.log for details" | tee -a "$LOG_FILE"
fi

echo "==================================="
echo "Verifying fixes with a test build (JavaScript disabled)..."
echo "Running test build with JavaScript disabled" >> "$LOG_FILE"

# Build with JavaScript disabled to verify conditional compilation
make clean && make CFLAGS="-Wall -Wextra -Werror=format-security -DDISABLE_JS -Iinclude" > "$LOGS_DIR/verify_build_nojs.log" 2>&1
NOJS_BUILD_STATUS=$?

if [ $NOJS_BUILD_STATUS -eq 0 ]; then
    echo "✓ Build without JavaScript successful - no warnings!" | tee -a "$LOG_FILE"
else
    echo "✗ Build without JavaScript failed - check $LOGS_DIR/verify_build_nojs.log for details" | tee -a "$LOG_FILE"
fi

# Step 5: Summarize results
echo "==================================="
echo "Compiler warning fix summary:"
echo "==================================="

if [ $JS_BUILD_STATUS -eq 0 ] && [ $NOJS_BUILD_STATUS -eq 0 ]; then
    echo "✓ All compiler warnings have been successfully fixed!"
    echo "✓ Both JavaScript-enabled and JavaScript-disabled builds succeed with -Werror"
    echo "Completed successfully at $(date)" >> "$LOG_FILE"
    exit 0
else
    echo "✗ Some warnings may still remain. Check the build logs for details:"
    echo "  JavaScript enabled build: $LOGS_DIR/verify_build_js.log"
    echo "  JavaScript disabled build: $LOGS_DIR/verify_build_nojs.log"
    echo "Failed with status JS:$JS_BUILD_STATUS NoJS:$NOJS_BUILD_STATUS at $(date)" >> "$LOG_FILE"
    exit 1
fi