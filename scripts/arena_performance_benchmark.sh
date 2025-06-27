#!/bin/bash
# Arena Performance Benchmark Runner
# Compiles and executes comprehensive Arena allocator performance testing

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
JDBX_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$JDBX_ROOT/build"

echo "🔬 Arena Allocator Performance Benchmark Suite"
echo "=============================================="

# Create build directory if needed
mkdir -p "$BUILD_DIR/benchmark"

# Compile the benchmark with JDBX memory management
echo "🔧 Compiling benchmark suite..."
cd "$JDBX_ROOT/src"

gcc -std=c11 -O2 -Wall -Wextra \
    -I"$JDBX_ROOT/src/include" \
    -DJDBX_ENABLE_EXOTIC_ALLOCATORS=1 \
    -DJDBX_ENABLE_ARENA_ALLOCATOR=1 \
    "$SCRIPT_DIR/arena_performance_benchmark.c" \
    components/utils/memory_manager.c \
    components/utils/arena_allocator.c \
    components/utils/tlsf_allocator.c \
    components/utils/memory_allocator_config.c \
    -o "$BUILD_DIR/benchmark/arena_performance_benchmark" \
    -pthread

echo "✅ Benchmark compiled successfully"

# Set environment for optimal Arena performance
export JDBX_ENABLE_EXOTIC_ALLOCATORS=true
export JDBX_ENABLE_ARENA_ALLOCATOR=true
export JDBX_ENABLE_TLSF_ALLOCATOR=false  # Focus on Arena only
export JDBX_MEM_DEBUG=false  # No debug overhead

echo ""
echo "🚀 Executing Arena performance benchmark..."
echo "Environment: Arena=ON, TLSF=OFF, Debug=OFF"
echo ""

# Run the benchmark
"$BUILD_DIR/benchmark/arena_performance_benchmark"

echo ""
echo "📊 Benchmark completed successfully!"
echo "Results saved for performance analysis and optimization planning."