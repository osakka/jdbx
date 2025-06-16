#!/bin/bash

# Advanced buffer pool conversion script with enhanced pattern matching
# Handles complex cases like multi-line calls, casts, and conditional allocations

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
REPORT_FILE="/opt/jdbx/buffer_pool_conversion_report_advanced.txt"
BACKUP_DIR="/opt/jdbx/backup_before_buffer_pool_$(date +%Y%m%d_%H%M%S)"
TEMP_DIR="/tmp/buffer_pool_conversion_$$"
DRY_RUN=${1:-false}

# Create directories
mkdir -p "$TEMP_DIR"
if [ "$DRY_RUN" != "true" ]; then
    mkdir -p "$BACKUP_DIR"
fi

# Initialize report
echo "Advanced Buffer Pool Conversion Report" > "$REPORT_FILE"
echo "======================================" >> "$REPORT_FILE"
echo "Date: $(date)" >> "$REPORT_FILE"
echo "Mode: $([ "$DRY_RUN" = "true" ] && echo "DRY RUN" || echo "ACTUAL CONVERSION")" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"

# Files to skip
SKIP_FILES=(
    "memory_manager.c"
    "buffer_pool.h"
    "buffer_pool.c"
)

# Function to check if file should be skipped
should_skip_file() {
    local file="$1"
    local basename=$(basename "$file")
    
    for skip in "${SKIP_FILES[@]}"; do
        if [[ "$basename" == "$skip" ]]; then
            return 0
        fi
    done
    return 1
}

# Function to create backup
backup_file() {
    local file="$1"
    if [ "$DRY_RUN" != "true" ]; then
        local rel_path="${file#/opt/jdbx/}"
        local backup_path="$BACKUP_DIR/$rel_path"
        mkdir -p "$(dirname "$backup_path")"
        cp "$file" "$backup_path"
    fi
}

# Function to check if buffer_pool.h is included
has_buffer_pool_include() {
    local file="$1"
    grep -q '#include.*buffer_pool\.h' "$file"
}

# Function to add buffer_pool.h include intelligently
add_buffer_pool_include() {
    local file="$1"
    local temp_file="$TEMP_DIR/$(basename "$file").tmp"
    
    # Don't add if already present
    if has_buffer_pool_include "$file"; then
        return 0
    fi
    
    # Find the best place to add the include
    local added=false
    
    # Try to add after other utils includes
    if grep -q '#include.*utils/' "$file"; then
        awk '
            /#include.*utils\// { utils_found=1; print }
            !/#include.*utils\// && utils_found && !added {
                print "#include \"utils/buffer_pool.h\"";
                added=1;
                utils_found=0;
                print
            }
            !/#include.*utils\// && !utils_found { print }
        ' "$file" > "$temp_file"
        added=true
    # Otherwise add after the last include
    elif grep -q '^#include' "$file"; then
        awk '
            BEGIN { last_include=0 }
            /^#include/ { last_include=NR; includes[NR]=$0 }
            { lines[NR]=$0 }
            END {
                for (i=1; i<=NR; i++) {
                    print lines[i]
                    if (i == last_include) {
                        print "#include \"utils/buffer_pool.h\""
                    }
                }
            }
        ' "$file" > "$temp_file"
        added=true
    else
        # Add at the beginning after initial comments
        awk '
            BEGIN { added=0 }
            /^\/\*/ { in_comment=1 }
            /\*\// { in_comment=0; print; next }
            !in_comment && !added && !/^$/ && !/^\/\// {
                print "#include \"utils/buffer_pool.h\""
                print ""
                added=1
            }
            { print }
        ' "$file" > "$temp_file"
        added=true
    fi
    
    if [ "$added" = "true" ] && [ "$DRY_RUN" != "true" ]; then
        mv "$temp_file" "$file"
        echo "  Added #include \"utils/buffer_pool.h\"" >> "$REPORT_FILE"
    else
        rm -f "$temp_file"
    fi
}

