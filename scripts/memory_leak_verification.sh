#!/bin/bash

# Memory leak verification test
# Tests if memory trace logging is causing the leak

echo "🔬 Memory Leak Verification Test"
echo "================================"

# Function to get memory usage
get_memory() {
    local pid=$1
    ps -p "$pid" -o rss= 2>/dev/null | awk '{print int($1/1024)}'
}

# Test 1: Run WITHOUT memory trace
echo ""
echo "📊 TEST 1: Running WITHOUT memory trace..."
echo "Restarting server with memory trace disabled..."

# Ensure memory trace is disabled
grep -q "^JDBX_TRACE_CATEGORIES=memory" /opt/jdbx/build/var/jdbx.env && \
    sed -i 's/^JDBX_TRACE_CATEGORIES=memory/# JDBX_TRACE_CATEGORIES=memory/' /opt/jdbx/build/var/jdbx.env

cd /opt/jdbx
build/jdbx_runtime.sh restart >/dev/null 2>&1
sleep 5

PID=$(pgrep -f jdbxd | head -1)
if [ -z "$PID" ]; then
    echo "❌ Failed to start server"
    exit 1
fi

echo "✅ Server started (PID: $PID)"
echo "📊 Monitoring memory for 5 minutes..."

# Monitor for 5 minutes
START_MEM=$(get_memory $PID)
echo "Starting memory: ${START_MEM}MB"

for i in {1..10}; do
    sleep 30
    CURRENT_MEM=$(get_memory $PID)
    echo "  $(date +%H:%M:%S) - Memory: ${CURRENT_MEM}MB"
    
    # Generate some load
    for j in {1..10}; do
        curl -s -k https://localhost:5000/api/collections >/dev/null 2>&1 &
    done
    wait
done

END_MEM=$(get_memory $PID)
GROWTH=$((END_MEM - START_MEM))
echo "Final memory: ${END_MEM}MB (Growth: ${GROWTH}MB)"

if [ "$GROWTH" -lt 10 ]; then
    echo "✅ PASS: Memory stable without trace (growth < 10MB)"
    NO_TRACE_RESULT="STABLE"
else
    echo "❌ FAIL: Memory still growing without trace"
    NO_TRACE_RESULT="LEAKING"
fi

# Test 2: Run WITH memory trace
echo ""
echo "📊 TEST 2: Running WITH memory trace..."
echo "Enabling memory trace and restarting..."

# Enable memory trace
sed -i 's/# JDBX_TRACE_CATEGORIES=memory/JDBX_TRACE_CATEGORIES=memory/' /opt/jdbx/build/var/jdbx.env

build/jdbx_runtime.sh restart >/dev/null 2>&1
sleep 5

PID=$(pgrep -f jdbxd | head -1)
if [ -z "$PID" ]; then
    echo "❌ Failed to start server"
    exit 1
fi

echo "✅ Server started (PID: $PID)"
echo "📊 Monitoring memory for 5 minutes..."

START_MEM=$(get_memory $PID)
echo "Starting memory: ${START_MEM}MB"

for i in {1..10}; do
    sleep 30
    CURRENT_MEM=$(get_memory $PID)
    echo "  $(date +%H:%M:%S) - Memory: ${CURRENT_MEM}MB"
    
    # Generate some load
    for j in {1..10}; do
        curl -s -k https://localhost:5000/api/collections >/dev/null 2>&1 &
    done
    wait
done

END_MEM=$(get_memory $PID)
GROWTH=$((END_MEM - START_MEM))
echo "Final memory: ${END_MEM}MB (Growth: ${GROWTH}MB)"

if [ "$GROWTH" -gt 20 ]; then
    echo "❌ FAIL: Memory leak detected with trace (growth > 20MB)"
    WITH_TRACE_RESULT="LEAKING"
else
    echo "✅ PASS: Memory stable with trace"
    WITH_TRACE_RESULT="STABLE"
fi

# Restore original state (trace disabled)
sed -i 's/^JDBX_TRACE_CATEGORIES=memory/# JDBX_TRACE_CATEGORIES=memory/' /opt/jdbx/build/var/jdbx.env

echo ""
echo "📊 RESULTS SUMMARY:"
echo "==================="
echo "Without memory trace: $NO_TRACE_RESULT"
echo "With memory trace: $WITH_TRACE_RESULT"

if [ "$NO_TRACE_RESULT" = "STABLE" ] && [ "$WITH_TRACE_RESULT" = "LEAKING" ]; then
    echo ""
    echo "✅ CONFIRMED: Memory trace logging is causing the leak!"
    echo "💡 SOLUTION: Keep JDBX_TRACE_CATEGORIES disabled in production"
else
    echo ""
    echo "🤔 INCONCLUSIVE: Need further investigation"
fi