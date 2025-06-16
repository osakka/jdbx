#!/bin/bash

# Convert direct memory allocation calls to buffer pool calls
# This script will find and replace all malloc/free/calloc/realloc/strdup calls
# with their BUFFER_* equivalents, except in memory_manager.c

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Report file
REPORT_FILE="/opt/jdbx/buffer_pool_conversion_report.txt"
TEMP_DIR="/tmp/buffer_pool_conversion_$$"
mkdir -p "$TEMP_DIR"

# Initialize report
echo "Buffer Pool Conversion Report" > "$REPORT_FILE"
echo "=============================" >> "$REPORT_FILE"
echo "Date: $(date)" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"

# Function to check if file should be skipped
should_skip_file() {
    local file="$1"
    # Skip memory_manager.c as it implements the buffer pool
    if [[ "$file" == *"memory_manager.c" ]]; then
        return 0
    fi
    # Skip buffer_pool.h itself
    if [[ "$file" == *"buffer_pool.h" ]]; then
        return 0
    fi
    return 1
}

# Function to check if buffer_pool.h is included
has_buffer_pool_include() {
    local file="$1"
    grep -q '#include.*buffer_pool\.h' "$file" || \
    grep -q '#include.*"buffer_pool\.h"' "$file" || \
    grep -q '#include.*<buffer_pool\.h>' "$file"
}

# Function to add buffer_pool.h include if needed
add_buffer_pool_include() {
    local file="$1"
    local temp_file="$TEMP_DIR/$(basename "$file").tmp"
    
    # Find the last include statement
    local last_include_line=$(grep -n "^#include" "$file" | tail -1 | cut -d: -f1)
    
    if [ -z "$last_include_line" ]; then
        # No includes found, add after any initial comments
        local first_code_line=$(grep -n -v "^/\*\|^\*/\|^ \*\|^$" "$file" | head -1 | cut -d: -f1)
        if [ -z "$first_code_line" ]; then
            first_code_line=1
        fi
        head -n $((first_code_line - 1)) "$file" > "$temp_file"
        echo '#include "utils/buffer_pool.h"' >> "$temp_file"
        echo "" >> "$temp_file"
        tail -n +$first_code_line "$file" >> "$temp_file"
    else
        # Add after the last include
        head -n "$last_include_line" "$file" > "$temp_file"
        echo '#include "utils/buffer_pool.h"' >> "$temp_file"
        tail -n +$((last_include_line + 1)) "$file" >> "$temp_file"
    fi
    
    mv "$temp_file" "$file"
    echo "  Added #include \"utils/buffer_pool.h\" to $file" >> "$REPORT_FILE"
}

# Function to convert memory calls in a file
convert_file() {
    local file="$1"
    local temp_file="$TEMP_DIR/$(basename "$file").tmp"
    local changes_made=0
    
    # Create a backup
    cp "$file" "$temp_file"
    
    # Track changes for this file
    local file_changes=""
    
    # Convert malloc to BUFFER_ALLOC
    if grep -q '\bmalloc\s*(' "$file"; then
        sed -i 's/\bmalloc\s*(/BUFFER_ALLOC(/g' "$file"
        file_changes="${file_changes}  malloc -> BUFFER_ALLOC\n"
        changes_made=1
    fi
    
    # Convert free to BUFFER_FREE
    if grep -q '\bfree\s*(' "$file"; then
        sed -i 's/\bfree\s*(/BUFFER_FREE(/g' "$file"
        file_changes="${file_changes}  free -> BUFFER_FREE\n"
        changes_made=1
    fi
    
    # Convert calloc to BUFFER_CALLOC
    if grep -q '\bcalloc\s*(' "$file"; then
        sed -i 's/\bcalloc\s*(/BUFFER_CALLOC(/g' "$file"
        file_changes="${file_changes}  calloc -> BUFFER_CALLOC\n"
        changes_made=1
    fi
    
    # Convert realloc to BUFFER_REALLOC
    if grep -q '\brealloc\s*(' "$file"; then
        sed -i 's/\brealloc\s*(/BUFFER_REALLOC(/g' "$file"
        file_changes="${file_changes}  realloc -> BUFFER_REALLOC\n"
        changes_made=1
    fi
    
    # Convert strdup to BUFFER_STRDUP
    if grep -q '\bstrdup\s*(' "$file"; then
        sed -i 's/\bstrdup\s*(/BUFFER_STRDUP(/g' "$file"
        file_changes="${file_changes}  strdup -> BUFFER_STRDUP\n"
        changes_made=1
    fi
    
    if [ $changes_made -eq 1 ]; then
        echo "" >> "$REPORT_FILE"
        echo "File: $file" >> "$REPORT_FILE"
        echo -e "$file_changes" >> "$REPORT_FILE"
        
        # Check if buffer_pool.h needs to be added
        if ! has_buffer_pool_include "$file"; then
            add_buffer_pool_include "$file"
        fi
        
        return 0
    else
        # Restore original if no changes
        mv "$temp_file" "$file"
        return 1
    fi
}

