#!/bin/bash
# Script to check and fix build errors after consolidation

echo "Checking for build errors in components directory..."

# Function to check common issues in all files
check_files() {
    local dir=$1
    
    echo "Checking files in $dir..."
    
    # Find .c and .h files
    files=$(find "$dir" -type f \( -name "*.c" -o -name "*.h" \))
    
    for file in $files; do
        echo "Checking $file..."
        
        # Check for old component includes
        grep -n "include \"components/" "$file" && echo "  - Found old component include path"
        
        # Check for src/include paths
        grep -n "include \"src/include/" "$file" && echo "  - Found src/include path"
        
        # Check for explicit absolute paths
        grep -n "include \"/home/claude-3/project/" "$file" && echo "  - Found absolute path include"
    done
}

# Check components directories
for component in api core database js query rbac tools transaction utils; do
    check_files "/home/claude-3/project/src/components/$component"
done

# Also check the main component file
echo "Checking /home/claude-3/project/src/components/main.c..."
grep -n "include \"components/" "/home/claude-3/project/src/components/main.c" && echo "  - Found old component include path"
grep -n "include \"src/include/" "/home/claude-3/project/src/components/main.c" && echo "  - Found src/include path"

echo "Check complete."