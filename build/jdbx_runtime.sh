#!/bin/bash
# JDBX Server Management Script - Configurable Paths
# Supports environment variables for path configuration

# Auto-detect base directory from script location
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEFAULT_BASE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Use environment variables with auto-detected fallbacks
BASE_DIR="${JDBX_BASE_PATH:-$DEFAULT_BASE_DIR}"
DAEMON="${JDBX_DAEMON:-$BASE_DIR/build/bin/jdbxd}"
PID_FILE="${JDBX_PID_FILE:-$BASE_DIR/build/var/jdbxd.pid}"
VAR_DIR="${JDBX_VAR_PATH:-$BASE_DIR/build/var}"
ENV_FILE="${JDBX_ENV_FILE:-$VAR_DIR/jdbx.env}"

case "$1" in
    start)
        echo "Starting JDBX server..."
        cd "$BASE_DIR"
        # Force kill any existing processes first
        pkill -f jdbxd 2>/dev/null || true
        sleep 1
        rm -f "$PID_FILE"
        # Clean database for fresh start (use configurable paths)
        rm -f "$VAR_DIR/database.jdb" "$VAR_DIR/jdbx.jdbx" "$VAR_DIR/jdbx.wal"
        # Use configurable environment and daemon path
        JDBX_DEFERRED_BOOTSTRAP=1 JDBX_INITIAL_ADMIN_USER=admin JDBX_INITIAL_ADMIN_PASSWORD=admin "$DAEMON" --daemon
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
            PID=$(cat "$PID_FILE")
            if ps -p "$PID" > /dev/null 2>&1; then
                echo "JDBX server is running (PID: $PID)"
                echo "Base directory: $BASE_DIR"
                echo "PID file: $PID_FILE"
                echo "Environment file: $ENV_FILE"
            else
                echo "JDBX server is not running (stale PID file)"
                rm -f "$PID_FILE"
            fi
        else
            echo "JDBX server is not running"
            echo "Configuration:"
            echo "  Base directory: $BASE_DIR"
            echo "  Daemon binary: $DAEMON"
            echo "  PID file: $PID_FILE"
            echo "  Environment file: $ENV_FILE"
        fi
        ;;
    *)
        echo "Usage: $0 {start|stop|restart|status}"
        exit 1
        ;;
esac