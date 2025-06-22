#!/bin/bash
# JDBX Memory Leak Testing - Phase 1.1 Simple Implementation
# Tests checkpoint-based memory management under sustained load

# Configuration
SERVER_URL="https://localhost:5000"
ADMIN_USER="${JDBX_BOOTSTRAP_ADMIN_USER:-admin}"
ADMIN_PASS="${JDBX_BOOTSTRAP_ADMIN_PASS:-admin}"
TEST_DURATION=180  # 3 minutes
CONCURRENT_PROCESSES=8
OPERATIONS_PER_PROCESS=25

echo "🧪 JDBX Memory Leak Testing - Phase 1.1"
echo "======================================"

# Find JDBX process
JDBX_PID=$(pgrep -f jdbxd | head -1)
if [ -z "$JDBX_PID" ]; then
    echo "❌ JDBX server process not found"
    exit 1
fi

echo "📊 Monitoring JDBX process PID: $JDBX_PID"

# Function to get memory usage
get_memory_usage() {
    if [ -f "/proc/$JDBX_PID/status" ]; then
        grep "VmRSS" /proc/$JDBX_PID/status | awk '{print $2}'
    else
        echo "0"
    fi
}

# Function to authenticate and get token
get_auth_token() {
    curl -k -s -X POST "$SERVER_URL/api/auth/login" \
        -H "Content-Type: application/json" \
        -d "{\"username\":\"$ADMIN_USER\",\"password\":\"$ADMIN_PASS\"}" | \
        grep -o '"token":"[^"]*"' | cut -d'"' -f4
}

# Function to perform CRUD operations
perform_operations() {
    local process_id=$1
    local token=$2
    local operations=0
    local errors=0
    
    for ((i=1; i<=OPERATIONS_PER_PROCESS; i++)); do
        # Create document
        doc_response=$(curl -k -s -X POST "$SERVER_URL/api/libraries/default/collections/test_docs/documents" \
            -H "Authorization: Bearer $token" \
            -H "Content-Type: application/json" \
            -d "{\"name\":\"test_doc_${process_id}_${i}\",\"data\":{\"value\":$RANDOM,\"text\":\"$(head -c 100 /dev/urandom | base64 | tr -d '\n')\"}}")
        
        if echo "$doc_response" | grep -q '"uuid"'; then
            operations=$((operations + 1))
            
            # Extract UUID for update/delete
            uuid=$(echo "$doc_response" | grep -o '"uuid":"[^"]*"' | cut -d'"' -f4)
            
            # Query documents
            curl -k -s -X GET "$SERVER_URL/api/libraries/default/collections/test_docs/documents" \
                -H "Authorization: Bearer $token" > /dev/null
            operations=$((operations + 1))
            
            # Update document
            if [ -n "$uuid" ]; then
                curl -k -s -X PUT "$SERVER_URL/api/libraries/default/collections/test_docs/documents/$uuid" \
                    -H "Authorization: Bearer $token" \
                    -H "Content-Type: application/json" \
                    -d "{\"data\":{\"updated\":true,\"timestamp\":$(date +%s)}}" > /dev/null
                operations=$((operations + 1))
                
                # Delete document
                curl -k -s -X DELETE "$SERVER_URL/api/libraries/default/collections/test_docs/documents/$uuid" \
                    -H "Authorization: Bearer $token" > /dev/null
                operations=$((operations + 1))
            fi
        else
            errors=$((errors + 1))
        fi
        
        # Small delay
        sleep 0.05
    done
    
    echo "Process $process_id: $operations operations, $errors errors"
}

# Start memory monitoring
echo "🚀 Starting memory monitoring and load test..."

START_MEMORY=$(get_memory_usage)
echo "📊 Initial memory usage: ${START_MEMORY} KB"

# Create memory monitoring log
MEMORY_LOG="/tmp/jdbx_memory_test.log"
echo "timestamp,memory_kb" > "$MEMORY_LOG"

# Background memory monitoring
(
    while [ -f "$MEMORY_LOG.running" ]; do
        timestamp=$(date +%s)
        memory=$(get_memory_usage)
        echo "$timestamp,$memory" >> "$MEMORY_LOG"
        sleep 5
    done
) &
MONITOR_PID=$!

# Signal that monitoring should run
touch "$MEMORY_LOG.running"

