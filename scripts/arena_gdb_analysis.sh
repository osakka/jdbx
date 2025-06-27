#!/bin/bash
# GDB-based Arena Allocator Analysis
# Precise debugging of Arena allocation issues

set -euo pipefail

JDBX_ROOT="/opt/jdbx"
BUILD_DIR="$JDBX_ROOT/build"
DIAGNOSTIC_DIR="$BUILD_DIR/diagnostics"
LOG_DIR="$DIAGNOSTIC_DIR/logs"

mkdir -p "$LOG_DIR"

echo "🔬 GDB Arena Allocator Analysis"
echo "==============================="

# Configure Arena-only mode
echo "📝 Configuring Arena-only mode..."
sed -i "s/JDBX_ENABLE_EXOTIC_ALLOCATORS=.*/JDBX_ENABLE_EXOTIC_ALLOCATORS=true/" "$BUILD_DIR/var/jdbx.env"
sed -i "s/JDBX_ENABLE_ARENA_ALLOCATOR=.*/JDBX_ENABLE_ARENA_ALLOCATOR=true/" "$BUILD_DIR/var/jdbx.env"
sed -i "s/JDBX_ENABLE_TLSF_ALLOCATOR=.*/JDBX_ENABLE_TLSF_ALLOCATOR=false/" "$BUILD_DIR/var/jdbx.env"

# Stop any running instance
"$BUILD_DIR/../build/jdbx_runtime.sh" stop 2>/dev/null || true
sleep 2

echo "🐛 Preparing GDB script for Arena analysis..."

# Create GDB command script
cat > "$LOG_DIR/arena_gdb_commands.txt" << 'EOF'
# Set up Arena allocator debugging
set logging file /opt/jdbx/build/diagnostics/logs/arena_gdb_output.log
set logging on

# Set breakpoints at critical Arena functions
break arena_alloc
break arena_destroy
break memory_alloc
break memory_checkpoint_create
break memory_checkpoint_rewind

# Set breakpoint for segfaults
catch signal SIGSEGV

# Continue and let it run
continue

# When breakpoints hit, show detailed information
commands 1
  printf "🎯 arena_alloc called\n"
  printf "   arena = %p\n", arena
  printf "   size = %zu\n", size
  bt 5
  continue
end

commands 2
  printf "🎯 arena_destroy called\n"
  printf "   arena = %p\n", arena
  bt 5
  continue
end

commands 3
  printf "🎯 memory_alloc called\n"
  printf "   size = %zu\n", size
  bt 5
  continue
end

commands 4
  printf "🎯 memory_checkpoint_create called\n"
  bt 5
  continue
end

commands 5
  printf "🎯 memory_checkpoint_rewind called\n"
  bt 5
  continue
end

# For segfault
commands 6
  printf "🚨 SEGFAULT DETECTED\n"
  printf "Signal info:\n"
  info signal
  printf "Registers:\n"
  info registers
  printf "Backtrace:\n"
  bt
  printf "Memory around crash:\n"
  x/10x $rip-40
  printf "Stack:\n"
  x/20x $rsp
  continue
end

# Run for 30 seconds then quit
set $timeout = 30
run --config=/opt/jdbx/build/var/jdbx.env --foreground

# If we get here, quit
quit
EOF

echo "🏃 Running JDBX under GDB..."
cd "$JDBX_ROOT"

# Run GDB with the command script
timeout 60s gdb -batch -x "$LOG_DIR/arena_gdb_commands.txt" "$BUILD_DIR/bin/jdbxd" > "$LOG_DIR/arena_gdb_session.log" 2>&1 &

gdb_pid=$!
echo "🐛 GDB PID: $gdb_pid"

sleep 20  # Let GDB and JDBX start

echo "📡 Testing Arena allocator under GDB..."

# Test API calls that trigger Arena allocations
echo "   🔹 Testing health endpoint..."
curl -k -s --max-time 5 https://localhost:5000/api/health > "$LOG_DIR/arena_gdb_health_test.json" 2>&1 || true

echo "   🔹 Testing UI loading..."
curl -k -s --max-time 5 https://localhost:5000/ > "$LOG_DIR/arena_gdb_ui_test.html" 2>&1 || true

# Wait for GDB to finish or timeout
wait $gdb_pid 2>/dev/null || true

echo ""
echo "📊 GDB Analysis Results:"
echo "========================"

if [ -f "$LOG_DIR/arena_gdb_output.log" ]; then
    echo "🎯 Arena function calls detected:"
    grep -E "🎯|🚨" "$LOG_DIR/arena_gdb_output.log" || echo "No Arena calls detected"
    
    echo ""
    echo "🚨 Crash information:"
    grep -A 10 "SEGFAULT DETECTED" "$LOG_DIR/arena_gdb_output.log" || echo "No crashes detected"
else
    echo "❌ No GDB output log found"
fi

echo ""
echo "📁 Analysis files:"
echo "   - $LOG_DIR/arena_gdb_output.log"
echo "   - $LOG_DIR/arena_gdb_session.log"
echo "   - $LOG_DIR/arena_gdb_health_test.json"

echo "✅ GDB Arena analysis complete"