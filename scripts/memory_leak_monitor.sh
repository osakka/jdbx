#!/bin/bash

# Memory leak monitoring script for JDBX server
# This script monitors memory usage and identifies potential memory leaks

JDBX_PID_FILE="/opt/jdbx/build/var/jdbxd.pid"
LOG_FILE="/opt/jdbx/build/var/memory_monitor.log"
INTERVAL=10  # Check every 10 seconds
MAX_MEMORY_MB=1024  # Alert if memory exceeds 1GB

# Colors for output
RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

echo "Starting JDBX memory leak monitoring..."
echo "Log file: $LOG_FILE"
echo "Check interval: ${INTERVAL}s"
echo "Memory alert threshold: ${MAX_MEMORY_MB}MB"
echo "================================="

# Initialize log file
echo "$(date): Memory leak monitoring started" > "$LOG_FILE"
echo "PID,RSS(KB),VSZ(KB),CPU%,MEM%,TIME" >> "$LOG_FILE"

# Function to get memory info
get_memory_info() {
    if [ -f "$JDBX_PID_FILE" ]; then
        PID=$(cat "$JDBX_PID_FILE")
        if kill -0 "$PID" 2>/dev/null; then
            # Get memory statistics
            MEMORY_INFO=$(ps -p "$PID" -o pid,rss,vsz,pcpu,pmem,time --no-headers)
            if [ -n "$MEMORY_INFO" ]; then
                echo "$MEMORY_INFO"
                return 0
            fi
        fi
    fi
    return 1
}

# Function to check for memory leaks
check_memory_leak() {
    local rss_kb=$1
    local rss_mb=$((rss_kb / 1024))
    
    if [ "$rss_mb" -gt "$MAX_MEMORY_MB" ]; then
        echo -e "${RED}[ALERT] Memory usage: ${rss_mb}MB (exceeds ${MAX_MEMORY_MB}MB threshold)${NC}"
        echo "$(date): MEMORY ALERT - RSS: ${rss_mb}MB" >> "$LOG_FILE"
        
        # Get additional process info
        if [ -f "$JDBX_PID_FILE" ]; then
            PID=$(cat "$JDBX_PID_FILE")
            echo "=== Process Memory Map ===" >> "$LOG_FILE"
            pmap -x "$PID" >> "$LOG_FILE" 2>/dev/null || echo "pmap not available" >> "$LOG_FILE"
            echo "=== Process Status ===" >> "$LOG_FILE"
            cat "/proc/$PID/status" >> "$LOG_FILE" 2>/dev/null || echo "status not available" >> "$LOG_FILE"
        fi
        
        return 1
    fi
    
    return 0
}

# Function to analyze memory growth
analyze_memory_growth() {
    local log_lines=10
    if [ -f "$LOG_FILE" ]; then
        # Get last 10 entries and check for growth pattern
        tail -n "$log_lines" "$LOG_FILE" | grep -E "^[0-9]+" | \
        awk -F',' '{print $2}' | \
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
                printf "Memory Analysis: Avg=%.0fKB, Min=%dKB, Max=%dKB, Growth=%dKB\n", avg, min, max, growth;
                if (growth > 10240) {  # 10MB growth
                    printf "WARNING: Significant memory growth detected: %dKB\n", growth;
                }
            }
        }'
    fi
}

# Monitor loop
previous_rss=0
growth_count=0
stable_count=0

while true; do
    MEMORY_INFO=$(get_memory_info)
    
    if [ $? -eq 0 ]; then
        PID=$(echo "$MEMORY_INFO" | awk '{print $1}')
        RSS_KB=$(echo "$MEMORY_INFO" | awk '{print $2}')
        VSZ_KB=$(echo "$MEMORY_INFO" | awk '{print $3}')
        CPU_PCT=$(echo "$MEMORY_INFO" | awk '{print $4}')
        MEM_PCT=$(echo "$MEMORY_INFO" | awk '{print $5}')
        TIME=$(echo "$MEMORY_INFO" | awk '{print $6}')
        
        RSS_MB=$((RSS_KB / 1024))
        VSZ_MB=$((VSZ_KB / 1024))
        
        # Log to file
        echo "$(date '+%Y-%m-%d %H:%M:%S'),$PID,$RSS_KB,$VSZ_KB,$CPU_PCT,$MEM_PCT,$TIME" >> "$LOG_FILE"
        
        # Check for memory growth
        if [ "$previous_rss" -gt 0 ]; then
            growth=$((RSS_KB - previous_rss))
            if [ "$growth" -gt 1024 ]; then  # More than 1MB growth
                growth_count=$((growth_count + 1))
                stable_count=0
                echo -e "${YELLOW}[GROWTH] Memory increased by $((growth / 1024))MB (Total: ${RSS_MB}MB)${NC}"
            else
                stable_count=$((stable_count + 1))
                if [ "$growth_count" -gt 0 ]; then
                    growth_count=$((growth_count - 1))
                fi
            fi
        fi
        
        # Display current status
        if [ "$growth_count" -gt 5 ]; then
            echo -e "${RED}[LEAK DETECTED] Continuous memory growth detected${NC}"
            echo "$(date): POTENTIAL MEMORY LEAK - Continuous growth detected" >> "$LOG_FILE"
            analyze_memory_growth
        elif [ "$stable_count" -gt 10 ]; then
            echo -e "${GREEN}[STABLE] Memory usage stable at ${RSS_MB}MB${NC}"
        else
            echo "[MONITOR] PID:$PID RSS:${RSS_MB}MB VSZ:${VSZ_MB}MB CPU:${CPU_PCT}% MEM:${MEM_PCT}%"
        fi
        
        # Check for memory leak
        check_memory_leak "$RSS_KB"
        
        previous_rss=$RSS_KB
        
    else
        echo -e "${RED}[ERROR] JDBX server not running${NC}"
        echo "$(date): Server not running" >> "$LOG_FILE"
        sleep 5
    fi
    
    sleep "$INTERVAL"
done