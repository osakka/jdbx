#!/bin/bash

# JSONdb Server Monitoring Script
# Monitors for crashes, tracks performance, and logs diagnostics

LOG_FILE="/opt/jsondb/var/monitor.log"
SERVER_LOG="/opt/jsondb/build/var/jsondb.log"
PID_FILE="/opt/jsondb/build/var/jsondb.pid"
CRASH_LOG="/opt/jsondb/var/crash_reports.log"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_message() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') - $1" | tee -a "$LOG_FILE"
}

check_server_status() {
    if [ -f "$PID_FILE" ]; then
        PID=$(cat "$PID_FILE")
        if kill -0 "$PID" 2>/dev/null; then
            return 0  # Server is running
        else
            return 1  # PID file exists but process is dead
        fi
    else
        return 2  # No PID file
    fi
}

log_crash_info() {
    echo "=================== CRASH DETECTED ===================" >> "$CRASH_LOG"
    echo "Time: $(date)" >> "$CRASH_LOG"
    echo "Last 50 lines of server log:" >> "$CRASH_LOG"
    tail -50 "$SERVER_LOG" >> "$CRASH_LOG" 2>/dev/null || echo "No server log available" >> "$CRASH_LOG"
    echo "System info:" >> "$CRASH_LOG"
    echo "Memory usage: $(free -h | grep Mem)" >> "$CRASH_LOG"
    echo "Disk usage: $(df -h /opt/jsondb)" >> "$CRASH_LOG"
    echo "Load average: $(uptime)" >> "$CRASH_LOG"
    echo "Open files: $(lsof -p $PID 2>/dev/null | wc -l)" >> "$CRASH_LOG"
    echo "====================================================" >> "$CRASH_LOG"
    echo "" >> "$CRASH_LOG"
}

monitor_loop() {
    local consecutive_failures=0
    local last_status="unknown"
    
    log_message "Starting JSONdb server monitoring..."
    
    while true; do
        check_server_status
        status=$?
        
        case $status in
            0)  # Server running
                if [ "$last_status" != "running" ]; then
                    echo -e "${GREEN}✓ Server is running (PID: $(cat $PID_FILE))${NC}"
                    log_message "Server is running (PID: $(cat $PID_FILE))"
                    consecutive_failures=0
                fi
                last_status="running"
                
                # Check memory usage
                if [ -f "$PID_FILE" ]; then
                    PID=$(cat "$PID_FILE")
                    MEMORY=$(ps -p "$PID" -o rss= 2>/dev/null || echo "0")
                    if [ "$MEMORY" -gt 100000 ]; then  # > 100MB
                        echo -e "${YELLOW}⚠ High memory usage: ${MEMORY}KB${NC}"
                        log_message "WARNING: High memory usage: ${MEMORY}KB"
                    fi
                fi
                ;;
                
            1)  # Process dead but PID file exists
                echo -e "${RED}✗ Server crashed! (PID file exists but process is dead)${NC}"
                log_message "ERROR: Server crashed - PID file exists but process is dead"
                log_crash_info
                consecutive_failures=$((consecutive_failures + 1))
                last_status="crashed"
                ;;
                
            2)  # No PID file
                echo -e "${RED}✗ Server not running (no PID file)${NC}"
                log_message "ERROR: Server not running - no PID file"
                consecutive_failures=$((consecutive_failures + 1))
                last_status="stopped"
                ;;
        esac
        
        # Check for rapid failures
        if [ "$consecutive_failures" -ge 3 ]; then
            echo -e "${RED}💥 Multiple consecutive failures detected!${NC}"
            log_message "CRITICAL: Multiple consecutive failures ($consecutive_failures)"
            
            # Optional: Auto-restart (uncomment if desired)
            # echo "Attempting auto-restart..."
            # cd /opt/jsondb && build/jsondb_runtime.sh restart
            # consecutive_failures=0
        fi
        
        sleep 5
    done
}

# Check for server log errors
check_recent_errors() {
    if [ -f "$SERVER_LOG" ]; then
        # Look for recent errors (last 100 lines)
        errors=$(tail -100 "$SERVER_LOG" | grep -i "error\|crash\|segfault\|abort\|fatal" | tail -5)
        if [ ! -z "$errors" ]; then
            echo -e "${YELLOW}Recent errors in server log:${NC}"
            echo "$errors"
        fi
    fi
}

# Usage information
usage() {
    echo "Usage: $0 [start|stop|check|errors]"
    echo "  start  - Start monitoring in background"
    echo "  stop   - Stop monitoring"
    echo "  check  - One-time status check"
    echo "  errors - Check for recent errors in logs"
}

case "${1:-start}" in
    start)
        echo "Starting JSONdb server monitoring..."
        check_recent_errors
        monitor_loop
        ;;
    stop)
        pkill -f "monitor_server.sh"
        echo "Monitoring stopped"
        ;;
    check)
        check_server_status
        case $? in
            0) echo -e "${GREEN}✓ Server is running${NC}" ;;
            1) echo -e "${RED}✗ Server crashed${NC}" ;;
            2) echo -e "${RED}✗ Server not running${NC}" ;;
        esac
        check_recent_errors
        ;;
    errors)
        check_recent_errors
        if [ -f "$CRASH_LOG" ]; then
            echo -e "${YELLOW}Crash reports:${NC}"
            tail -50 "$CRASH_LOG"
        fi
        ;;
    *)
        usage
        ;;
esac