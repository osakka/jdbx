#!/bin/bash
# Script to fix component include paths after source consolidation

echo "Fixing component include paths after consolidation..."

# Find all .c and .h files in the components directory
find /home/claude-3/project/src/components -type f \( -name "*.c" -o -name "*.h" \) | while read file; do
    echo "Processing $file"
    # Replace "components/..." with the correct path
    sed -i 's|"components/|"|g' "$file"
done

echo "Include path fixes complete."