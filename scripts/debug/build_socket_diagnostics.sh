#!/bin/bash
# Build script for socket diagnostics tool

# Set working directory
cd /opt/jsondb

# Create build directory if needed
mkdir -p build/debug

# Compile socket diagnostics tool
gcc -Wall -Wextra -o build/debug/socket_diagnostics scripts/debug/socket_diagnostics.c

# Check if compilation was successful
if [ $? -eq 0 ]; then
    echo "Socket diagnostics tool built successfully at build/debug/socket_diagnostics"
    echo "Usage: ./build/debug/socket_diagnostics [--port=PORT] [--host=HOST]"
    
    # Run with default settings as a quick test
    ./build/debug/socket_diagnostics
else
    echo "Failed to build socket diagnostics tool"
fi