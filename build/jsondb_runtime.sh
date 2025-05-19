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
    # Ensure LD_LIBRARY_PATH is set for QuickJS
    if [ -d "/opt/qjs/lib/quickjs" ]; then
        export LD_LIBRARY_PATH="/opt/qjs/lib/quickjs:$LD_LIBRARY_PATH"
    fi

    # Use --verbose instead of --daemon for better debugging if needed
    DAEMON_MODE="--daemon"
    if [ "${DEBUG_MODE}" = "1" ]; then
        DAEMON_MODE="--verbose"
        echo "Running in debug mode (foreground with verbose output)"
    fi
    
    # Force clean any existing processes
    echo "Checking for existing jsondb_server processes..."
    ps -ef | grep jsondb_server | grep -v grep | awk '{print $2}' | xargs -r kill -9
    sleep 1
    echo "Checking for processes using port ${PORT}..."
    if command -v lsof >/dev/null 2>&1; then
        lsof -i :${PORT} | tail -n +2 | awk '{print $2}' | xargs -r kill -9
    fi
    sleep 1

    ./bin/jsondb_server \
      ${DAEMON_MODE} \
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
    
    # Improved check for server startup with longer timeout and better resilience
    local timeout=30  # Increase timeout to 30 seconds
    local elapsed=0
    local interval=2
    
    echo "Waiting up to ${timeout} seconds for server to start..."
    
    while [ $elapsed -lt $timeout ]; do
        # Check if PID file exists
        if [ -f "$PIDFILE" ]; then
            PID=$(cat "$PIDFILE")
            if ps -p "$PID" > /dev/null 2>&1; then
                echo "JSONdb server started successfully on ${HOST}:${PORT} (PID: $PID)"
                # Check if port is actually in use - wait up to 10 seconds for port activation
                local port_check_timeout=10
                local port_check_elapsed=0
                local port_check_interval=1
                
                while [ $port_check_elapsed -lt $port_check_timeout ]; do
                    if is_port_in_use $PORT; then
                        echo "Confirmed port $PORT is active"
                        return 0
                    else
                        echo "Waiting for port $PORT to become active... (${port_check_elapsed}/${port_check_timeout}s)"
                        sleep $port_check_interval
                        port_check_elapsed=$((port_check_elapsed + port_check_interval))
                    fi
                done
                
                echo "Warning: Process is running but port $PORT did not become active within timeout"
                # Consider this a success anyway since the process is running
                return 0
            fi
        fi
        
        # Check for running process directly
        SERVER_PID=$(ps -ef | grep jsondb_server | grep -v grep | grep -v "sudo" | head -1 | awk '{print $2}')
        if [ -n "$SERVER_PID" ]; then
            echo "Found server process with PID: $SERVER_PID"
            # Create the PID file if it doesn't exist
            if [ ! -f "$PIDFILE" ]; then
                echo "$SERVER_PID" > "$PIDFILE"
                echo "Created PID file: $PIDFILE"
            fi
            
            # Check if port is active - wait up to 10 seconds for port activation
            local port_check_timeout=10
            local port_check_elapsed=0
            local port_check_interval=1
            
            while [ $port_check_elapsed -lt $port_check_timeout ]; do
                if is_port_in_use $PORT; then
                    echo "JSONdb server is running and port $PORT is active"
                    return 0
                else
                    echo "Waiting for port $PORT to become active... (${port_check_elapsed}/${port_check_timeout}s)"
                    sleep $port_check_interval
                    port_check_elapsed=$((port_check_elapsed + port_check_interval))
                fi
            done
            
            echo "Warning: Process is running but port $PORT did not become active within timeout"
            # Consider this a success anyway since the process is running
            return 0
        fi
        
        # If the server is still starting up, check the log file
        if [ -f "$LOGFILE" ]; then
            # Check for positive indicators in log
            if grep -q "Socket listening successfully" "$LOGFILE"; then
                echo "Server appears to be starting based on logs (socket listening)"
            elif grep -q "Socket bound successfully" "$LOGFILE"; then
                echo "Server appears to be starting based on logs (socket bound)"
            elif grep -q "Socket created successfully" "$LOGFILE"; then
                echo "Server appears to be starting based on logs (socket created)"
            fi
        fi
        
        sleep $interval
        elapsed=$((elapsed + interval))
        echo "Still waiting for server to start... (${elapsed}/${timeout} seconds)"
    done
    
    # PID file doesn't exist or contains invalid PID
    # Let's check for running process directly one more time
    SERVER_PID=$(ps -ef | grep jsondb_server | grep -v grep | grep -v "sudo" | head -1 | awk '{print $2}')
    
    if [ -n "$SERVER_PID" ]; then
        echo "JSONdb server started successfully on ${HOST}:${PORT} (PID: $SERVER_PID)"
        
        # Create the PID file
        echo "$SERVER_PID" > "$PIDFILE"
        echo "Created PID file: $PIDFILE"
        return 0
    else
        # Check the log file for any clues
        if [ -f "$LOGFILE" ]; then
            echo "Last 10 log lines:"
            tail -n 10 "$LOGFILE"
            
            # Check for specific errors
            if grep -q "Failed to bind socket" "$LOGFILE"; then
                echo "ERROR: Server failed to bind to port $PORT"
                echo "Try using a different port or making sure no other process is using port $PORT"
            elif grep -q "Failed to create socket" "$LOGFILE"; then
                echo "ERROR: Server failed to create socket"
            elif grep -q "Socket bound successfully" "$LOGFILE" && ! grep -q "Socket listening successfully" "$LOGFILE"; then
                echo "ERROR: Socket was bound but failed to listen"
            fi
        fi
        
        echo "Failed to start JSONdb server"
        return 1
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
    echo "Usage: $0 {start|stop|restart|status} [--port=PORT] [--host=HOST] [--validators-dir=DIR] [--transforms-dir=DIR] [--metrics-dir=DIR] [--debug]"
    echo ""
    echo "Options:"
    echo "  --port=PORT            Set server port (default: $PORT)"
    echo "  --host=HOST            Set server host (default: $HOST)" 
    echo "  --validators-dir=DIR   Set validators directory"
    echo "  --transforms-dir=DIR   Set transforms directory"
    echo "  --metrics-dir=DIR      Set metrics directory"
    echo "  --debug                Run in debug mode (verbose output, foreground)" 
    echo "" 
    exit 1
fi

# Set debug mode flag (default: off)
DEBUG_MODE=0

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
        --debug)
            DEBUG_MODE=1
            echo "Debug mode enabled"
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 {start|stop|restart|status} [--port=PORT] [--host=HOST] [--validators-dir=DIR] [--transforms-dir=DIR] [--metrics-dir=DIR] [--debug]"
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
    restart)
        stop_server;
        start_server;
        ;;
    status)
        check_status
        ;;
    *)
        echo "Unknown command: $COMMAND"
        echo "Usage: $0 {start|stop|restart|status} [--port=PORT] [--host=HOST] [--validators-dir=DIR] [--transforms-dir=DIR] [--metrics-dir=DIR]"
        exit 1
        ;;
esac

exit 0
