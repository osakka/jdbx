#!/bin/bash
# Simple script to test JavaScript conditional compilation

echo "=== JavaScript Conditional Compilation Test ==="
echo

# Test with JavaScript enabled
echo "Testing compilation WITH JavaScript support..."
gcc -Wall -Iinclude -DUSE_QUICKJS -o test_js_enabled /home/claude-3/project/test_js_compilation.c -lm
if [ $? -eq 0 ]; then
    echo -e "\033[0;32m✓ Build WITH JavaScript succeeded!\033[0m"
    echo "Running test binary:"
    ./test_js_enabled
    rm ./test_js_enabled
else
    echo -e "\033[0;31m✗ Build WITH JavaScript failed!\033[0m"
fi

echo 
echo "----------------------------------------------"
echo

# Test with JavaScript disabled
echo "Testing compilation WITHOUT JavaScript support..."
gcc -Wall -Iinclude -DDISABLE_JS -o test_js_disabled /home/claude-3/project/test_js_compilation.c -lm
if [ $? -eq 0 ]; then
    echo -e "\033[0;32m✓ Build WITHOUT JavaScript succeeded!\033[0m"
    echo "Running test binary:"
    ./test_js_disabled
    rm ./test_js_disabled
else
    echo -e "\033[0;31m✗ Build WITHOUT JavaScript failed!\033[0m"
fi

echo
echo "=== Test Completed ==="