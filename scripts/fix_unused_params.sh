#!/bin/bash
# fix_unused_params.sh
# Script to fix the "unused parameter" warnings in the codebase

# ANSI color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== Fix Unused Parameters Script ===${NC}"
echo "This script adds (void) casts to silence unused parameter warnings."

# Function to process a file
fix_unused_params() {
    local file=$1
    local backup="${file}.unused_params_bak"
    
    echo -e "${YELLOW}Processing ${file}...${NC}"
    
    # Make a backup
    cp "$file" "$backup"
    
    # Find all unused parameter warnings
    local params=$(grep -n "\[-Wunused-parameter\]" "$backup" | sed -E 's/.*parameter '\''([^'\'']+)'\''.*/\1/')
    
    if [ -z "$params" ]; then
        echo "  - No unused parameters found"
        rm "$backup"
        return 0
    fi
    
    # Count how many parameters we found
    local param_count=$(echo "$params" | wc -l)
    echo "  - Found $param_count unused parameter(s)"
    
    # Process each parameter
    for param in $params; do
        # Find function that contains this parameter
        local func_lines=$(grep -n "^[a-zA-Z_][a-zA-Z0-9_]*[ \t]*[a-zA-Z_][a-zA-Z0-9_]*[ \t]*.*${param}" "$file" | cut -d: -f1)
        
        for line in $func_lines; do
            # Look for opening brace
            local brace_line=$(tail -n +$line "$file" | grep -n "{" | head -1 | cut -d: -f1)
            local actual_brace_line=$((line + brace_line - 1))
            
            # Check if we already have a (void) cast for this parameter
            local has_void_cast=$(tail -n +$actual_brace_line "$file" | head -10 | grep -c "(void)[ \t]*${param}")
            
            if [ $has_void_cast -eq 0 ]; then
                # Add (void) cast after the opening brace
                sed -i "${actual_brace_line}a\\    (void)${param}; /* Avoid unused parameter warning */" "$file"
                echo "  - Added (void) cast for parameter '${param}'"
            else
                echo "  - Parameter '${param}' already has a (void) cast"
            fi
        done
    done
    
    # Compare files to see if changes were made
    if diff -q "$file" "$backup" >/dev/null; then
        echo "  - No changes needed for $file"
        rm "$backup"
    else
        echo -e "  - ${GREEN}Successfully updated $file${NC}"
    fi
}

# Fix the signedness comparison in api.c
fix_signedness_comparison() {
    local file="/home/claude-3/project/src/core/api.c"
    local backup="${file}.sign_compare_bak"
    
    echo -e "${YELLOW}Fixing signedness comparison in ${file}...${NC}"
    
    # Make a backup
    cp "$file" "$backup"
    
    # Fix the specific line with the signedness issue
    sed -i 's/slash ? slash - path : strlen(path)/slash ? (size_t)(slash - path) : strlen(path)/g' "$file"
    
    # Compare files to see if changes were made
    if diff -q "$file" "$backup" >/dev/null; then
        echo "  - No changes needed for signedness comparison"
        rm "$backup"
    else
        echo -e "  - ${GREEN}Successfully fixed signedness comparison${NC}"
    fi
}

# Process core files
echo -e "\n${BLUE}Processing core files...${NC}"
for file in src/core/*.c; do
    fix_unused_params "$file"
done

# Process API files
echo -e "\n${BLUE}Processing API files...${NC}"
for file in src/api/*.c; do
    fix_unused_params "$file"
done

# Process other directories that might have warnings
echo -e "\n${BLUE}Processing database files...${NC}"
for file in src/database/*.c; do
    fix_unused_params "$file"
done

echo -e "\n${BLUE}Processing JavaScript files...${NC}"
for file in src/js/*.c src/js/utils/*.c; do
    fix_unused_params "$file"
done

echo -e "\n${BLUE}Processing transaction files...${NC}"
for file in src/transaction/*.c; do
    fix_unused_params "$file"
done

# Fix the signedness comparison in api.c
fix_signedness_comparison

echo -e "\n${GREEN}Completed fixing unused parameters!${NC}"
echo "You should now run a build to check if warnings have been reduced."