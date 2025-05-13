#!/bin/bash
# Run script for JSONdb server

DBPATH='./var/jsondb_database.json';
RBACFILE='./var/json_rbac.json';
LOGFILE='./var/jsondb_server.log';
PIDFILE='./var/jsondb_server.pid';
WEBROOT='../share/htdocs';
LOGLEVEL='debug';

# Go to the build directory
cd "$(dirname "$0")"

# Add QuickJS library directory to library path if needed
if [ -d "/opt/qjs/lib/quickjs" ]; then
    export LD_LIBRARY_PATH="/opt/qjs/lib/quickjs:$LD_LIBRARY_PATH"
fi

# Check if server is running with better stale PID file handling
check_status() {
    if [ -f "$PIDFILE" ]; then
        PID=$(cat "$PIDFILE")
        if ps -p "$PID" > /dev/null 2>&1; then
            echo "JSONdb server is running (PID: $PID)"
            return 0
        else
            echo "JSONdb server is not running (stale PID file exists)"
            # Remove stale PID file
            rm -f "$PIDFILE"
            return 1
        fi
    else
        echo "JSONdb server is not running"
        return 1
    fi
}

# Start the server with better directory handling and error checking
start_server() {
    if check_status > /dev/null; then
        echo "JSONdb server is already running"
    else
        # Create necessary directories if they don't exist
        for DIR in $(dirname "$LOGFILE") $(dirname "$PIDFILE") $(dirname "$DBPATH"); do
            if [ ! -d "$DIR" ]; then
                echo "Creating directory: $DIR"
                mkdir -p "$DIR"
            fi
        done
        
        echo "Starting JSONdb server..."
        ./bin/jsondb_server \
          --daemon \
          --log-level=${LOGLEVEL} \
          --db-dir=${DBPATH} \
          --rbac-file=${RBACFILE} \
          --log-file=${LOGFILE} \
          --pid-file=${PIDFILE} \
          --web-root=${WEBROOT}
        
        # Give the server a moment to start
        sleep 2
        
        if check_status > /dev/null; then
            echo "JSONdb server started successfully"
        else
            echo "Failed to start JSONdb server. Check logs at:"
            echo "  Log file: $LOGFILE"
            # Check if anything was logged
            if [ -f "$LOGFILE" ]; then
                echo "Last 5 log lines:"
                tail -n 5 "$LOGFILE"
            fi
        fi
    fi
}

# Stop the server with grace period and force kill if needed
stop_server() {
    if check_status > /dev/null; then
        PID=$(cat "$PIDFILE")
        echo "Stopping JSONdb server (PID: $PID)..."
        
        # Try graceful termination first using built-in terminate option
        ./bin/jsondb_server --terminate --pid-file=${PIDFILE}
        
        # Wait for the server to stop (with a timeout)
        for i in {1..5}; do
            sleep 1
            if ! check_status > /dev/null; then
                echo "JSONdb server stopped successfully"
                return 0
            fi
        done
        
        # If we get here, the server didn't stop gracefully
        echo "Warning: JSONdb server did not stop gracefully, forcing termination..."
        PID=$(cat "$PIDFILE")
        kill -9 $PID 2>/dev/null
        rm -f "$PIDFILE"
        echo "JSONdb server terminated forcefully"
    else
        echo "JSONdb server is not running"
    fi
}

# Restart the server
restart_server() {
    echo "Restarting JSONdb server..."
    stop_server
    sleep 1
    start_server
}

# Show usage if no arguments
if [ $# -eq 0 ]; then
    echo "Usage: $0 {start|stop|status|restart}"
    echo "  start   - Start the JSONdb server"
    echo "  stop    - Stop the JSONdb server"
    echo "  status  - Check if the JSONdb server is running"
    echo "  restart - Restart the JSONdb server"
    exit 1
fi

# Process command
case "$1" in
    start)
        start_server
        ;;
    stop)
        stop_server
        ;;
    status)
        check_status
        ;;
    restart)
        restart_server
        ;;
    *)
        echo "Unknown command: $1"
        echo "Usage: $0 {start|stop|status|restart}"
        exit 1
        ;;
esac

exit 0
