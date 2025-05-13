#!/bin/bash
# fix_ssl_warnings.sh
# Script to fix unused parameter warnings in ssl.c

# ANSI color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

SSL_FILE="/home/claude-3/project/src/core/ssl.c"
BACKUP_FILE="${SSL_FILE}.ssl_bak"

echo -e "${BLUE}=== Fix SSL Warnings Script ===${NC}"
echo "This script adds (void) casts to silence unused parameter warnings in ssl.c."

# Make a backup
cp "$SSL_FILE" "$BACKUP_FILE"
echo -e "${YELLOW}Created backup at ${BACKUP_FILE}${NC}"

# Find the SSL functions and add (void) casts
echo -e "${YELLOW}Adding (void) casts for unused parameters...${NC}"

# Fix ssl_create_connection
LINE_NUM=$(grep -n "ssl_create_connection" "$SSL_FILE" | grep -v "typedef" | head -1 | cut -d: -f1)
if [ -n "$LINE_NUM" ]; then
    BRACE_LINE=$(tail -n +$LINE_NUM "$SSL_FILE" | grep -n "{" | head -1 | cut -d: -f1)
    ACTUAL_BRACE_LINE=$((LINE_NUM + BRACE_LINE - 1))
    sed -i "${ACTUAL_BRACE_LINE}a\\    (void)socket_fd; /* Avoid unused parameter warning */" "$SSL_FILE"
    sed -i "${ACTUAL_BRACE_LINE}a\\    (void)context; /* Avoid unused parameter warning */" "$SSL_FILE"
fi

# Fix ssl_free_connection
LINE_NUM=$(grep -n "ssl_free_connection" "$SSL_FILE" | grep -v "typedef" | head -1 | cut -d: -f1)
if [ -n "$LINE_NUM" ]; then
    BRACE_LINE=$(tail -n +$LINE_NUM "$SSL_FILE" | grep -n "{" | head -1 | cut -d: -f1)
    ACTUAL_BRACE_LINE=$((LINE_NUM + BRACE_LINE - 1))
    sed -i "${ACTUAL_BRACE_LINE}a\\    (void)ssl; /* Avoid unused parameter warning */" "$SSL_FILE"
fi

# Fix ssl_read
LINE_NUM=$(grep -n "ssl_read" "$SSL_FILE" | grep -v "typedef" | head -1 | cut -d: -f1)
if [ -n "$LINE_NUM" ]; then
    BRACE_LINE=$(tail -n +$LINE_NUM "$SSL_FILE" | grep -n "{" | head -1 | cut -d: -f1)
    ACTUAL_BRACE_LINE=$((LINE_NUM + BRACE_LINE - 1))
    sed -i "${ACTUAL_BRACE_LINE}a\\    (void)length; /* Avoid unused parameter warning */" "$SSL_FILE"
    sed -i "${ACTUAL_BRACE_LINE}a\\    (void)buffer; /* Avoid unused parameter warning */" "$SSL_FILE"
    sed -i "${ACTUAL_BRACE_LINE}a\\    (void)ssl; /* Avoid unused parameter warning */" "$SSL_FILE"
fi

# Fix ssl_write
LINE_NUM=$(grep -n "ssl_write" "$SSL_FILE" | grep -v "typedef" | head -1 | cut -d: -f1)
if [ -n "$LINE_NUM" ]; then
    BRACE_LINE=$(tail -n +$LINE_NUM "$SSL_FILE" | grep -n "{" | head -1 | cut -d: -f1)
    ACTUAL_BRACE_LINE=$((LINE_NUM + BRACE_LINE - 1))
    sed -i "${ACTUAL_BRACE_LINE}a\\    (void)length; /* Avoid unused parameter warning */" "$SSL_FILE"
    sed -i "${ACTUAL_BRACE_LINE}a\\    (void)buffer; /* Avoid unused parameter warning */" "$SSL_FILE"
    sed -i "${ACTUAL_BRACE_LINE}a\\    (void)ssl; /* Avoid unused parameter warning */" "$SSL_FILE"
fi

# Compare files to see if changes were made
if diff -q "$SSL_FILE" "$BACKUP_FILE" >/dev/null; then
    echo -e "${RED}No changes were made to $SSL_FILE${NC}"
    rm "$BACKUP_FILE"
else
    echo -e "${GREEN}Successfully updated $SSL_FILE${NC}"
    echo "You should now run a build to check if warnings have been reduced."
fi