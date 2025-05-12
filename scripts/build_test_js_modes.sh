#!/bin/bash
# build_test_js_modes.sh
# Tests building the full project with and without JavaScript support

# ANSI color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== JavaScript Build Mode Test Script ===${NC}"
echo "This script will attempt to build the project with and without JavaScript support."
echo

# Function to build with specific JavaScript mode
build_with_js_mode() {
    local mode=$1
    
    if [ "$mode" == "enabled" ]; then
        echo -e "${BLUE}Building with JavaScript ENABLED (default mode)...${NC}"
        build_cmd="make clean && make"
    else
        echo -e "${BLUE}Building with JavaScript DISABLED...${NC}"
        build_cmd="make clean && make CFLAGS=-DDISABLE_JS"
    fi
    
    echo -e "${YELLOW}Running: $build_cmd${NC}"
    eval $build_cmd
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Build with JavaScript $mode succeeded!${NC}"
        return 0
    else
        echo -e "${RED}✗ Build with JavaScript $mode failed!${NC}"
        return 1
    fi
}

# Track overall success
success=true

# Test with JavaScript enabled (default)
build_with_js_mode "enabled"
if [ $? -ne 0 ]; then
    success=false
fi

echo

# Test with JavaScript disabled
build_with_js_mode "disabled"
if [ $? -ne 0 ]; then
    success=false
fi

echo
echo -e "${BLUE}=== JavaScript Build Mode Test Summary ===${NC}"

if $success; then
    echo -e "${GREEN}✓ All build tests passed! JavaScript conditional compilation is working correctly.${NC}"
    exit 0
else
    echo -e "${RED}✗ Some build tests failed. Please review the output for issues.${NC}"
    exit 1
fi