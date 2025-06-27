#!/bin/bash
# Inspector Clouseau's SSL Investigation Runner
# "Ah! We must examine ze crime scene with ze utmost precision!"

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
JDBX_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$JDBX_ROOT/build"

echo "🕵️ Inspector Claude's SSL Memory Corruption Investigation"
echo "========================================================"

# Create investigation directory
mkdir -p "$BUILD_DIR/investigation"

echo "🔧 Compiling ze detective tools..."
cd "$JDBX_ROOT/src"

# Compile with SSL and memory management
gcc -std=c11 -O1 -g -Wall -Wextra \
    -I"$JDBX_ROOT/src/include" \
    -DJDBX_ENABLE_EXOTIC_ALLOCATORS=1 \
    -DJDBX_ENABLE_ARENA_ALLOCATOR=1 \
    -DJDBX_ENABLE_TLSF_ALLOCATOR=1 \
    "$SCRIPT_DIR/inspector_clouseau_ssl_detective.c" \
    components/utils/memory_manager.c \
    components/utils/arena_allocator.c \
    components/utils/tlsf_allocator.c \
    components/utils/memory_allocator_config.c \
    -o "$BUILD_DIR/investigation/ssl_detective" \
    -lssl -lcrypto -pthread

echo "✅ Detective tools compiled successfully"

# Set investigation environment
export JDBX_ENABLE_EXOTIC_ALLOCATORS=true
export JDBX_ENABLE_ARENA_ALLOCATOR=true
export JDBX_ENABLE_TLSF_ALLOCATOR=true
export JDBX_MEM_DEBUG=false

echo ""
echo "🚀 Beginning Inspector Claude's investigation..."
echo "Environment: Arena=ON, TLSF=ON, Debug=OFF"
echo ""

# Run the investigation
"$BUILD_DIR/investigation/ssl_detective"

echo ""
echo "📊 Investigation completed!"
echo "Results logged for analysis and further clue gathering."