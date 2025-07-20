#!/bin/bash

# Simple memory leak test script
# This script generates load on the server and monitors memory usage

SERVER_URL="https://localhost:5000"
LOG_FILE="/opt/jdbx/build/var/memory_test.log"
REQUESTS_COUNT=100
CONCURRENT_REQUESTS=5

echo "Starting memory leak test..."
echo "Server URL: $SERVER_URL"
echo "Total requests: $REQUESTS_COUNT"
echo "Concurrent requests: $CONCURRENT_REQUESTS"

# Function to get current memory usage
get_memory_usage() {
    local pid_file="/opt/jdbx/build/var/jdbxd.pid"
    if [ -f "$pid_file" ]; then
        local pid=$(cat "$pid_file")
        if kill -0 "$pid" 2>/dev/null; then
            ps -p "$pid" -o rss --no-headers | tr -d ' '
        else
            echo "0"
        fi
    else
        echo "0"
    fi
}

# Get initial memory usage
initial_memory=$(get_memory_usage)
echo "Initial memory usage: ${initial_memory}KB"

# Create temporary file for tracking memory
temp_file=$(mktemp)
echo "timestamp,memory_kb" > "$temp_file"

# Function to make requests
make_requests() {
    local request_num=$1
    
    # Try to login (this will fail but still consume memory)
    curl -k -s -X POST "$SERVER_URL/api/auth/login" \
         -H "Content-Type: application/json" \
         -d '{"username":"test","password":"test"}' > /dev/null 2>&1
    
    # Try to get status
    curl -k -s -X GET "$SERVER_URL/api/status" > /dev/null 2>&1
    
    # Try to get some documents
    curl -k -s -X GET "$SERVER_URL/api/documents" > /dev/null 2>&1
    
    # Log current memory usage
    local current_memory=$(get_memory_usage)
    echo "$(date '+%Y-%m-%d %H:%M:%S'),${current_memory}" >> "$temp_file"
    
    echo "Request $request_num completed, memory: ${current_memory}KB"
}

# Run requests
echo "Starting request loop..."
for i in $(seq 1 $REQUESTS_COUNT); do
    # Run requests in parallel batches
    for j in $(seq 1 $CONCURRENT_REQUESTS); do
        make_requests $((i * CONCURRENT_REQUESTS + j)) &
    done
    
    # Wait for batch to complete
    wait
    
    # Small delay between batches
    sleep 0.1
done

echo "All requests completed."

# Get final memory usage
final_memory=$(get_memory_usage)
memory_increase=$((final_memory - initial_memory))

echo "=== Memory Leak Test Results ==="
echo "Initial memory: ${initial_memory}KB"
echo "Final memory: ${final_memory}KB"
echo "Memory increase: ${memory_increase}KB ($(($memory_increase / 1024))MB)"

# Analyze memory growth pattern
if [ -f "$temp_file" ]; then
    echo "=== Memory Growth Analysis ==="
    
    # Get min, max, and growth
    awk -F',' 'NR>1 {print $2}' "$temp_file" | \
    awk '
    BEGIN { count=0; sum=0; min=999999999; max=0; first=0; last=0 }
    {
        if (count == 0) first = $1;
        last = $1;
        sum += $1;
        if ($1 < min) min = $1;
        if ($1 > max) max = $1;
        count++;
    }
    END {
        if (count > 1) {
            avg = sum / count;
            growth = last - first;
            printf "Average memory: %.0fKB (%.1fMB)\n", avg, avg/1024;
            printf "Memory range: %dKB - %dKB\n", min, max;
            printf "Total growth: %dKB (%.1fMB)\n", growth, growth/1024;
            printf "Growth rate: %.2fKB per request\n", growth/(count-1);
            
            if (growth > 10240) {  # 10MB growth
                printf "WARNING: Significant memory growth detected!\n";
            } else if (growth > 1024) {  # 1MB growth
                printf "CAUTION: Moderate memory growth detected.\n";
            } else {
                printf "GOOD: Memory usage appears stable.\n";
            }
        }
    }'
    
    # Copy detailed log
    cp "$temp_file" "$LOG_FILE"
    echo "Detailed memory log saved to: $LOG_FILE"
fi

# Cleanup
rm -f "$temp_file"

echo "Memory leak test completed."