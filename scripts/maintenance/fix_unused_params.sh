#\!/bin/bash
#
# fix_unused_params.sh
# Fixes unused parameter warnings in the JSONdb codebase
#

echo "Starting to fix unused parameter warnings..."

# Find all files with unused parameter warnings
echo "Finding files with unused parameter warnings..."
files_with_warnings=$(find src -name "*.c" -exec gcc -Wall -Wextra -c -I./include {} -o /dev/null 2>&1 \;  < /dev/null |  grep "unused parameter" | awk -F":" '{print $1}' | sort -u)

if [ -z "$files_with_warnings" ]; then
    echo "No files with unused parameter warnings found."
    exit 0
fi

# Process each file to fix unused parameter warnings
for file in $files_with_warnings; do
    echo "Processing $file..."
    
    # Get list of unused parameter warnings for this file
    unused_params=$(gcc -Wall -Wextra -c -I./include "$file" -o /dev/null 2>&1 | grep "unused parameter" | awk -F"'" '{print $2}')
    
    # Create backup of the file
    cp "$file" "${file}.bak"
    
    # For each unused parameter
    for param in $unused_params; do
        echo "  Fixing unused parameter '$param'..."
        
        # Find all function definitions with this parameter and add UNUSED macro
        # This only works for simple cases, more complex ones will need manual attention
        sed -i "s/\([[:space:]]\)$param\([,)]\)/\1UNUSED($param)\2/g" "$file"
    done
    
    # Add UNUSED macro definition if not already present
    if \! grep -q "#define UNUSED" "$file"; then
        # Add after the include statements
        sed -i '/^#include/a\
/* Macro to suppress unused parameter warnings */\
#define UNUSED(x) (void)(x)\
' "$file"
    fi
    
    echo "Fixed unused parameter warnings in $file."
done

echo "Unused parameter fix script completed."