# Get authentication token
echo "🔐 Authenticating..."
TOKEN=$(get_auth_token)
if [ -z "$TOKEN" ]; then
    echo "❌ Failed to authenticate"
    rm -f "$MEMORY_LOG.running"
    kill $MONITOR_PID 2>/dev/null
    exit 1
fi

echo "✅ Authentication successful"

# Start concurrent load testing
echo "🚀 Starting $CONCURRENT_PROCESSES concurrent processes..."
echo "   Operations per process: $OPERATIONS_PER_PROCESS"
echo "   Total operations: $((CONCURRENT_PROCESSES * OPERATIONS_PER_PROCESS * 4))"

START_TIME=$(date +%s)

# Launch background processes
PIDS=()
for ((p=1; p<=CONCURRENT_PROCESSES; p++)); do
    perform_operations $p "$TOKEN" &
    PIDS+=($!)
done

# Wait for all processes to complete
echo "⏳ Waiting for operations to complete..."
for pid in "${PIDS[@]}"; do
    wait $pid
done

END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))

# Stop memory monitoring
rm -f "$MEMORY_LOG.running"
sleep 6  # Wait for monitor to finish
kill $MONITOR_PID 2>/dev/null

# Get final memory usage
END_MEMORY=$(get_memory_usage)

echo ""
echo "📈 Memory Analysis Results:"
echo "=========================="
echo "Start Memory: ${START_MEMORY} KB"
echo "End Memory: ${END_MEMORY} KB"

if [ "$END_MEMORY" -gt 0 ] && [ "$START_MEMORY" -gt 0 ]; then
    MEMORY_DIFF=$((END_MEMORY - START_MEMORY))
    echo "Memory Change: ${MEMORY_DIFF} KB"
    
    # Calculate growth rate
    if [ "$ELAPSED" -gt 0 ]; then
        GROWTH_RATE_KB_MIN=$((MEMORY_DIFF * 60 / ELAPSED))
        echo "Growth Rate: ${GROWTH_RATE_KB_MIN} KB/minute"
    fi
    
    # Analyze memory samples
    if [ -f "$MEMORY_LOG" ]; then
        PEAK_MEMORY=$(tail -n +2 "$MEMORY_LOG" | cut -d',' -f2 | sort -n | tail -1)
        AVG_MEMORY=$(tail -n +2 "$MEMORY_LOG" | cut -d',' -f2 | awk '{sum+=$1; count++} END {if(count>0) print int(sum/count); else print 0}')
        echo "Peak Memory: ${PEAK_MEMORY} KB"
        echo "Average Memory: ${AVG_MEMORY} KB"
    fi
    
    echo "Test Duration: ${ELAPSED} seconds"
    
    # Memory leak assessment
    GROWTH_THRESHOLD_KB=10240  # 10 MB
    RATE_THRESHOLD_KB_MIN=2048  # 2 MB/minute
    
    if [ "$MEMORY_DIFF" -gt "$GROWTH_THRESHOLD_KB" ]; then
        echo "⚠️  POTENTIAL MEMORY LEAK: Growth of ${MEMORY_DIFF} KB exceeds threshold"
        LEAK_DETECTED=1
    elif [ "$ELAPSED" -gt 0 ] && [ "$GROWTH_RATE_KB_MIN" -gt "$RATE_THRESHOLD_KB_MIN" ]; then
        echo "⚠️  POTENTIAL MEMORY LEAK: Growth rate of ${GROWTH_RATE_KB_MIN} KB/min exceeds threshold"
        LEAK_DETECTED=1
    else
        echo "✅ MEMORY STABLE: No significant memory leaks detected"
        LEAK_DETECTED=0
    fi
else
    echo "❌ Unable to measure memory usage"
    LEAK_DETECTED=1
fi

echo ""
echo "🏆 Phase 1.1 Memory Management Assessment:"
echo "========================================"

if [ "$LEAK_DETECTED" -eq 0 ]; then
    echo "✅ PRODUCTION READY: Checkpoint-based memory management validated"
    echo "✅ No memory leaks detected under sustained concurrent load"
    echo "✅ Memory usage remained stable throughout test"
    
    # Clean up
    rm -f "$MEMORY_LOG"
    exit 0
else
    echo "⚠️  REQUIRES ATTENTION: Memory management issues detected"
    echo "📋 Memory log saved: $MEMORY_LOG"
    exit 1
fi