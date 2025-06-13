#!/bin/bash
# fix_server_warnings.sh
# Script to fix the signedness comparison warnings in server.c

# ANSI color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

SERVER_FILE="/home/claude-3/project/src/core/server.c"
BACKUP_FILE="${SERVER_FILE}.server_bak"

echo -e "${BLUE}=== Fix Server Warnings Script ===${NC}"
echo "This script fixes signedness comparison warnings in server.c."

# Make a backup
cp "$SERVER_FILE" "$BACKUP_FILE"
echo -e "${YELLOW}Created backup at ${BACKUP_FILE}${NC}"

# Fix signedness comparison issues
echo -e "${YELLOW}Fixing signedness comparison issues...${NC}"

# Replace patterns like "n < 0 || n >= remaining" with "n < 0 || (size_t)n >= remaining"
sed -i 's/\(n < 0 || \)n >= remaining/\1(size_t)n >= remaining/g' "$SERVER_FILE"

# Compare files to see if changes were made
if diff -q "$SERVER_FILE" "$BACKUP_FILE" >/dev/null; then
    echo -e "${RED}No changes were made to $SERVER_FILE${NC}"
    rm "$BACKUP_FILE"
else
    echo -e "${GREEN}Successfully updated $SERVER_FILE${NC}"
    echo "You should now run a build to check if warnings have been reduced."
fi