#!/bin/bash

# 24-Hour Memory Monitoring Script for JDBX
# Logs detailed memory usage every 30 seconds with leak detection

LOG_DIR="/opt/jdbx/build/var/memory_logs"
mkdir -p "$LOG_DIR"

TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
LOG_FILE="$LOG_DIR/memory_24h_$TIMESTAMP.log"
ALERT_FILE="$LOG_DIR/memory_alerts_$TIMESTAMP.log"

# Memory thresholds (MB)
WARNING_THRESHOLD=100
CRITICAL_THRESHOLD=500
KILL_THRESHOLD=1000

echo "🔍 Starting 24-hour memory monitoring at $(date)" | tee "$LOG_FILE"
echo "📊 Logging to: $LOG_FILE" | tee -a "$LOG_FILE"
echo "🚨 Alerts to: $ALERT_FILE" | tee -a "$LOG_FILE"
echo "================================================" | tee -a "$LOG_FILE"

# Function to get process memory info
get_memory_info() {
    local pid=$1
    if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
        # Get detailed memory info
        local mem_info=$(ps -p "$pid" -o pid,ppid,rss,vsz,pmem,etime,cmd --no-headers 2>/dev/null)
        local rss=$(echo "$mem_info" | awk '{print $3}')
        local vsz=$(echo "$mem_info" | awk '{print $4}')
        local pmem=$(echo "$mem_info" | awk '{print $5}')
        local etime=$(echo "$mem_info" | awk '{print $6}')
        
        # Convert KB to MB
        local rss_mb=$((rss / 1024))
        local vsz_mb=$((vsz / 1024))
        
        echo "$rss_mb $vsz_mb $pmem $etime"
    else
        echo "0 0 0.0 00:00"
    fi
}

# Function to get system memory
get_system_memory() {
    free -m | awk '/^Mem:/ {printf "%d %d %d %.1f", $3, $2, $7, ($3/$2)*100}'
}

# Function to check for memory leaks
check_memory_growth() {
    local current_rss=$1
    local previous_rss=$2
    local growth=$((current_rss - previous_rss))
    
    if [ "$growth" -gt 10 ]; then  # More than 10MB growth
        echo "📈 MEMORY GROWTH: +${growth}MB in 30s (${previous_rss}MB → ${current_rss}MB)"
        return 1
    fi
    return 0
}

# Function to send alerts
send_alert() {
    local level=$1
    local message=$2
    local timestamp=$(date "+%Y-%m-%d %H:%M:%S")
    
    echo "[$timestamp] $level: $message" | tee -a "$ALERT_FILE"
    
    if [ "$level" = "CRITICAL" ]; then
        echo "🚨 CRITICAL MEMORY ALERT: $message" >&2
    fi
}

# Main monitoring loop
previous_rss=0
alert_count=0
sample_count=0

echo "Starting monitoring loop..." | tee -a "$LOG_FILE"

while true; do
    timestamp=$(date "+%Y-%m-%d %H:%M:%S")
    sample_count=$((sample_count + 1))
    
    # Get JDBX process info
    jdbx_pid=$(pgrep -f "jdbxd" | head -1)
    
    if [ -n "$jdbx_pid" ]; then
        # Get memory info
        mem_info=$(get_memory_info "$jdbx_pid")
        rss_mb=$(echo "$mem_info" | awk '{print $1}')
        vsz_mb=$(echo "$mem_info" | awk '{print $2}')
        pmem=$(echo "$mem_info" | awk '{print $3}')
        etime=$(echo "$mem_info" | awk '{print $4}')
        
        # Get system memory
        sys_mem=$(get_system_memory)
        sys_used=$(echo "$sys_mem" | awk '{print $1}')
        sys_total=$(echo "$sys_mem" | awk '{print $2}')
        sys_available=$(echo "$sys_mem" | awk '{print $3}')
        sys_percent=$(echo "$sys_mem" | awk '{print $4}')
        
        # Log detailed info
        echo "[$timestamp] PID:$jdbx_pid RSS:${rss_mb}MB VSZ:${vsz_mb}MB %MEM:${pmem}% RUNTIME:$etime SYS:${sys_used}/${sys_total}MB (${sys_percent}%)" | tee -a "$LOG_FILE"
        
        # Check for memory growth
        if [ "$previous_rss" -gt 0 ]; then
            if ! check_memory_growth "$rss_mb" "$previous_rss"; then
                growth=$((rss_mb - previous_rss))
                send_alert "WARNING" "Memory growth detected: +${growth}MB (${previous_rss}MB → ${rss_mb}MB)"
                alert_count=$((alert_count + 1))
            fi
        fi
        
        # Check thresholds
        if [ "$rss_mb" -gt "$KILL_THRESHOLD" ]; then
            send_alert "CRITICAL" "Memory usage ${rss_mb}MB exceeds KILL threshold ${KILL_THRESHOLD}MB"
            echo "🛑 EMERGENCY: Killing JDBX process due to excessive memory usage"
            kill -TERM "$jdbx_pid"
            sleep 5
            kill -KILL "$jdbx_pid" 2>/dev/null
        elif [ "$rss_mb" -gt "$CRITICAL_THRESHOLD" ]; then
            send_alert "CRITICAL" "Memory usage ${rss_mb}MB exceeds critical threshold ${CRITICAL_THRESHOLD}MB"
        elif [ "$rss_mb" -gt "$WARNING_THRESHOLD" ]; then
            send_alert "WARNING" "Memory usage ${rss_mb}MB exceeds warning threshold ${WARNING_THRESHOLD}MB"
        fi
        
        previous_rss=$rss_mb
        
    else
        echo "[$timestamp] JDBX process not found" | tee -a "$LOG_FILE"
        previous_rss=0
    fi
    
    # Summary every 10 minutes
    if [ $((sample_count % 20)) -eq 0 ]; then
        echo "[$timestamp] === 10-MINUTE SUMMARY (Sample #$sample_count) ===" | tee -a "$LOG_FILE"
        echo "[$timestamp] Alerts in last 10min: $alert_count" | tee -a "$LOG_FILE"
        alert_count=0
    fi
    
    sleep 30
done