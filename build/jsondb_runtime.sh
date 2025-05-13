#!/bin/bash
# Simple run script for JSONdb server

# Configuration
DBPATH='./var/jsondb_database.json'
RBACFILE='./var/json_rbac.json'
LOGFILE='./var/jsondb_server.log'
PIDFILE='./var/jsondb_server.pid'
WEBROOT='../share/htdocs'
LOGLEVEL='debug'
PORT=5000
HOST='0.0.0.0'
VALIDATORS_DIR='./var/validators'
TRANSFORMS_DIR='./var/transforms'
METRICS_DIR='./var/metrics'

# Go to the build directory
cd "$(dirname "$0")"

# Add QuickJS library directory to library path if needed
if [ -d "/opt/qjs/lib/quickjs" ]; then
    export LD_LIBRARY_PATH="/opt/qjs/lib/quickjs:$LD_LIBRARY_PATH"
fi

# Simple check if server is running (via PID file)
check_status() {
    if [ -f "$PIDFILE" ]; then
        PID=$(cat "$PIDFILE")
        if ps -p "$PID" > /dev/null 2>&1; then
            echo "JSONdb server is running (PID: $PID)"
            return 0
        else
            echo "JSONdb server is not running (stale PID file exists)"
            rm -f "$PIDFILE"
            return 1
        fi
    else
        echo "JSONdb server is not running (no PID file)"
        return 1
    fi
}

# Check if a port is in use
is_port_in_use() {
    local port=$1
    # The return value is inverted - we want to return 0 (true) if port is in use
    if command -v lsof >/dev/null 2>&1; then
        if lsof -i :$port >/dev/null 2>&1; then
            return 0  # Port is in use
        else
            return 1  # Port is not in use
        fi
    elif command -v netstat >/dev/null 2>&1; then
        if netstat -tuln | grep ":$port " >/dev/null 2>&1; then
            return 0  # Port is in use
        else
            return 1  # Port is not in use
        fi
    else
        # Fall back to a direct connection test
        if (echo > /dev/tcp/localhost/$port) >/dev/null 2>&1; then
            return 0  # Port is in use
        else
            return 1  # Port is not in use
        fi
    fi
}

# Start the server
start_server() {
    if [ -f "$PIDFILE" ]; then
        PID=$(cat "$PIDFILE")
        if ps -p "$PID" > /dev/null 2>&1; then
            echo "JSONdb server is already running"
            return 0
        fi
        rm -f "$PIDFILE"
    fi

    # Check if port is already in use
    if is_port_in_use $PORT; then
        echo "Error: Port $PORT is already in use. Please use a different port."
        return 1
    fi

    # Create necessary directories
    mkdir -p $(dirname "$LOGFILE") $(dirname "$PIDFILE") $(dirname "$DBPATH")
    
    echo "Starting JSONdb server on ${HOST}:${PORT}..."
    ./bin/jsondb_server \
      --daemon \
      --log-level=${LOGLEVEL} \
      --db-dir=${DBPATH} \
      --rbac-file=${RBACFILE} \
      --log-file=${LOGFILE} \
      --pid-file=${PIDFILE} \
      --web-root=${WEBROOT} \
      --port=${PORT} \
      --host=${HOST} \
      --validators-dir=${VALIDATORS_DIR} \
      --transforms-dir=${TRANSFORMS_DIR} \
      --metrics-dir=${METRICS_DIR}
    
    # Simple check for server startup
    sleep 2
    
    # First check if PID file exists
    if [ -f "$PIDFILE" ]; then
        PID=$(cat "$PIDFILE")
        if ps -p "$PID" > /dev/null 2>&1; then
            echo "JSONdb server started successfully on ${HOST}:${PORT} (PID: $PID)"
            return 0
        fi
    fi
    
    # PID file doesn't exist or contains invalid PID
    # Let's check for running process directly
    SERVER_PID=$(ps -ef | grep jsondb_server | grep -v grep | grep "port=${PORT}" | awk '{print $2}')
    
    if [ -n "$SERVER_PID" ]; then
        echo "JSONdb server started successfully on ${HOST}:${PORT} (PID: $SERVER_PID)"
        
        # Create the PID file
        echo "$SERVER_PID" > "$PIDFILE"
        echo "Created PID file: $PIDFILE"
        return 0
    else
        # Check the log file for success message
        if [ -f "$LOGFILE" ] && grep -q "Server running in daemon mode on" "$LOGFILE"; then
            echo "JSONdb server appears to be running based on logs"
            echo "Last 5 log lines:"
            tail -n 5 "$LOGFILE"
            return 0
        else
            echo "Failed to start JSONdb server"
            echo "Last 5 log lines:"
            tail -n 5 "$LOGFILE"
            return 1
        fi
    fi
}

# Stop the server
stop_server() {
    if [ -f "$PIDFILE" ]; then
        PID=$(cat "$PIDFILE")
        if ps -p "$PID" > /dev/null 2>&1; then
            echo "Stopping JSONdb server (PID: $PID)..."
            ./bin/jsondb_server --terminate --pid-file=${PIDFILE}
            sleep 2
            if ps -p "$PID" > /dev/null 2>&1; then
                echo "Forcing termination of JSONdb server..."
                kill -9 $PID
            fi
        else
            echo "No running server found (stale PID file)"
        fi
        rm -f "$PIDFILE"
    else
        echo "JSONdb server is not running (no PID file)"
    fi
}

# Show usage if no arguments
if [ $# -eq 0 ]; then
    echo "Usage: $0 {start|stop|status} [--port=PORT] [--host=HOST] [--validators-dir=DIR] [--transforms-dir=DIR] [--metrics-dir=DIR]"
    exit 1
fi

# Process command
COMMAND=$1
shift

# Parse remaining arguments
while [ $# -gt 0 ]; do
    case "$1" in
        --port=*)
            PORT="${1#*=}"
            ;;
        --host=*)
            HOST="${1#*=}"
            ;;
        --validators-dir=*)
            VALIDATORS_DIR="${1#*=}"
            ;;
        --transforms-dir=*)
            TRANSFORMS_DIR="${1#*=}"
            ;;
        --metrics-dir=*)
            METRICS_DIR="${1#*=}"
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 {start|stop|status} [--port=PORT] [--host=HOST] [--validators-dir=DIR] [--transforms-dir=DIR] [--metrics-dir=DIR]"
            exit 1
            ;;
    esac
    shift
done

case "$COMMAND" in
    start)
        start_server
        ;;
    stop)
        stop_server
        ;;
    status)
        check_status
        ;;
    *)
        echo "Unknown command: $COMMAND"
        echo "Usage: $0 {start|stop|status} [--port=PORT] [--host=HOST] [--validators-dir=DIR] [--transforms-dir=DIR] [--metrics-dir=DIR]"
        exit 1
        ;;
esac

exit 0
