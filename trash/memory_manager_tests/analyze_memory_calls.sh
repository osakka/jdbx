#!/bin/bash

# Analyze current memory allocation usage in the codebase
# This script provides detailed statistics before conversion

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}=== JDBX Memory Allocation Analysis ===${NC}"
echo ""

# Count total files
total_c_files=$(find /opt/jdbx/src -name "*.c" | wc -l)
total_h_files=$(find /opt/jdbx/src -name "*.h" | wc -l)
echo -e "${BLUE}Total source files:${NC}"
echo "  C files: $total_c_files"
echo "  H files: $total_h_files"
echo ""

# Analyze each memory function
echo -e "${BLUE}Memory allocation usage by function:${NC}"
for func in malloc free calloc realloc strdup; do
    count=$(grep -r "\b${func}\s*(" /opt/jdbx/src --include="*.c" --include="*.h" | wc -l)
    files=$(grep -r "\b${func}\s*(" /opt/jdbx/src --include="*.c" --include="*.h" | cut -d: -f1 | sort -u | wc -l)
    echo "  $func(): $count calls in $files files"
done
echo ""

# Find files with most allocations
echo -e "${BLUE}Top 10 files by memory allocation calls:${NC}"
grep -r '\b\(malloc\|free\|calloc\|realloc\|strdup\)\s*(' /opt/jdbx/src --include="*.c" --include="*.h" | \
    cut -d: -f1 | sort | uniq -c | sort -rn | head -10 | \
    while read count file; do
        echo "  $count calls in $(basename $file)"
    done
echo ""

# Check for existing buffer pool usage
echo -e "${BLUE}Current buffer pool usage:${NC}"
buffer_files=$(grep -r "BUFFER_\(ALLOC\|FREE\|CALLOC\|REALLOC\|STRDUP\)" /opt/jdbx/src --include="*.c" --include="*.h" | cut -d: -f1 | sort -u | wc -l)
buffer_calls=$(grep -r "BUFFER_\(ALLOC\|FREE\|CALLOC\|REALLOC\|STRDUP\)" /opt/jdbx/src --include="*.c" --include="*.h" | wc -l)
echo "  Files using buffer pool: $buffer_files"
echo "  Total buffer pool calls: $buffer_calls"
echo ""

# Check for buffer_pool.h includes
echo -e "${BLUE}Files including buffer_pool.h:${NC}"
include_count=$(grep -r '#include.*buffer_pool\.h' /opt/jdbx/src --include="*.c" --include="*.h" | wc -l)
echo "  $include_count files"
echo ""

# Find problematic patterns
echo -e "${YELLOW}Potential issues to watch for:${NC}"

# Cast before malloc
cast_malloc=$(grep -r ')\s*malloc\s*(' /opt/jdbx/src --include="*.c" --include="*.h" | wc -l)
if [ $cast_malloc -gt 0 ]; then
    echo "  - Found $cast_malloc malloc calls with casts"
fi

# Multi-line malloc calls
multiline=$(grep -A1 -r 'malloc\s*($' /opt/jdbx/src --include="*.c" --include="*.h" | grep -v "^--$" | wc -l)
if [ $multiline -gt 0 ]; then
    echo "  - Found potential multi-line malloc calls"
fi

# Free in macros
macro_free=$(grep -r '#define.*free\s*(' /opt/jdbx/src --include="*.c" --include="*.h" | wc -l)
if [ $macro_free -gt 0 ]; then
    echo "  - Found $macro_free macros using free()"
fi

echo ""

# Files that will be skipped
echo -e "${BLUE}Files that will be skipped:${NC}"
for file in memory_manager.c buffer_pool.c buffer_pool.h; do
    found=$(find /opt/jdbx/src -name "$file" | wc -l)
    if [ $found -gt 0 ]; then
        echo "  ✓ $file"
    fi
done
echo ""

# Estimate conversion impact
echo -e "${GREEN}Conversion impact estimate:${NC}"
files_to_convert=$(grep -r '\b\(malloc\|free\|calloc\|realloc\|strdup\)\s*(' /opt/jdbx/src --include="*.c" --include="*.h" | \
    grep -v "memory_manager.c" | cut -d: -f1 | sort -u | wc -l)
calls_to_convert=$(grep -r '\b\(malloc\|free\|calloc\|realloc\|strdup\)\s*(' /opt/jdbx/src --include="*.c" --include="*.h" | \
    grep -v "memory_manager.c" | wc -l)

echo "  Files to be modified: $files_to_convert"
echo "  Memory calls to convert: $calls_to_convert"
echo ""

# Suggest next steps
echo -e "${CYAN}Next steps:${NC}"
echo "1. Review this analysis"
echo "2. Run a dry-run: ./convert_to_buffer_pool_advanced.sh true"
echo "3. Perform actual conversion: ./convert_to_buffer_pool_advanced.sh"
echo "4. Compile and test the converted code"
echo "5. Check the conversion report for any issues"