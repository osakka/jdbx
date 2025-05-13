#!/bin/bash
# Script to fix include paths after source consolidation

echo "Fixing include paths after source consolidation..."

# Function to fix include paths in a file
fix_includes() {
    local file=$1
    
    # Replace "src/include/" with ""
    sed -i 's|"src/include/|"|g' "$file"
}

# Find all .c and .h files in the components directory
COMPONENT_FILES=$(find /home/claude-3/project/src/components -type f \( -name "*.c" -o -name "*.h" \))

# Process each file
for file in $COMPONENT_FILES; do
    echo "Processing $file"
    fix_includes "$file"
done

echo "Include path fixes complete."