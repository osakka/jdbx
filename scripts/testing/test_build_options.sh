#!/bin/bash
# Test building with and without JavaScript support
# This script tests both compilation modes to ensure they work correctly

echo "=== JSONdb Build Test Script ==="
echo "This script tests building with and without JavaScript support"
echo ""

# Store the current directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Clean the build artifacts
clean_build() {
    echo -e "${YELLOW}Cleaning build artifacts...${NC}"
    make clean
    rm -f test_server
    echo "Done cleaning."
    echo ""
}

# Build with JavaScript support (default)
build_with_js() {
    echo -e "${YELLOW}Building with JavaScript support...${NC}"
    make
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Build with JavaScript succeeded!${NC}"
        return 0
    else
        echo -e "${RED}✗ Build with JavaScript failed!${NC}"
        return 1
    fi
}

# Build without JavaScript support
build_without_js() {
    echo -e "${YELLOW}Building without JavaScript support...${NC}"
    make DISABLE_QUICKJS=1
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Build without JavaScript succeeded!${NC}"
        return 0
    else
        echo -e "${RED}✗ Build without JavaScript failed!${NC}"
        return 1
    fi
}

# Print compiler warnings
check_warnings() {
    echo -e "${YELLOW}Checking for compiler warnings...${NC}"
    WARNINGS=$(make DISABLE_QUICKJS=1 2>&1 | grep -i warning | wc -l)
    echo -e "Found $WARNINGS compiler warnings."
    if [ $WARNINGS -gt 0 ]; then
        echo -e "${YELLOW}Top 10 warnings:${NC}"
        make DISABLE_QUICKJS=1 2>&1 | grep -i warning | head -10
    fi
    echo ""
}

# Main test function
run_tests() {
    # Start with a clean build
    clean_build
    
    # Test building with JavaScript support
    build_with_js
    JS_BUILD_RESULT=$?
    
    # Clean again
    clean_build
    
    # Test building without JavaScript support
    build_without_js
    NO_JS_BUILD_RESULT=$?
    
    # Print results
    echo ""
    echo "=== Build Test Results ==="
    if [ $JS_BUILD_RESULT -eq 0 ]; then
        echo -e "${GREEN}✓ Build WITH JavaScript: SUCCESS${NC}"
    else
        echo -e "${RED}✗ Build WITH JavaScript: FAILED${NC}"
    fi
    
    if [ $NO_JS_BUILD_RESULT -eq 0 ]; then
        echo -e "${GREEN}✓ Build WITHOUT JavaScript: SUCCESS${NC}"
    else
        echo -e "${RED}✗ Build WITHOUT JavaScript: FAILED${NC}"
    fi
    
    echo ""
    if [ $JS_BUILD_RESULT -eq 0 ] && [ $NO_JS_BUILD_RESULT -eq 0 ]; then
        echo -e "${GREEN}All builds succeeded!${NC}"
        return 0
    else
        echo -e "${RED}Some builds failed. See details above.${NC}"
        return 1
    fi
}

# Check for compiler warnings
check_warnings

# Run the main test
run_tests
exit_code=$?

echo ""
echo "=== Build Test Completed ==="
echo "Remember to fix any remaining warnings!"

exit $exit_code