# Main conversion process
echo -e "${GREEN}Starting buffer pool conversion...${NC}"
echo "" >> "$REPORT_FILE"
echo "Files Processed:" >> "$REPORT_FILE"
echo "================" >> "$REPORT_FILE"

total_files=0
converted_files=0
skipped_files=0

# Find all C and H files
find /opt/jdbx/src -name "*.c" -o -name "*.h" | while read -r file; do
    total_files=$((total_files + 1))
    
    if should_skip_file "$file"; then
        echo -e "${YELLOW}Skipping: $file${NC}"
        echo "SKIPPED: $file (exempted file)" >> "$REPORT_FILE"
        skipped_files=$((skipped_files + 1))
        continue
    fi
    
    # Check if file contains any memory allocation calls
    if grep -q '\b\(malloc\|free\|calloc\|realloc\|strdup\)\s*(' "$file"; then
        echo -e "${GREEN}Converting: $file${NC}"
        if convert_file "$file"; then
            converted_files=$((converted_files + 1))
        fi
    fi
done

# Summary statistics
echo "" >> "$REPORT_FILE"
echo "Summary:" >> "$REPORT_FILE"
echo "========" >> "$REPORT_FILE"

# Count actual results
actual_converted=$(grep -c "^File: " "$REPORT_FILE" || true)
actual_skipped=$(grep -c "^SKIPPED: " "$REPORT_FILE" || true)

echo "Total files scanned: $(find /opt/jdbx/src -name "*.c" -o -name "*.h" | wc -l)" >> "$REPORT_FILE"
echo "Files converted: $actual_converted" >> "$REPORT_FILE"
echo "Files skipped: $actual_skipped" >> "$REPORT_FILE"

# Find any remaining direct calls (for verification)
echo "" >> "$REPORT_FILE"
echo "Verification - Remaining Direct Calls:" >> "$REPORT_FILE"
echo "=====================================" >> "$REPORT_FILE"

remaining_found=0
find /opt/jdbx/src -name "*.c" -o -name "*.h" | while read -r file; do
    if should_skip_file "$file"; then
        continue
    fi
    
    if grep -q '\b\(malloc\|free\|calloc\|realloc\|strdup\)\s*(' "$file"; then
        echo "" >> "$REPORT_FILE"
        echo "WARNING: $file still contains direct memory calls:" >> "$REPORT_FILE"
        grep -n '\b\(malloc\|free\|calloc\|realloc\|strdup\)\s*(' "$file" >> "$REPORT_FILE" || true
        remaining_found=1
    fi
done

if [ $remaining_found -eq 0 ]; then
    echo "SUCCESS: No remaining direct memory allocation calls found!" >> "$REPORT_FILE"
fi

# Clean up
rm -rf "$TEMP_DIR"

echo -e "${GREEN}Conversion complete! Report saved to: $REPORT_FILE${NC}"
echo ""
echo "Quick Stats:"
echo "- Files converted: $actual_converted"
echo "- Files skipped: $actual_skipped"
echo ""
echo "To verify the changes, run:"
echo "  grep -r '\\b\\(malloc\\|free\\|calloc\\|realloc\\|strdup\\)\\s*(' /opt/jdbx/src --include='*.c' --include='*.h' | grep -v memory_manager.c"