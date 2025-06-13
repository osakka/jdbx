#!/bin/bash
# Simple JDBX Server Management Script

BASE_DIR="/opt/jdbx"
DAEMON="$BASE_DIR/build/bin/jdbxd"
PID_FILE="$BASE_DIR/build/var/jdbxd.pid"

case "$1" in
    start)
        echo "Starting JDBX server..."
        cd "$BASE_DIR"
        # Force kill any existing processes first
        pkill -f jdbxd 2>/dev/null || true
        sleep 1
        rm -f "$PID_FILE"
        JDBX_DEFERRED_BOOTSTRAP=1 JDBX_INITIAL_ADMIN_USER=admin JDBX_INITIAL_ADMIN_PASSWORD=admin $DAEMON --daemon
        ;;
    stop)
        echo "Stopping JDBX server..."
        cd "$BASE_DIR"
        # Try graceful shutdown first
        if [ -f "$PID_FILE" ]; then
            PID=$(cat "$PID_FILE")
            kill "$PID" 2>/dev/null || true
            sleep 2
        fi
        # Force kill any remaining processes
        pkill -f jdbxd 2>/dev/null || true
        rm -f "$PID_FILE"
        ;;
    restart)
        echo "Restarting JDBX server..."
        "$0" stop
        sleep 3
        "$0" start
        ;;
    status)
        if [ -f "$PID_FILE" ]; then
            PID=$(cat $PID_FILE)
            if ps -p $PID > /dev/null 2>&1; then
                echo "JDBX server is running (PID: $PID)"
            else
                echo "JDBX server is not running (stale PID file)"
                rm -f "$PID_FILE"
            fi
        else
            echo "JDBX server is not running"
        fi
        ;;
    *)
        echo "Usage: $0 {start|stop|restart|status}"
        exit 1
        ;;
esac