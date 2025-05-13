#!/bin/bash
# Script to finalize deprecation of old source files
# This script ensures that all old source files have been fully removed

ROOT_DIR="/home/claude-3/project"
cd "$ROOT_DIR" || exit 1

echo "Finalizing source code deprecation..."

# List of directories to check for leftover files
DEPRECATED_DIRS=(
  "src/api"
  "src/core"
  "src/database"
  "src/js"
  "src/query"
  "src/rbac"
  "src/tools"
  "src/transaction"
  "src/utils"
)

# Check for left-over files
FOUND_FILES=0

for dir in "${DEPRECATED_DIRS[@]}"; do
  if [ -d "$dir" ]; then
    echo "Found leftover directory: $dir"
    FILES=$(find "$dir" -type f)
    if [ -n "$FILES" ]; then
      echo "Files in $dir:"
      echo "$FILES"
      FOUND_FILES=1
    fi
  fi
done

# Check for leftover main.c
if [ -f "src/main.c" ]; then
  echo "Found leftover src/main.c file"
  FOUND_FILES=1
fi

if [ $FOUND_FILES -eq 0 ]; then
  echo "No leftover files found. Deprecation is complete."
else
  echo "Leftover files found. Proceeding with cleanup..."
  
  # Remove leftover directories
  for dir in "${DEPRECATED_DIRS[@]}"; do
    if [ -d "$dir" ]; then
      echo "Removing directory: $dir"
      rm -rf "$dir"
    fi
  done
  
  # Remove leftover main.c if it exists
  if [ -f "src/main.c" ]; then
    echo "Removing file: src/main.c"
    rm -f "src/main.c"
  fi
  
  echo "Cleanup complete."
fi

# Add a note about temporary files
echo "Note: Temporary and backup (.bak) files were intentionally kept during the migration."
echo "      You may want to manually review and clean them up when appropriate."
echo ""
echo "Deprecation process finalized."