# Advanced pattern conversion with context awareness
convert_file_advanced() {
    local file="$1"
    local temp_file="$TEMP_DIR/$(basename "$file").tmp"
    local changes_made=0
    local conversions=""
    
    # Create working copy
    cp "$file" "$temp_file"
    
    # Use perl for more sophisticated pattern matching
    perl -i -pe '
        # Skip commented lines
        next if /^\s*\/\//;
        next if /^\s*\*/;
        
        # Handle malloc with casts
        s/\(\s*(\w+\s*\*+)\s*\)\s*malloc\s*\(/($1)BUFFER_ALLOC(/g;
        
        # Handle simple malloc
        s/\bmalloc\s*\(/BUFFER_ALLOC(/g;
        
        # Handle free (be careful with "free" in other contexts)
        s/\bfree\s*\(/BUFFER_FREE(/g;
        
        # Handle calloc
        s/\bcalloc\s*\(/BUFFER_CALLOC(/g;
        
        # Handle realloc
        s/\brealloc\s*\(/BUFFER_REALLOC(/g;
        
        # Handle strdup
        s/\bstrdup\s*\(/BUFFER_STRDUP(/g;
    ' "$temp_file"
    
    # Check if any changes were made
    if ! diff -q "$file" "$temp_file" > /dev/null 2>&1; then
        changes_made=1
        
        # Generate detailed change report
        echo "" >> "$REPORT_FILE"
        echo "File: $file" >> "$REPORT_FILE"
        echo "Changes:" >> "$REPORT_FILE"
        
        # Show specific changes
        diff -u "$file" "$temp_file" | grep -E "^[-+].*\b(malloc|free|calloc|realloc|strdup|BUFFER_)" | while read -r line; do
            if [[ "$line" =~ ^- ]]; then
                echo "  REMOVED: ${line:1}" >> "$REPORT_FILE"
            elif [[ "$line" =~ ^\+ ]]; then
                echo "  ADDED:   ${line:1}" >> "$REPORT_FILE"
            fi
        done
        
        if [ "$DRY_RUN" != "true" ]; then
            backup_file "$file"
            mv "$temp_file" "$file"
            add_buffer_pool_include "$file"
        else
            rm "$temp_file"
        fi
    else
        rm "$temp_file"
    fi
    
    return $changes_made
}

# Function to verify conversion
verify_conversion() {
    local file="$1"
    local issues=""
    
    # Check for remaining direct calls
    if grep -q '\b\(malloc\|free\|calloc\|realloc\|strdup\)\s*(' "$file" 2>/dev/null; then
        # Double-check it's not in a comment or string
        if grep -v '^\s*//' "$file" | grep -v '^\s*\*' | grep -q '\b\(malloc\|free\|calloc\|realloc\|strdup\)\s*('; then
            issues="Still contains direct memory calls"
        fi
    fi
    
    # Check if uses BUFFER_* but missing include
    if grep -q '\bBUFFER_\(ALLOC\|FREE\|CALLOC\|REALLOC\|STRDUP\)\s*(' "$file" 2>/dev/null; then
        if ! has_buffer_pool_include "$file"; then
            issues="${issues:+$issues; }Missing buffer_pool.h include"
        fi
    fi
    
    echo "$issues"
}

# Main conversion process
echo -e "${GREEN}Starting advanced buffer pool conversion...${NC}"
if [ "$DRY_RUN" = "true" ]; then
    echo -e "${YELLOW}Running in DRY RUN mode - no files will be modified${NC}"
fi

echo "" >> "$REPORT_FILE"
echo "Files Processed:" >> "$REPORT_FILE"
echo "================" >> "$REPORT_FILE"

total_files=0
converted_files=0
skipped_files=0
error_files=0

# Process all C and H files
while IFS= read -r -d '' file; do
    total_files=$((total_files + 1))
    
    # Skip exempted files
    if should_skip_file "$file"; then
        echo -e "${YELLOW}Skipping: $file${NC}"
        echo "SKIPPED: $file (exempted file)" >> "$REPORT_FILE"
        skipped_files=$((skipped_files + 1))
        continue
    fi
    
    # Check if file contains any memory allocation calls
    if grep -q '\b\(malloc\|free\|calloc\|realloc\|strdup\)\s*(' "$file" 2>/dev/null; then
        echo -e "${BLUE}Analyzing: $file${NC}"
        
        if convert_file_advanced "$file"; then
            converted_files=$((converted_files + 1))
            echo -e "${GREEN}  ✓ Converted${NC}"
        else
            echo -e "  - No changes needed"
        fi
    fi
done < <(find /opt/jdbx/src -type f \( -name "*.c" -o -name "*.h" \) -print0)

# Verification pass
echo "" >> "$REPORT_FILE"
echo "Verification Results:" >> "$REPORT_FILE"
echo "====================" >> "$REPORT_FILE"

issues_found=0
while IFS= read -r -d '' file; do
    if should_skip_file "$file"; then
        continue
    fi
    
    issues=$(verify_conversion "$file")
    if [ -n "$issues" ]; then
        echo "WARNING in $file: $issues" >> "$REPORT_FILE"
        echo -e "${RED}WARNING in $file: $issues${NC}"
        issues_found=$((issues_found + 1))
    fi
done < <(find /opt/jdbx/src -type f \( -name "*.c" -o -name "*.h" \) -print0)

# Summary
echo "" >> "$REPORT_FILE"
echo "Summary:" >> "$REPORT_FILE"
echo "========" >> "$REPORT_FILE"
echo "Total files scanned: $total_files" >> "$REPORT_FILE"
echo "Files converted: $converted_files" >> "$REPORT_FILE"
echo "Files skipped: $skipped_files" >> "$REPORT_FILE"
echo "Files with issues: $issues_found" >> "$REPORT_FILE"

if [ "$DRY_RUN" != "true" ]; then
    echo "Backup created at: $BACKUP_DIR" >> "$REPORT_FILE"
fi

# Clean up
rm -rf "$TEMP_DIR"

# Final summary
echo ""
echo -e "${GREEN}Conversion complete!${NC}"
echo "Report saved to: $REPORT_FILE"
echo ""
echo "Summary:"
echo "- Total files scanned: $total_files"
echo "- Files converted: $converted_files"
echo "- Files skipped: $skipped_files"
echo "- Files with issues: $issues_found"

if [ "$DRY_RUN" != "true" ] && [ -d "$BACKUP_DIR" ]; then
    echo ""
    echo -e "${BLUE}Backup created at: $BACKUP_DIR${NC}"
    echo "To restore: cp -r $BACKUP_DIR/* /opt/jdbx/"
fi

if [ $issues_found -gt 0 ]; then
    echo ""
    echo -e "${RED}WARNING: Some files still have issues. Check the report for details.${NC}"
    exit 1
fi

exit 0