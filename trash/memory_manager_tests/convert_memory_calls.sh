#!/bin/bash

# Final bulletproof memory allocation conversion script
# This will convert ALL remaining direct memory calls to buffer pool calls

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Files to process
FILES_TO_CONVERT=(
    "/opt/jdbx/src/components/api/health_api.c"
    "/opt/jdbx/src/components/core/api.c"
    "/opt/jdbx/src/components/core/client_connection.c"
    "/opt/jdbx/src/components/core/thread_pool.c"
    "/opt/jdbx/src/components/database/batch_operations.c"
    "/opt/jdbx/src/components/database/database.c"
    "/opt/jdbx/src/components/database/index_cleanup.c"
    "/opt/jdbx/src/components/database/lock_manager.c"
    "/opt/jdbx/src/components/database/versioning_policy.c"
    "/opt/jdbx/src/components/lockfree/hazard_pointer.c"
    "/opt/jdbx/src/components/storage/jdbx_btree.c"
    "/opt/jdbx/src/components/storage/jdbx_page_manager.c"
    "/opt/jdbx/src/components/transaction/transaction.c"
    "/opt/jdbx/src/components/transaction/transaction_log.c"
    "/opt/jdbx/src/components/utils/generic_cache.c"
    "/opt/jdbx/src/components/utils/skiplist.c"
    "/opt/jdbx/src/include/utils/json_helpers.h"
    "/opt/jdbx/src/include/utils/memory_monitoring.h"
    "/opt/jdbx/src/initialize/config.c"
    "/opt/jdbx/src/initialize/logger_init.c"
)

REPORT_FILE="/opt/jdbx/memory_conversion_report.txt"
BACKUP_DIR="/opt/jdbx/backup_memory_$(date +%Y%m%d_%H%M%S)"

# Create backup directory
mkdir -p "$BACKUP_DIR"

# Initialize report
echo "Memory Call Conversion Report" > "$REPORT_FILE"
echo "=============================" >> "$REPORT_FILE"
echo "Date: $(date)" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"

# Function to check if buffer_pool.h is included
has_buffer_pool_include() {
    local file="$1"
    grep -q '#include.*buffer_pool\.h' "$file"
}

# Function to add buffer_pool.h include
add_buffer_pool_include() {
    local file="$1"
    
    if has_buffer_pool_include "$file"; then
        return 0
    fi
    
    # Add after the last include
    local temp_file="${file}.tmp"
    awk '
        BEGIN { last_include=0; added=0 }
        /^#include/ { last_include=NR }
        { lines[NR]=$0 }
        END {
            for (i=1; i<=NR; i++) {
                print lines[i]
                if (i == last_include && !added) {
                    print "#include \"utils/buffer_pool.h\""
                    added=1
                }
            }
            if (last_include == 0 && !added) {
                print "#include \"utils/buffer_pool.h\""
                print ""
            }
        }
    ' "$file" > "$temp_file"
    
    mv "$temp_file" "$file"
    echo "  Added buffer_pool.h include" >> "$REPORT_FILE"
}

