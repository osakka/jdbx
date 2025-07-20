#!/bin/bash

# Simple memory monitoring script
PID_FILE="/opt/jdbx/build/var/jdbxd.pid"
LOG_FILE="/opt/jdbx/build/var/memory_monitor.log"

echo "Starting memory monitoring..."
echo "Time,PID,RSS(KB),VSZ(KB),CPU%,MEM%" > "$LOG_FILE"

while true; do
    if [ -f "$PID_FILE" ]; then
        PID=$(cat "$PID_FILE")
        if kill -0 "$PID" 2>/dev/null; then
            # Get memory stats
            STATS=$(ps -p "$PID" -o pid,rss,vsz,pcpu,pmem --no-headers | tr -s ' ')
            if [ -n "$STATS" ]; then
                RSS=$(echo "$STATS" | awk '{print $2}')
                VSZ=$(echo "$STATS" | awk '{print $3}')
                CPU=$(echo "$STATS" | awk '{print $4}')
                MEM=$(echo "$STATS" | awk '{print $5}')
                
                RSS_MB=$((RSS / 1024))
                VSZ_MB=$((VSZ / 1024))
                
                TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')
                echo "$TIMESTAMP,$PID,$RSS,$VSZ,$CPU,$MEM" >> "$LOG_FILE"
                
                # Display current status
                echo "[$TIMESTAMP] PID:$PID RSS:${RSS_MB}MB VSZ:${VSZ_MB}MB CPU:${CPU}% MEM:${MEM}%"
            fi
        else
            echo "[$TIMESTAMP] Server not running (PID $PID not found)"
            exit 1
        fi
    else
        echo "[$TIMESTAMP] PID file not found"
        exit 1
    fi
    
    sleep 5
done