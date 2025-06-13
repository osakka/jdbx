#!/bin/bash
# fix_cors_warnings.sh
# Script to fix the unused parameter warnings in cors.c

# ANSI color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

CORS_FILE="/home/claude-3/project/src/core/cors.c"
BACKUP_FILE="${CORS_FILE}.cors_bak"

echo -e "${BLUE}=== Fix CORS Warnings Script ===${NC}"
echo "This script adds (void) casts to silence unused parameter warnings in cors.c."

# Make a backup
cp "$CORS_FILE" "$BACKUP_FILE"
echo -e "${YELLOW}Created backup at ${BACKUP_FILE}${NC}"

# Find the apply_cors_headers function
LINE_NUM=$(grep -n "apply_cors_headers" "$CORS_FILE" | head -1 | cut -d: -f1)

if [ -z "$LINE_NUM" ]; then
    echo -e "${RED}Function apply_cors_headers not found in $CORS_FILE${NC}"
    rm "$BACKUP_FILE"
    exit 1
fi

# Find the opening brace line
BRACE_LINE=$(tail -n +$LINE_NUM "$CORS_FILE" | grep -n "{" | head -1 | cut -d: -f1)
ACTUAL_BRACE_LINE=$((LINE_NUM + BRACE_LINE - 1))

echo -e "${YELLOW}Adding (void) casts for unused parameters...${NC}"

# Add (void) casts for cors and origin parameters
sed -i "${ACTUAL_BRACE_LINE}a\\    (void)origin; /* Avoid unused parameter warning */" "$CORS_FILE"
sed -i "${ACTUAL_BRACE_LINE}a\\    (void)cors; /* Avoid unused parameter warning */" "$CORS_FILE"

# Compare files to see if changes were made
if diff -q "$CORS_FILE" "$BACKUP_FILE" >/dev/null; then
    echo -e "${RED}No changes were made to $CORS_FILE${NC}"
    rm "$BACKUP_FILE"
else
    echo -e "${GREEN}Successfully updated $CORS_FILE${NC}"
    echo "You should now run a build to check if warnings have been reduced."
fi