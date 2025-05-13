#!/bin/bash
# fix_js_api_conditionals.sh
# Script to fix all JavaScript conditional compilation in js_api.c

set -e  # Exit on any error

JS_API_FILE="/home/claude-3/project/src/js/js_api.c"
BACKUP_FILE="${JS_API_FILE}.bak"

# ANSI color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== JavaScript API Conditional Compilation Fix Script ===${NC}"
echo "This script will fix all conditional compilation in js_api.c"

# Make a backup
cp "$JS_API_FILE" "$BACKUP_FILE"
echo -e "${YELLOW}Created backup at ${BACKUP_FILE}${NC}"

# First, replace all #ifdef USE_QUICKJS with #ifndef DISABLE_JS
echo "Standardizing conditional compilation directives..."
sed -i 's/#ifdef USE_QUICKJS/#ifndef DISABLE_JS/g' "$JS_API_FILE"

# Update the endif comments
sed -i 's/#endif$/#endif \/* DISABLE_JS *\//g' "$JS_API_FILE"

# Update the else comments
sed -i 's/#else$/#else \/* JavaScript functionality disabled *\//g' "$JS_API_FILE"
sed -i 's/#else \/\* JavaScript functionality not available \*\//#else \/* JavaScript functionality disabled *\//g' "$JS_API_FILE"
sed -i 's/#else \/\* QuickJS is not available \*\//#else \/* JavaScript functionality disabled *\//g' "$JS_API_FILE"

# List of JavaScript handler functions to check
HANDLERS=(
    "api_handle_js_query"
    "api_handle_js_function_register"
    "api_handle_js_function_execute"
    "api_handle_js_validator_register"
    "api_handle_js_transformer_register"
    "api_handle_js_eval"
)

# Check and fix each handler
for handler in "${HANDLERS[@]}"; do
    echo -e "${YELLOW}Checking $handler...${NC}"
    
    # Check if the handler needs to be fixed
    if grep -q "#ifdef USE_QUICKJS.*$handler" "$JS_API_FILE"; then
        echo "  - Found handler with USE_QUICKJS directive, updating to DISABLE_JS"
    elif grep -q "#ifndef DISABLE_JS.*$handler" "$JS_API_FILE"; then
        echo "  - Handler already using DISABLE_JS directive, skipping"
        continue
    else
        echo "  - Handler doesn't have conditional compilation, adding it"
        # This is more complex and might require manual intervention
        echo "    * Manual review needed for $handler"
    fi
done

echo -e "${GREEN}Finished processing js_api.c!${NC}"
echo "Please manually verify the changes and run build tests for both configurations."