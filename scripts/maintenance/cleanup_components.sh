#!/bin/bash

# Cleanup duplicate components directory structure
# This script removes the src/components directory which contains duplicate files
# that violate the "one source of truth" principle

echo "Removing duplicate files from src/components directory..."

# Check if the src/components directory exists
if [ ! -d "src/components" ]; then
    echo "Error: src/components directory not found"
    exit 1
fi

# Backup the directory in case we need to restore it
echo "Creating backup of src/components to src_components_backup..."
mkdir -p src_components_backup
cp -r src/components/* src_components_backup/

# Remove the components directory
echo "Removing src/components directory..."
rm -rf src/components

echo "Duplicate files cleanup complete."
echo "If needed, the original files are available in src_components_backup/"

# Update project_structure.md to reflect the cleanup
sed -i '/.*├── components\/.*/d' doc/architecture/project_structure.md

echo "Documentation updated to reflect changes."

# Done
echo "Cleanup complete."