# Function to convert a single file
convert_file() {
    local file="$1"
    local basename=$(basename "$file")
    
    echo -e "${BLUE}Processing: $basename${NC}"
    echo "" >> "$REPORT_FILE"
    echo "File: $file" >> "$REPORT_FILE"
    
    # Backup the file
    cp "$file" "$BACKUP_DIR/$basename"
    
    # Create temp file
    local temp_file="${file}.tmp"
    cp "$file" "$temp_file"
    
    # Track changes
    local changes_made=0
    
    # Convert malloc (including with casts)
    if grep -q 'malloc\s*(' "$temp_file"; then
        perl -i -pe 's/(\([^)]+\))?\s*malloc\s*\(/($1 ? "$1BUFFER_ALLOC(" : "BUFFER_ALLOC(")/ge' "$temp_file"
        echo "  malloc -> BUFFER_ALLOC" >> "$REPORT_FILE"
        changes_made=1
    fi
    
    # Convert free
    if grep -q '\bfree\s*(' "$temp_file"; then
        perl -i -pe 's/\bfree\s*\(/BUFFER_FREE(/g' "$temp_file"
        echo "  free -> BUFFER_FREE" >> "$REPORT_FILE"
        changes_made=1
    fi
    
    # Convert calloc
    if grep -q '\bcalloc\s*(' "$temp_file"; then
        perl -i -pe 's/(\([^)]+\))?\s*calloc\s*\(/($1 ? "$1BUFFER_CALLOC(" : "BUFFER_CALLOC(")/ge' "$temp_file"
        echo "  calloc -> BUFFER_CALLOC" >> "$REPORT_FILE"
        changes_made=1
    fi
    
    # Convert realloc
    if grep -q '\brealloc\s*(' "$temp_file"; then
        perl -i -pe 's/(\([^)]+\))?\s*realloc\s*\(/($1 ? "$1BUFFER_REALLOC(" : "BUFFER_REALLOC(")/ge' "$temp_file"
        echo "  realloc -> BUFFER_REALLOC" >> "$REPORT_FILE"
        changes_made=1
    fi
    
    # Convert strdup
    if grep -q '\bstrdup\s*(' "$temp_file"; then
        perl -i -pe 's/\bstrdup\s*\(/BUFFER_STRDUP(/g' "$temp_file"
        echo "  strdup -> BUFFER_STRDUP" >> "$REPORT_FILE"
        changes_made=1
    fi
    
    if [ $changes_made -eq 1 ]; then
        mv "$temp_file" "$file"
        add_buffer_pool_include "$file"
        echo -e "  ${GREEN}✓ Converted${NC}"
        return 0
    else
        rm "$temp_file"
        echo -e "  ${YELLOW}No changes needed${NC}"
        echo "  No direct memory calls found" >> "$REPORT_FILE"
        return 1
    fi
}

# Main conversion
echo -e "${GREEN}Starting targeted memory call conversion...${NC}"
echo ""

converted_count=0
for file in "${FILES_TO_CONVERT[@]}"; do
    if [ -f "$file" ]; then
        if convert_file "$file"; then
            converted_count=$((converted_count + 1))
        fi
    else
        echo -e "${RED}File not found: $file${NC}"
    fi
done

# Summary
echo "" >> "$REPORT_FILE"
echo "Summary:" >> "$REPORT_FILE"
echo "========" >> "$REPORT_FILE"
echo "Files processed: ${#FILES_TO_CONVERT[@]}" >> "$REPORT_FILE"
echo "Files converted: $converted_count" >> "$REPORT_FILE"
echo "Backup location: $BACKUP_DIR" >> "$REPORT_FILE"

# Final verification
echo "" >> "$REPORT_FILE"
echo "Remaining Direct Calls:" >> "$REPORT_FILE"
echo "======================" >> "$REPORT_FILE"

remaining=0
for file in "${FILES_TO_CONVERT[@]}"; do
    if [ -f "$file" ]; then
        if grep -q '\b\(malloc\|free\|calloc\|realloc\|strdup\)\s*(' "$file"; then
            echo "" >> "$REPORT_FILE"
            echo "In $file:" >> "$REPORT_FILE"
            grep -n '\b\(malloc\|free\|calloc\|realloc\|strdup\)\s*(' "$file" >> "$REPORT_FILE"
            remaining=$((remaining + 1))
        fi
    fi
done

if [ $remaining -eq 0 ]; then
    echo "None found - conversion complete!" >> "$REPORT_FILE"
fi

# Display summary
echo ""
echo -e "${GREEN}Conversion complete!${NC}"
echo "- Files converted: $converted_count"
echo "- Backup created: $BACKUP_DIR"
echo "- Report saved: $REPORT_FILE"

if [ $remaining -gt 0 ]; then
    echo ""
    echo -e "${YELLOW}WARNING: $remaining files still have direct memory calls${NC}"
    echo "Check the report for details."
fi

echo ""
echo "Next step: Compile and test the changes"
echo "  cd /opt/jdbx/src && make clean && make"