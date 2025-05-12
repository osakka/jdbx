#!/bin/bash
# fix_api_unused_params.sh
# Script to fix unused parameter warnings in the src/core/api.c file

# ANSI color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

API_FILE="/home/claude-3/project/src/core/api.c"
BACKUP_FILE="${API_FILE}.params_bak"

echo -e "${BLUE}=== Fix API Unused Parameters Script ===${NC}"
echo "This script adds (void) casts to silence unused parameter warnings in api.c."

# Make a backup
cp "$API_FILE" "$BACKUP_FILE"
echo -e "${YELLOW}Created backup at ${BACKUP_FILE}${NC}"

# List of placeholder API handler functions that need fixes
PLACEHOLDER_FUNCTIONS=(
    "api_handle_users_list"
    "api_handle_user_get"
    "api_handle_user_create"
    "api_handle_user_update"
    "api_handle_user_delete"
    "api_handle_roles_list"
    "api_handle_role_get"
    "api_handle_role_create"
    "api_handle_role_update"
    "api_handle_role_delete"
    "api_handle_config_get"
    "api_handle_config_update"
)

# Loop through each function and add (void) casts
for func in "${PLACEHOLDER_FUNCTIONS[@]}"; do
    echo -e "${YELLOW}Processing ${func}...${NC}"
    
    # Find the function line number
    line_num=$(grep -n "^http_response_t\* ${func}(" "$API_FILE" | cut -d: -f1)
    
    if [ -z "$line_num" ]; then
        echo "  - Function not found, skipping"
        continue
    fi
    
    # Find the opening brace line
    brace_line=$(tail -n +$line_num "$API_FILE" | grep -n "{" | head -1 | cut -d: -f1)
    actual_brace_line=$((line_num + brace_line - 1))
    
    # Check function signature for parameters
    func_line=$(sed -n "${line_num}p" "$API_FILE")
    
    # Extract parameters
    if [[ $func_line =~ api_context_t\*[[:space:]]+([a-zA-Z_][a-zA-Z0-9_]*) ]]; then
        ctx_param="${BASH_REMATCH[1]}"
        # Add void cast for ctx parameter
        sed -i "${actual_brace_line}a\\    (void)${ctx_param}; /* Avoid unused parameter warning */" "$API_FILE"
        echo "  - Added (void) cast for parameter '${ctx_param}'"
    fi
    
    if [[ $func_line =~ http_request_t\*[[:space:]]+([a-zA-Z_][a-zA-Z0-9_]*) ]]; then
        req_param="${BASH_REMATCH[1]}"
        # Add void cast for request parameter
        sed -i "${actual_brace_line}a\\    (void)${req_param}; /* Avoid unused parameter warning */" "$API_FILE"
        echo "  - Added (void) cast for parameter '${req_param}'"
    fi
done

# Compare files to see if changes were made
if diff -q "$API_FILE" "$BACKUP_FILE" >/dev/null; then
    echo -e "${RED}No changes were made to $API_FILE${NC}"
    rm "$BACKUP_FILE"
else
    echo -e "${GREEN}Successfully updated $API_FILE${NC}"
    echo "You should now run a build to check if warnings have been reduced."
fi