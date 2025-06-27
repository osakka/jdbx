#!/bin/bash
# JDBX Memory Allocator Comprehensive Diagnostic Suite
# Systematic analysis of TLSF and Arena allocator issues

set -euo pipefail

JDBX_ROOT="/opt/jdbx"
BUILD_DIR="$JDBX_ROOT/build"
DIAGNOSTIC_DIR="$BUILD_DIR/diagnostics"
LOG_DIR="$DIAGNOSTIC_DIR/logs"
CORE_DIR="$DIAGNOSTIC_DIR/cores"

# Create diagnostic directory structure
mkdir -p "$DIAGNOSTIC_DIR" "$LOG_DIR" "$CORE_DIR"

echo "🔬 JDBX Memory Allocator Diagnostic Suite"
echo "=========================================="

# Function to run controlled test with specific allocator configuration
run_allocator_test() {
    local test_name="$1"
    local exotic_enabled="$2"
    local arena_enabled="$3"
    local tlsf_enabled="$4"
    local description="$5"
    
    echo "🧪 Test: $test_name - $description"
    echo "   Config: exotic=$exotic_enabled, arena=$arena_enabled, tlsf=$tlsf_enabled"
    
    # Configure allocators
    sed -i "s/JDBX_ENABLE_EXOTIC_ALLOCATORS=.*/JDBX_ENABLE_EXOTIC_ALLOCATORS=$exotic_enabled/" "$BUILD_DIR/var/jdbx.env"
    sed -i "s/JDBX_ENABLE_ARENA_ALLOCATOR=.*/JDBX_ENABLE_ARENA_ALLOCATOR=$arena_enabled/" "$BUILD_DIR/var/jdbx.env"
    sed -i "s/JDBX_ENABLE_TLSF_ALLOCATOR=.*/JDBX_ENABLE_TLSF_ALLOCATOR=$tlsf_enabled/" "$BUILD_DIR/var/jdbx.env"
    
    # Stop any running instance
    "$BUILD_DIR/../build/jdbx_runtime.sh" stop 2>/dev/null || true
    sleep 2
    
    # Start with diagnostics
    echo "   📋 Starting JDBX with configuration..."
    timeout 30s "$BUILD_DIR/../build/jdbx_runtime.sh" start > "$LOG_DIR/${test_name}_start.log" 2>&1 || {
        echo "   ❌ Failed to start - analyzing logs..."
        tail -20 "$BUILD_DIR/var/jdbxd.log" > "$LOG_DIR/${test_name}_crash.log" 2>/dev/null || true
        return 1
    }
    
    sleep 3
    
    # Test basic API functionality
    echo "   🔄 Testing API responsiveness..."
    if curl -k -s --max-time 10 https://localhost:5000/api/health > "$LOG_DIR/${test_name}_health.json" 2>/dev/null; then
        echo "   ✅ Health API responsive"
        
        # Test UI loading
        echo "   🌐 Testing UI loading..."
        if curl -k -s --max-time 10 https://localhost:5000/ > /dev/null 2>&1; then
            echo "   ✅ UI loads successfully"
            
            # Test multiple concurrent requests
            echo "   ⚡ Testing concurrent load..."
            for i in {1..5}; do
                curl -k -s --max-time 5 https://localhost:5000/api/health > /dev/null 2>&1 &
            done
            wait
            
            if pgrep -f jdbxd > /dev/null; then
                echo "   ✅ Survived concurrent load"
                echo "   📊 Test PASSED: $test_name"
                return 0
            else
                echo "   ❌ Crashed during concurrent load"
            fi
        else
            echo "   ❌ UI loading failed"
        fi
    else
        echo "   ❌ Health API unresponsive"
    fi
    
    # Capture crash information
    if [ -f "$BUILD_DIR/var/jdbxd.log" ]; then
        tail -50 "$BUILD_DIR/var/jdbxd.log" > "$LOG_DIR/${test_name}_final.log"
    fi
    
    echo "   📊 Test FAILED: $test_name"
    return 1
}

