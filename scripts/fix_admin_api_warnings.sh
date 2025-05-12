#!/bin/bash
# fix_admin_api_warnings.sh
# Script to fix unused parameter warnings in admin_api.c

# ANSI color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

ADMIN_API_FILE="/home/claude-3/project/src/api/admin_api.c"
BACKUP_FILE="${ADMIN_API_FILE}.admin_api_bak"

echo -e "${BLUE}=== Fix Admin API Warnings Script ===${NC}"
echo "This script adds (void) casts to silence unused parameter warnings in admin_api.c."

# Make a backup
cp "$ADMIN_API_FILE" "$BACKUP_FILE"
echo -e "${YELLOW}Created backup at ${BACKUP_FILE}${NC}"

# Find the api_handle_admin_test function and add (void) casts
echo -e "${YELLOW}Adding (void) casts for unused parameters...${NC}"

# Fix api_handle_admin_test
LINE_NUM=$(grep -n "api_handle_admin_test" "$ADMIN_API_FILE" | grep -v "typedef" | head -1 | cut -d: -f1)
if [ -n "$LINE_NUM" ]; then
    BRACE_LINE=$(tail -n +$LINE_NUM "$ADMIN_API_FILE" | grep -n "{" | head -1 | cut -d: -f1)
    ACTUAL_BRACE_LINE=$((LINE_NUM + BRACE_LINE - 1))
    sed -i "${ACTUAL_BRACE_LINE}a\\    (void)request; /* Avoid unused parameter warning */" "$ADMIN_API_FILE"
    sed -i "${ACTUAL_BRACE_LINE}a\\    (void)ctx; /* Avoid unused parameter warning */" "$ADMIN_API_FILE"
fi

# Compare files to see if changes were made
if diff -q "$ADMIN_API_FILE" "$BACKUP_FILE" >/dev/null; then
    echo -e "${RED}No changes were made to $ADMIN_API_FILE${NC}"
    rm "$BACKUP_FILE"
else
    echo -e "${GREEN}Successfully updated $ADMIN_API_FILE${NC}"
    echo "You should now run a build to check if warnings have been reduced."
fi