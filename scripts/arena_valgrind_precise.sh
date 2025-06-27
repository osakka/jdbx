#!/bin/bash
# Precise Arena Allocator Valgrind Analysis
# Focus on API request phase where Arena fails

set -euo pipefail

JDBX_ROOT="/opt/jdbx"
BUILD_DIR="$JDBX_ROOT/build"
DIAGNOSTIC_DIR="$BUILD_DIR/diagnostics"
LOG_DIR="$DIAGNOSTIC_DIR/logs"

mkdir -p "$LOG_DIR"

echo "🔬 Precise Arena Allocator Valgrind Analysis"
echo "============================================"

# Configure Arena-only mode
echo "📝 Configuring Arena-only mode..."
sed -i "s/JDBX_ENABLE_EXOTIC_ALLOCATORS=.*/JDBX_ENABLE_EXOTIC_ALLOCATORS=true/" "$BUILD_DIR/var/jdbx.env"
sed -i "s/JDBX_ENABLE_ARENA_ALLOCATOR=.*/JDBX_ENABLE_ARENA_ALLOCATOR=true/" "$BUILD_DIR/var/jdbx.env"
sed -i "s/JDBX_ENABLE_TLSF_ALLOCATOR=.*/JDBX_ENABLE_TLSF_ALLOCATOR=false/" "$BUILD_DIR/var/jdbx.env"

# Stop any running instance
"$BUILD_DIR/../build/jdbx_runtime.sh" stop 2>/dev/null || true
sleep 2

echo "🏃 Starting JDBX under Valgrind (Arena-only)..."
cd "$JDBX_ROOT"

# Start valgrind with comprehensive options
valgrind \
    --tool=memcheck \
    --leak-check=full \
    --show-leak-kinds=all \
    --track-origins=yes \
    --track-fds=yes \
    --show-reachable=yes \
    --error-exitcode=1 \
    --gen-suppressions=all \
    --verbose \
    --log-file="$LOG_DIR/arena_valgrind_detailed.log" \
    --xml=yes \
    --xml-file="$LOG_DIR/arena_valgrind_detailed.xml" \
    "$BUILD_DIR/bin/jdbxd" \
    --config="$BUILD_DIR/var/jdbx.env" \
    --foreground \
    > "$LOG_DIR/arena_valgrind_stdout.log" 2>&1 &

valgrind_pid=$!
echo "🔬 Valgrind PID: $valgrind_pid"

echo "⏳ Waiting for server initialization..."
sleep 15

echo "📡 Testing API endpoint (where Arena fails)..."

# Test the exact sequence that causes Arena to fail
echo "   🔹 Testing health endpoint..."
if curl -k -s --max-time 10 https://localhost:5000/api/health > "$LOG_DIR/arena_api_test.json" 2>&1; then
    echo "   ✅ Health API succeeded under Valgrind!"
else
    echo "   ❌ Health API failed under Valgrind"
fi

echo "   🔹 Testing UI loading..."
if curl -k -s --max-time 10 https://localhost:5000/ > "$LOG_DIR/arena_ui_test.html" 2>&1; then
    echo "   ✅ UI loading succeeded under Valgrind!"
else
    echo "   ❌ UI loading failed under Valgrind"
fi

echo "   🔹 Testing concurrent requests..."
for i in {1..3}; do
    curl -k -s --max-time 5 https://localhost:5000/api/health > /dev/null 2>&1 &
done
wait

echo "⏹️ Stopping Valgrind analysis..."
kill $valgrind_pid 2>/dev/null || true
wait $valgrind_pid 2>/dev/null || true

echo ""
echo "📊 Analysis Results:"
echo "===================="

# Check for specific Arena-related errors
if grep -q "Invalid read\|Invalid write\|Use of uninitialised value\|Invalid free" "$LOG_DIR/arena_valgrind_detailed.log"; then
    echo "🚨 MEMORY ERRORS DETECTED:"
    grep -A 5 -B 2 "Invalid read\|Invalid write\|Use of uninitialised value\|Invalid free" "$LOG_DIR/arena_valgrind_detailed.log" | head -50
else
    echo "✅ No memory errors detected by Valgrind"
fi

echo ""
echo "📁 Detailed logs:"
echo "   - $LOG_DIR/arena_valgrind_detailed.log"
echo "   - $LOG_DIR/arena_valgrind_detailed.xml"
echo "   - $LOG_DIR/arena_valgrind_stdout.log"

echo "✅ Precise Arena Valgrind analysis complete"