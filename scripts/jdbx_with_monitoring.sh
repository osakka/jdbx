#!/bin/bash

# JDBX Service with Built-in Memory Monitoring
# Starts JDBX and automatically monitors memory usage

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
JDBX_DIR="$(dirname "$SCRIPT_DIR")"
LOG_DIR="$JDBX_DIR/build/var/memory_logs"

mkdir -p "$LOG_DIR"

echo "🚀 Starting JDBX with Memory Monitoring"
echo "======================================="

# Function to start memory monitoring
start_memory_monitoring() {
    echo "📊 Starting 24-hour memory monitoring..."
    "$SCRIPT_DIR/memory_monitor_24h.sh" &
    local monitor_pid=$!
    echo "$monitor_pid" > "$LOG_DIR/monitor.pid"
    echo "✅ Memory monitor started (PID: $monitor_pid)"
}

# Function to stop memory monitoring
stop_memory_monitoring() {
    if [ -f "$LOG_DIR/monitor.pid" ]; then
        local monitor_pid=$(cat "$LOG_DIR/monitor.pid")
        if kill -0 "$monitor_pid" 2>/dev/null; then
            echo "🛑 Stopping memory monitor (PID: $monitor_pid)..."
            kill -TERM "$monitor_pid"
            rm -f "$LOG_DIR/monitor.pid"
        fi
    fi
}

# Function to get memory status
get_memory_status() {
    echo "📊 Current Memory Status:"
    "$SCRIPT_DIR/detect_memory_leaks.sh" snapshot
    
    if [ -f "$LOG_DIR/monitor.pid" ]; then
        local monitor_pid=$(cat "$LOG_DIR/monitor.pid")
        if kill -0 "$monitor_pid" 2>/dev/null; then
            echo "✅ Memory monitoring active (PID: $monitor_pid)"
        else
            echo "❌ Memory monitoring not running"
            rm -f "$LOG_DIR/monitor.pid"
        fi
    else
        echo "❌ Memory monitoring not active"
    fi
}

# Main command handling
case "${1:-start}" in
    "start")
        echo "🚀 Starting JDBX with monitoring..."
        
        # Start JDBX service
        cd "$JDBX_DIR"
        build/jdbx_runtime.sh start
        
        # Wait a moment for service to start
        sleep 3
        
        # Start memory monitoring
        start_memory_monitoring
        
        echo ""
        echo "✅ JDBX started with memory monitoring"
        echo "📋 Use '$0 status' to check memory usage"
        echo "📋 Use '$0 analyze' to analyze memory patterns"
        ;;
        
    "stop")
        echo "🛑 Stopping JDBX and monitoring..."
        
        # Stop memory monitoring
        stop_memory_monitoring
        
        # Stop JDBX service
        cd "$JDBX_DIR"
        build/jdbx_runtime.sh stop
        
        echo "✅ JDBX and monitoring stopped"
        ;;
        
    "restart")
        echo "🔄 Restarting JDBX with monitoring..."
        "$0" stop
        sleep 2
        "$0" start
        ;;
        
    "status")
        cd "$JDBX_DIR"
        build/jdbx_runtime.sh status
        echo ""
        get_memory_status
        ;;
        
    "analyze")
        echo "🔍 Analyzing memory patterns..."
        "$SCRIPT_DIR/detect_memory_leaks.sh" latest
        ;;
        
    "logs")
        echo "📋 Recent memory logs:"
        ls -la "$LOG_DIR"/*.log 2>/dev/null | tail -10
        echo ""
        echo "📋 Recent alerts:"
        ls -la "$LOG_DIR"/memory_alerts_*.log 2>/dev/null | tail -5
        if [ -f "$(ls -t "$LOG_DIR"/memory_alerts_*.log 2>/dev/null | head -1)" ]; then
            echo ""
            echo "🚨 Latest alerts:"
            tail -20 "$(ls -t "$LOG_DIR"/memory_alerts_*.log 2>/dev/null | head -1)"
        fi
        ;;
        
    "clean")
        echo "🧹 Cleaning old memory logs (keeping last 7 days)..."
        find "$LOG_DIR" -name "*.log" -mtime +7 -delete
        echo "✅ Cleanup complete"
        ;;
        
    *)
        echo "Usage: $0 [start|stop|restart|status|analyze|logs|clean]"
        echo ""
        echo "Commands:"
        echo "  start   - Start JDBX with memory monitoring"
        echo "  stop    - Stop JDBX and monitoring"
        echo "  restart - Restart both services"
        echo "  status  - Show current status and memory usage"
        echo "  analyze - Analyze memory leak patterns"
        echo "  logs    - Show recent logs and alerts"
        echo "  clean   - Clean old log files"
        ;;
esac