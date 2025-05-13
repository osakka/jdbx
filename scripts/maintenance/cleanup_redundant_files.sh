#!/bin/bash
# Script to clean up redundant files after consolidation
# This script removes files from src/ that have been consolidated to src/components/

echo "Starting cleanup of redundant files..."
echo "This script will remove files from src/ that have been consolidated to src/components/"
echo "Backup files (.bak, etc.) will not be touched for safety"

# Function to check if a file exists in components
file_exists_in_components() {
    local src_file="$1"
    local rel_path="${src_file#/home/claude-3/project/src/}"
    local comp_path=""
    
    # Get the first directory component to determine where to look
    local first_dir=$(echo "$rel_path" | cut -d'/' -f1)
    
    # Special case for main.c in root
    if [ "$rel_path" = "main.c" ]; then
        if [ -f "/home/claude-3/project/src/components/main.c" ]; then
            return 0  # File exists in components
        else
            return 1  # File doesn't exist in components
        fi
    fi
    
    # For files in subdirectories
    if [ -f "/home/claude-3/project/src/components/$rel_path" ]; then
        return 0  # File exists in components
    else
        return 1  # File doesn't exist in components
    fi
}

# Files to skip (these files have intentional differences or are backups)
SKIP_FILES=(
    # Backup files (generally)
    ".bak"
    ".original"
    ".fixed"
    # Specific files to preserve
    "/home/claude-3/project/src/Makefile"
    "/home/claude-3/project/src/database/simplified_ops.h"
)

# Function to check if a file should be skipped
should_skip() {
    local file="$1"
    
    # Skip backup files
    for pattern in "${SKIP_FILES[@]}"; do
        if [[ "$file" == *"$pattern"* ]]; then
            return 0  # Should skip
        fi
    done
    
    return 1  # Should not skip
}

# Get all files in src/ excluding include and components directories
SRC_FILES=$(find /home/claude-3/project/src -type f -not -path "*/include/*" -not -path "*/components/*")

# Temporary file for storing files to be removed
TO_REMOVE_FILE=$(mktemp)

# Check each file
for src_file in $SRC_FILES; do
    if should_skip "$src_file"; then
        echo "Skipping $src_file (preserved file)"
        continue
    fi
    
    if file_exists_in_components "$src_file"; then
        # File exists in components, mark for removal
        echo "$src_file" >> "$TO_REMOVE_FILE"
    else
        echo "WARNING: $src_file does not have a corresponding file in components/"
    fi
done

# Count files to be removed
COUNT=$(wc -l < "$TO_REMOVE_FILE")
echo "Found $COUNT files to remove"

# Display files that will be removed
echo "Files to be removed:"
cat "$TO_REMOVE_FILE"
echo ""
echo "Review the list above. These files will be removed."
echo "IMPORTANT: If everything looks correct, run this script with the --execute parameter to perform the deletion."
echo "Example: bash $(basename "$0") --execute"

# If --execute is provided, perform the deletion
if [ "$1" = "--execute" ]; then
    echo "Executing removal..."
    while read -r file; do
        rm -v "$file"
    done < "$TO_REMOVE_FILE"
    echo "Cleanup complete."
else
    echo "Dry run complete. No files were removed."
fi

# Cleanup
rm "$TO_REMOVE_FILE"