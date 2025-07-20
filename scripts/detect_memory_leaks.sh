#!/bin/bash

# Memory Leak Detection for JDBX
# Analyzes memory patterns and identifies potential leak sources

LOG_DIR="/opt/jdbx/build/var/memory_logs"
ANALYSIS_DIR="/opt/jdbx/build/var/memory_analysis"
mkdir -p "$ANALYSIS_DIR"

echo "🔍 JDBX Memory Leak Detection"
echo "================================"

# Function to analyze memory logs
analyze_memory_logs() {
    local log_file=$1
    local output_file="$ANALYSIS_DIR/leak_analysis_$(date +%Y%m%d_%H%M%S).txt"
    
    echo "📊 Analyzing memory log: $log_file" | tee "$output_file"
    echo "================================================" | tee -a "$output_file"
    
    if [ ! -f "$log_file" ]; then
        echo "❌ Log file not found: $log_file"
        return 1
    fi
    
    # Extract memory values
    grep -E "RSS:[0-9]+MB" "$log_file" | \
    sed -E 's/.*RSS:([0-9]+)MB.*/\1/' > "$ANALYSIS_DIR/memory_values.tmp"
    
    if [ ! -s "$ANALYSIS_DIR/memory_values.tmp" ]; then
        echo "❌ No memory data found in log file"
        return 1
    fi
    
    # Calculate statistics
    local min_mem=$(sort -n "$ANALYSIS_DIR/memory_values.tmp" | head -1)
    local max_mem=$(sort -n "$ANALYSIS_DIR/memory_values.tmp" | tail -1)
    local avg_mem=$(awk '{sum+=$1} END {print int(sum/NR)}' "$ANALYSIS_DIR/memory_values.tmp")
    local total_samples=$(wc -l < "$ANALYSIS_DIR/memory_values.tmp")
    local growth=$((max_mem - min_mem))
    
    echo "📈 MEMORY STATISTICS:" | tee -a "$output_file"
    echo "   Minimum Memory: ${min_mem}MB" | tee -a "$output_file"
    echo "   Maximum Memory: ${max_mem}MB" | tee -a "$output_file"
    echo "   Average Memory: ${avg_mem}MB" | tee -a "$output_file"
    echo "   Total Growth: ${growth}MB" | tee -a "$output_file"
    echo "   Sample Count: $total_samples" | tee -a "$output_file"
    echo "" | tee -a "$output_file"
    
    # Calculate growth rate
    if [ "$total_samples" -gt 10 ]; then
        local first_10_avg=$(head -10 "$ANALYSIS_DIR/memory_values.tmp" | awk '{sum+=$1} END {print int(sum/NR)}')
        local last_10_avg=$(tail -10 "$ANALYSIS_DIR/memory_values.tmp" | awk '{sum+=$1} END {print int(sum/NR)}')
        local growth_rate=$(((last_10_avg - first_10_avg) * 100 / first_10_avg))
        
        echo "📊 GROWTH ANALYSIS:" | tee -a "$output_file"
        echo "   First 10 samples avg: ${first_10_avg}MB" | tee -a "$output_file"
        echo "   Last 10 samples avg: ${last_10_avg}MB" | tee -a "$output_file"
        echo "   Growth rate: ${growth_rate}%" | tee -a "$output_file"
        echo "" | tee -a "$output_file"
        
        # Leak detection
        if [ "$growth_rate" -gt 20 ]; then
            echo "🚨 MEMORY LEAK DETECTED: ${growth_rate}% growth!" | tee -a "$output_file"
        elif [ "$growth_rate" -gt 10 ]; then
            echo "⚠️  POTENTIAL MEMORY LEAK: ${growth_rate}% growth" | tee -a "$output_file"
        elif [ "$growth_rate" -gt 5 ]; then
            echo "📈 GRADUAL MEMORY INCREASE: ${growth_rate}% growth" | tee -a "$output_file"
        else
            echo "✅ MEMORY USAGE STABLE: ${growth_rate}% growth" | tee -a "$output_file"
        fi
    fi
    
    # Find memory spikes
    echo "" | tee -a "$output_file"
    echo "🔍 MEMORY SPIKES (>50MB jumps):" | tee -a "$output_file"
    awk 'NR>1 {if ($1-prev > 50) print "Spike at sample " NR ": " prev "MB → " $1 "MB (++" ($1-prev) "MB)"} {prev=$1}' \
        "$ANALYSIS_DIR/memory_values.tmp" | tee -a "$output_file"
    
    # Cleanup
    rm -f "$ANALYSIS_DIR/memory_values.tmp"
    
    echo "" | tee -a "$output_file"
    echo "📋 Analysis saved to: $output_file"
    
    return 0
}

# Function to get current memory snapshot
get_memory_snapshot() {
    local output_file="$ANALYSIS_DIR/memory_snapshot_$(date +%Y%m%d_%H%M%S).txt"
    
    echo "📸 JDBX Memory Snapshot" | tee "$output_file"
    echo "Timestamp: $(date)" | tee -a "$output_file"
    echo "================================" | tee -a "$output_file"
    
    # Find JDBX process
    local jdbx_pid=$(pgrep -f "jdbxd" | head -1)
    
    if [ -n "$jdbx_pid" ]; then
        echo "🔍 Process Information:" | tee -a "$output_file"
        ps -p "$jdbx_pid" -o pid,ppid,rss,vsz,pmem,pcpu,etime,cmd | tee -a "$output_file"
        echo "" | tee -a "$output_file"
        
        echo "🧠 Memory Maps:" | tee -a "$output_file"
        cat "/proc/$jdbx_pid/status" | grep -E "(VmPeak|VmSize|VmRSS|VmData|VmStk|VmExe)" | tee -a "$output_file"
        echo "" | tee -a "$output_file"
        
        echo "📊 Memory Regions:" | tee -a "$output_file"
        cat "/proc/$jdbx_pid/smaps" 2>/dev/null | \
        awk '/^[0-9a-f]/ {addr=$1} /^Size:/ {size+=$2} /^Rss:/ {rss+=$2} /^Pss:/ {pss+=$2} 
             END {print "Total Size: " size " kB"; print "Total RSS: " rss " kB"; print "Total PSS: " pss " kB"}' | tee -a "$output_file"
        
    else
        echo "❌ JDBX process not found" | tee -a "$output_file"
    fi
    
    echo "" | tee -a "$output_file"
    echo "💾 System Memory:" | tee -a "$output_file"
    free -h | tee -a "$output_file"
    
    echo "" | tee -a "$output_file"
    echo "📋 Snapshot saved to: $output_file"
}

# Main execution
case "${1:-snapshot}" in
    "analyze")
        if [ -n "$2" ]; then
            analyze_memory_logs "$2"
        else
            echo "Usage: $0 analyze <log_file>"
            echo "Available logs:"
            ls -la "$LOG_DIR"/*.log 2>/dev/null || echo "No log files found"
        fi
        ;;
    "snapshot")
        get_memory_snapshot
        ;;
    "latest")
        latest_log=$(ls -t "$LOG_DIR"/memory_24h_*.log 2>/dev/null | head -1)
        if [ -n "$latest_log" ]; then
            analyze_memory_logs "$latest_log"
        else
            echo "❌ No memory logs found. Start monitoring first with memory_monitor_24h.sh"
        fi
        ;;
    *)
        echo "Usage: $0 [snapshot|analyze <file>|latest]"
        echo ""
        echo "Commands:"
        echo "  snapshot  - Take current memory snapshot"
        echo "  analyze   - Analyze specific log file"
        echo "  latest    - Analyze latest monitoring log"
        ;;
esac