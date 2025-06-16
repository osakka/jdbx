#!/bin/bash

# Verify buffer pool conversion completeness
# Run this after conversion to ensure no direct memory calls remain

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}=== Buffer Pool Conversion Verification ===${NC}"
echo ""

# Function to check for remaining direct calls
check_remaining_calls() {
    local func=$1
    local results=$(grep -r "\b${func}\s*(" /opt/jdbx/src --include="*.c" --include="*.h" | grep -v "memory_manager.c" | grep -v "buffer_pool")
    
    if [ -n "$results" ]; then
        echo -e "${RED}WARNING: Found remaining ${func}() calls:${NC}"
        echo "$results" | while IFS=: read -r file line; do
            echo "  $(basename $file):$line"
        done | head -10
        local count=$(echo "$results" | wc -l)
        if [ $count -gt 10 ]; then
            echo "  ... and $((count - 10)) more"
        fi
        return 1
    fi
    return 0
}

# Check each function
echo -e "${BLUE}Checking for remaining direct memory calls...${NC}"
all_clear=true

for func in malloc free calloc realloc strdup; do
    echo -n "Checking $func()... "
    if check_remaining_calls $func; then
        echo -e "${GREEN}✓ CLEAR${NC}"
    else
        all_clear=false
    fi
    echo ""
done

# Check for buffer pool includes
echo -e "${BLUE}Checking buffer pool header inclusion...${NC}"
files_using_buffer=$(grep -r "BUFFER_\(ALLOC\|FREE\|CALLOC\|REALLOC\|STRDUP\)" /opt/jdbx/src --include="*.c" --include="*.h" | cut -d: -f1 | sort -u)
missing_includes=0

for file in $files_using_buffer; do
    if ! grep -q '#include.*buffer_pool\.h' "$file"; then
        echo -e "${YELLOW}Missing include in: $(basename $file)${NC}"
        missing_includes=$((missing_includes + 1))
    fi
done

if [ $missing_includes -eq 0 ]; then
    echo -e "${GREEN}✓ All files using buffer pool have the required include${NC}"
else
    echo -e "${RED}Found $missing_includes files missing buffer_pool.h include${NC}"
fi
echo ""

# Check for mixed usage (both direct and buffer pool)
echo -e "${BLUE}Checking for mixed usage patterns...${NC}"
mixed_files=0

find /opt/jdbx/src -name "*.c" -o -name "*.h" | while read -r file; do
    if [[ "$file" == *"memory_manager.c"* ]]; then
        continue
    fi
    
    has_direct=$(grep -q '\b\(malloc\|free\|calloc\|realloc\|strdup\)\s*(' "$file" && echo 1 || echo 0)
    has_buffer=$(grep -q 'BUFFER_\(ALLOC\|FREE\|CALLOC\|REALLOC\|STRDUP\)' "$file" && echo 1 || echo 0)
    
    if [ "$has_direct" -eq 1 ] && [ "$has_buffer" -eq 1 ]; then
        echo -e "${YELLOW}Mixed usage in: $(basename $file)${NC}"
        mixed_files=$((mixed_files + 1))
    fi
done

if [ $mixed_files -eq 0 ]; then
    echo -e "${GREEN}✓ No files with mixed usage patterns${NC}"
fi
echo ""

# Compilation test
echo -e "${BLUE}Testing compilation...${NC}"
cd /opt/jdbx/src
if make clean >/dev/null 2>&1 && make -j$(nproc) >/dev/null 2>&1; then
    echo -e "${GREEN}✓ Code compiles successfully${NC}"
else
    echo -e "${RED}✗ Compilation failed${NC}"
    echo "Run 'cd /opt/jdbx/src && make' to see errors"
fi
echo ""

# Summary
echo -e "${BLUE}=== Summary ===${NC}"
if [ "$all_clear" = true ] && [ $missing_includes -eq 0 ] && [ $mixed_files -eq 0 ]; then
    echo -e "${GREEN}✓ Buffer pool conversion appears complete and successful!${NC}"
    echo ""
    echo "Next steps:"
    echo "1. Run comprehensive tests"
    echo "2. Monitor memory usage with the buffer pool statistics"
    echo "3. Check for any performance impacts"
else
    echo -e "${YELLOW}⚠ Some issues were found that need attention${NC}"
    echo ""
    echo "Please review the warnings above and:"
    echo "1. Fix any remaining direct memory calls"
    echo "2. Add missing buffer_pool.h includes"
    echo "3. Resolve any mixed usage patterns"
fi

# Show buffer pool statistics if available
if command -v /opt/jdbx/build/bin/jdbxd >/dev/null 2>&1; then
    echo ""
    echo -e "${BLUE}Buffer Pool Statistics:${NC}"
    echo "Run the server and check /stats endpoint for buffer pool metrics"
fi