# Function to run valgrind analysis
run_valgrind_analysis() {
    local test_name="$1"
    local exotic_enabled="$2"
    local arena_enabled="$3"
    local tlsf_enabled="$4"
    
    echo "🔍 Valgrind Analysis: $test_name"
    
    # Configure allocators
    sed -i "s/JDBX_ENABLE_EXOTIC_ALLOCATORS=.*/JDBX_ENABLE_EXOTIC_ALLOCATORS=$exotic_enabled/" "$BUILD_DIR/var/jdbx.env"
    sed -i "s/JDBX_ENABLE_ARENA_ALLOCATOR=.*/JDBX_ENABLE_ARENA_ALLOCATOR=$arena_enabled/" "$BUILD_DIR/var/jdbx.env"
    sed -i "s/JDBX_ENABLE_TLSF_ALLOCATOR=.*/JDBX_ENABLE_TLSF_ALLOCATOR=$tlsf_enabled/" "$BUILD_DIR/var/jdbx.env"
    
    # Stop any running instance
    "$BUILD_DIR/../build/jdbx_runtime.sh" stop 2>/dev/null || true
    sleep 2
    
    echo "   🔬 Running under Valgrind..."
    cd "$JDBX_ROOT"
    
    # Run with valgrind (non-daemon mode for proper capture)
    timeout 60s valgrind \
        --tool=memcheck \
        --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        --verbose \
        --log-file="$LOG_DIR/${test_name}_valgrind.log" \
        "$BUILD_DIR/bin/jdbxd" \
        --config="$BUILD_DIR/var/jdbx.env" \
        --no-daemon \
        > "$LOG_DIR/${test_name}_valgrind_output.log" 2>&1 &
    
    local valgrind_pid=$!
    sleep 10  # Let it initialize
    
    # Test basic operations
    echo "   📡 Testing under Valgrind..."
    curl -k -s --max-time 5 https://localhost:5000/api/health > /dev/null 2>&1 || true
    sleep 2
    curl -k -s --max-time 5 https://localhost:5000/ > /dev/null 2>&1 || true
    sleep 2
    
    # Clean shutdown
    kill $valgrind_pid 2>/dev/null || true
    wait $valgrind_pid 2>/dev/null || true
    
    echo "   📊 Valgrind analysis complete: $LOG_DIR/${test_name}_valgrind.log"
}

# Test Matrix
echo "🧪 Starting systematic test matrix..."

# Test 1: System malloc baseline (should always pass)
run_allocator_test "system_malloc" "false" "false" "false" "Pure system malloc baseline"

# Test 2: TLSF only mode
run_allocator_test "tlsf_only" "true" "false" "true" "TLSF allocator only"

# Test 3: Arena only mode  
run_allocator_test "arena_only" "true" "true" "false" "Arena allocator only"

# Test 4: Combined mode
run_allocator_test "combined_mode" "true" "true" "true" "TLSF + Arena combined"

echo ""
echo "🔬 Starting Valgrind deep analysis..."

# Valgrind Analysis
run_valgrind_analysis "valgrind_system" "false" "false" "false"
run_valgrind_analysis "valgrind_tlsf" "true" "false" "true"
run_valgrind_analysis "valgrind_arena" "true" "true" "false"

echo ""
echo "📊 Diagnostic Summary"
echo "===================="
echo "📁 All logs stored in: $LOG_DIR"
echo "🔍 Review valgrind logs for memory corruption patterns"
echo "📋 Test results provide failure pattern analysis"

# Generate summary report
{
    echo "# JDBX Memory Allocator Diagnostic Report"
    echo "Generated: $(date)"
    echo ""
    echo "## Test Results"
    for log in "$LOG_DIR"/*_start.log; do
        if [ -f "$log" ]; then
            test_name=$(basename "$log" _start.log)
            if [ -f "$LOG_DIR/${test_name}_health.json" ]; then
                echo "✅ $test_name: PASSED"
            else
                echo "❌ $test_name: FAILED"
            fi
        fi
    done
    echo ""
    echo "## Valgrind Analysis Files"
    ls -la "$LOG_DIR"/*valgrind* 2>/dev/null || echo "No valgrind logs found"
} > "$DIAGNOSTIC_DIR/summary_report.md"

echo "📄 Summary report: $DIAGNOSTIC_DIR/summary_report.md"
echo "✅ Diagnostic suite complete"