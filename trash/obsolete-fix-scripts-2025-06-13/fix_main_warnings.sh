#!/bin/bash
# fix_main_warnings.sh
# Script to fix the unused variable warnings in main.c

# ANSI color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

MAIN_FILE="/home/claude-3/project/src/core/main.c"
BACKUP_FILE="${MAIN_FILE}.main_bak"

echo -e "${BLUE}=== Fix Main Warnings Script ===${NC}"
echo "This script fixes unused variable warnings in main.c."

# Make a backup
cp "$MAIN_FILE" "$BACKUP_FILE"
echo -e "${YELLOW}Created backup at ${BACKUP_FILE}${NC}"

# Comment out unused variables
echo -e "${YELLOW}Commenting out unused variables...${NC}"

# Comment out unused global variable
sed -i 's/static char binary_dir\[PATH_MAX\]/\/\* Unused for now \*\/\n\/\*static char binary_dir\[PATH_MAX\]/' "$MAIN_FILE"

# Find the main function
MAIN_LINE=$(grep -n "int main(" "$MAIN_FILE" | head -1 | cut -d: -f1)

if [ -z "$MAIN_LINE" ]; then
    echo -e "${RED}Function main not found in $MAIN_FILE${NC}"
    rm "$BACKUP_FILE"
    exit 1
fi

# Comment out unused local variables
sed -i 's/    int show_help = 0;/    \/* Unused for now: int show_help = 0; *\//' "$MAIN_FILE"
sed -i 's/    int stop_server = 0;/    \/* Unused for now: int stop_server = 0; *\//' "$MAIN_FILE"
sed -i 's/    int check_status = 0;/    \/* Unused for now: int check_status = 0; *\//' "$MAIN_FILE"
sed -i 's/    int just_version = 0;/    \/* Unused for now: int just_version = 0; *\//' "$MAIN_FILE"
sed -i 's/    const char\* config_file = NULL;/    \/* Unused for now: const char\* config_file = NULL; *\//' "$MAIN_FILE"

# Compare files to see if changes were made
if diff -q "$MAIN_FILE" "$BACKUP_FILE" >/dev/null; then
    echo -e "${RED}No changes were made to $MAIN_FILE${NC}"
    rm "$BACKUP_FILE"
else
    echo -e "${GREEN}Successfully updated $MAIN_FILE${NC}"
    echo "You should now run a build to check if warnings have been reduced."
fi