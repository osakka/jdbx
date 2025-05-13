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

# Check if server is running
check_status() {
    if [ -f "$PIDFILE" ]; then
        PID=$(cat "$PIDFILE")
        if ps -p "$PID" > /dev/null 2>&1; then
            echo "JSONdb server is running (PID: $PID)"
            return 0
        else
            echo "JSONdb server is not running (stale PID file exists)"
            return 1
        fi
    else
        echo "JSONdb server is not running"
        return 1
    fi
}

# Start the server
start_server() {
    if check_status > /dev/null; then
        echo "JSONdb server is already running"
    else
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
        sleep 1
        
        if check_status > /dev/null; then
            echo "JSONdb server started successfully"
        else
            echo "Failed to start JSONdb server. Check the log at: $LOGFILE"
        fi
    fi
}

# Stop the server
stop_server() {
    if check_status > /dev/null; then
        echo "Stopping JSONdb server..."
        ./bin/jsondb_server --terminate --pid-file=${PIDFILE}
        
        # Wait for the server to stop
        sleep 1
        
        if check_status > /dev/null; then
            echo "Warning: JSONdb server did not stop gracefully"
        else
            echo "JSONdb server stopped successfully"
        fi
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
