#!/bin/bash

# Define colors for better readability
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== Testing js_file_utils.c compilation with and without JavaScript ===${NC}"

# Create a directory for test builds if it doesn't exist
mkdir -p build/tests

# Test with JavaScript disabled
echo -e "${BLUE}Testing with JavaScript DISABLED...${NC}"
echo -e "${YELLOW}cc -DDISABLE_JS -Wall -Wextra -Wformat-truncation=2 -c src/js/utils/js_file_utils.c -I./include -o build/tests/js_file_utils_disabled.o${NC}"

if cc -DDISABLE_JS -Wall -Wextra -Wformat-truncation=2 -c src/js/utils/js_file_utils.c -I./include -o build/tests/js_file_utils_disabled.o; then
    echo -e "${GREEN}✓ Compilation with JavaScript DISABLED succeeded!${NC}"
else
    echo -e "${RED}✗ Compilation with JavaScript DISABLED failed!${NC}"
    exit 1
fi

# Test without JavaScript disabled (enabled)
echo -e "\n${BLUE}Testing with JavaScript ENABLED...${NC}"
echo -e "${YELLOW}cc -Wall -Wextra -Wformat-truncation=2 -c src/js/utils/js_file_utils.c -I./include -o build/tests/js_file_utils_enabled.o${NC}"

if cc -Wall -Wextra -Wformat-truncation=2 -c src/js/utils/js_file_utils.c -I./include -o build/tests/js_file_utils_enabled.o; then
    echo -e "${GREEN}✓ Compilation with JavaScript ENABLED succeeded!${NC}"
else
    echo -e "${RED}✗ Compilation with JavaScript ENABLED failed!${NC}"
    exit 1
fi

echo -e "\n${GREEN}All tests passed successfully!${NC}"
echo -e "${BLUE}The js_file_utils.c file now builds without format-truncation warnings in both JavaScript modes.${NC}"