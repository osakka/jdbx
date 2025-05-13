#!/bin/bash
# Script to consolidate redundant source files between src/ and src/components/
# This script will keep the newer/larger version of each file and remove the redundant copy

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Set the project root directory
PROJECT_ROOT="/home/claude-3/project"
SRC_DIR="$PROJECT_ROOT/src"
COMPONENT_DIR="$SRC_DIR/components"

echo -e "${GREEN}Starting source consolidation...${NC}"
echo -e "${YELLOW}Creating a log of all actions in consolidation_log.txt${NC}"

# Create a log file
LOG_FILE="$PROJECT_ROOT/consolidation_log.txt"
echo "Source Consolidation Log - $(date)" > "$LOG_FILE"
echo "=======================================" >> "$LOG_FILE"

# Function to determine the newer/better version of a file
choose_better_version() {
    local src_file="$1"
    local comp_file="$2"
    
    # If one of the files doesn't exist, choose the existing one
    if [ ! -f "$src_file" ]; then
        echo "$comp_file"
        return
    fi
    
    if [ ! -f "$comp_file" ]; then
        echo "$src_file"
        return
    fi
    
    # Compare modification times
    src_time=$(stat -c %Y "$src_file")
    comp_time=$(stat -c %Y "$comp_file")
    
    if [ "$src_time" -gt "$comp_time" ]; then
        echo "$src_file"
    elif [ "$comp_time" -gt "$src_time" ]; then
        echo "$comp_file"
    else
        # If timestamps are the same, compare file sizes
        src_size=$(stat -c %s "$src_file")
        comp_size=$(stat -c %s "$comp_file")
        
        if [ "$src_size" -gt "$comp_size" ]; then
            echo "$src_file"
        else
            # Default to the component version if sizes are equal or component is larger
            echo "$comp_file"
        fi
    fi
}

# Process files in each module
process_module() {
    local module="$1"
    local src_module_dir="$SRC_DIR/$module"
    local comp_module_dir="$COMPONENT_DIR/$module"
    
    echo -e "${BLUE}Processing module: $module${NC}"
    echo "Processing module: $module" >> "$LOG_FILE"
    
    # Skip if src module directory doesn't exist
    if [ ! -d "$src_module_dir" ]; then
        echo "  No $src_module_dir directory, skipping" >> "$LOG_FILE"
        return
    fi
    
    # Create component directory if it doesn't exist
    if [ ! -d "$comp_module_dir" ]; then
        mkdir -p "$comp_module_dir"
        echo "  Created directory: $comp_module_dir" >> "$LOG_FILE"
    fi
    
    # Process each .c file in the src module directory
    for src_file in "$src_module_dir"/*.c; do
        # Skip if no files match
        [ -e "$src_file" ] || continue
        
        filename=$(basename "$src_file")
        comp_file="$comp_module_dir/$filename"
        
        # Skip if the file is a backup file
        if [[ "$filename" == *".bak"* || "$filename" == *".original"* || "$filename" == *".fixed"* ]]; then
            echo "  Skipping backup file: $filename" >> "$LOG_FILE"
            continue
        fi
        
        echo -e "  Checking ${YELLOW}$filename${NC}"
        echo "  Checking $filename" >> "$LOG_FILE"
        
        # Check if both files exist and if they're different
        if [ -f "$comp_file" ] && ! cmp -s "$src_file" "$comp_file"; then
            better_file=$(choose_better_version "$src_file" "$comp_file")
            
            if [ "$better_file" = "$src_file" ]; then
                echo -e "    ${GREEN}Using src version (newer/larger)${NC}"
                echo "    Using src version (newer/larger): $src_file" >> "$LOG_FILE"
                cp "$src_file" "$comp_file"
            else
                echo -e "    ${GREEN}Using component version (newer/larger)${NC}"
                echo "    Using component version (newer/larger): $comp_file" >> "$LOG_FILE"
            fi
        elif [ -f "$comp_file" ]; then
            echo -e "    ${GREEN}Files are identical${NC}"
            echo "    Files are identical" >> "$LOG_FILE"
        else
            echo -e "    ${GREEN}Moving file to components directory${NC}"
            echo "    Moving file to components directory: $filename" >> "$LOG_FILE"
            cp "$src_file" "$comp_file"
        fi
    done
}

# Special handling for main.c
handle_main_file() {
    local src_main="$SRC_DIR/main.c"
    local core_main="$SRC_DIR/core/main.c"
    local comp_main="$COMPONENT_DIR/main.c"
    
    echo -e "${BLUE}Processing main.c${NC}"
    echo "Processing main.c" >> "$LOG_FILE"
    
    # If src/main.c exists
    if [ -f "$src_main" ]; then
        if [ -f "$comp_main" ]; then
            better_file=$(choose_better_version "$src_main" "$comp_main")
            
            if [ "$better_file" = "$src_main" ]; then
                echo -e "  ${GREEN}Using src/main.c (newer/larger)${NC}"
                echo "  Using src/main.c (newer/larger)" >> "$LOG_FILE"
                cp "$src_main" "$comp_main"
            else
                echo -e "  ${GREEN}Using components/main.c (newer/larger)${NC}"
                echo "  Using components/main.c (newer/larger)" >> "$LOG_FILE"
            fi
        else
            echo -e "  ${GREEN}Moving src/main.c to components/main.c${NC}"
            echo "  Moving src/main.c to components/main.c" >> "$LOG_FILE"
            cp "$src_main" "$comp_main"
        fi
    fi
    
    # If src/core/main.c exists
    if [ -f "$core_main" ]; then
        if [ -f "$comp_main" ]; then
            better_file=$(choose_better_version "$core_main" "$comp_main")
            
            if [ "$better_file" = "$core_main" ]; then
                echo -e "  ${GREEN}Using src/core/main.c (newer/larger)${NC}"
                echo "  Using src/core/main.c (newer/larger)" >> "$LOG_FILE"
                cp "$core_main" "$comp_main"
            else
                echo -e "  ${GREEN}Using components/main.c (newer/larger)${NC}"
                echo "  Using components/main.c (newer/larger)" >> "$LOG_FILE"
            fi
        else
            echo -e "  ${GREEN}Moving src/core/main.c to components/main.c${NC}"
            echo "  Moving src/core/main.c to components/main.c" >> "$LOG_FILE"
            cp "$core_main" "$comp_main"
        fi
    fi
}

# Process each module
modules=("api" "core" "database" "js" "query" "rbac" "transaction" "utils" "tools")
for module in "${modules[@]}"; do
    process_module "$module"
done

# Handle special case for main.c
handle_main_file

echo -e "${GREEN}Consolidation complete!${NC}"
echo -e "${YELLOW}See $LOG_FILE for details.${NC}"
echo -e "${RED}NOTE: This script has only copied files. You need to remove the redundant files manually after verification.${NC}"
echo "Consolidation complete." >> "$LOG_FILE"
echo "This script has only copied files, not removed any. Please remove redundant files manually after verification." >> "$LOG_FILE"