#!/bin/bash
# Enhanced run script for JSONdb server with environment variable support

# Go to the build directory first
cd "$(dirname "$0")"

# Source environment configuration if exists
ENV_FILE="./var/jsondb_server.env"
if [ -f "$ENV_FILE" ]; then
    echo "Loading configuration from $ENV_FILE"
    source "$ENV_FILE"
else
    echo "Warning: Environment file $ENV_FILE not found, using defaults"
fi

# Allow for custom environment file from command line (--env-file parameter)
for arg in "$@"; do
    if [[ "$arg" == --env-file=* ]]; then
        CUSTOM_ENV_FILE="${arg#*=}"
        if [ -f "$CUSTOM_ENV_FILE" ]; then
            echo "Loading custom environment file: $CUSTOM_ENV_FILE"
            source "$CUSTOM_ENV_FILE"
            ENV_FILE="$CUSTOM_ENV_FILE"
        else
            echo "Error: Custom environment file not found: $CUSTOM_ENV_FILE"
            exit 1
        fi
        break
    fi
done

# Set default values if not in environment
: ${JSONDB_PORT:=5000}
: ${JSONDB_HOST:="0.0.0.0"}
: ${JSONDB_VERBOSE:="false"}
: ${JSONDB_LOG_LEVEL:="debug"}

# Set base paths if not already defined
: ${JSONDB_BASE_DIR:="/opt/jsondb"}
: ${JSONDB_BUILD_DIR:="${JSONDB_BASE_DIR}/build"}
: ${JSONDB_VAR_DIR:="${JSONDB_BASE_DIR}/var"}
: ${JSONDB_SHARE_DIR:="${JSONDB_BASE_DIR}/share"}

# Ensure absolute paths using base directories
: ${JSONDB_DB_DIR:="${JSONDB_VAR_DIR}/jsondb_database.json"}
: ${JSONDB_RBAC_FILE:="${JSONDB_VAR_DIR}/json_rbac.json"} 
: ${JSONDB_LOG_FILE:="${JSONDB_VAR_DIR}/jsondb_server.log"}
: ${JSONDB_PID_FILE:="${JSONDB_VAR_DIR}/jsondb_server.pid"}
: ${JSONDB_WEB_ROOT:="${JSONDB_SHARE_DIR}/htdocs"}
: ${JSONDB_VALIDATORS_DIR:="${JSONDB_VAR_DIR}/validators"}
: ${JSONDB_TRANSFORMS_DIR:="${JSONDB_VAR_DIR}/transforms"}
: ${JSONDB_METRICS_DIR:="${JSONDB_VAR_DIR}/metrics"}

# Set configuration from environment to variables used in script
DBPATH="$JSONDB_DB_DIR"
RBACFILE="$JSONDB_RBAC_FILE"
LOGFILE="$JSONDB_LOG_FILE"
PIDFILE="$JSONDB_PID_FILE"
WEBROOT="$JSONDB_WEB_ROOT"
LOGLEVEL="$JSONDB_LOG_LEVEL"
PORT=$JSONDB_PORT
HOST="$JSONDB_HOST"
VALIDATORS_DIR="$JSONDB_VALIDATORS_DIR"
TRANSFORMS_DIR="$JSONDB_TRANSFORMS_DIR"
METRICS_DIR="$JSONDB_METRICS_DIR"

# Display configuration for debugging
echo "Using configuration:"
echo "  Database path: $DBPATH"
echo "  RBAC file: $RBACFILE"
echo "  Log file: $LOGFILE"
echo "  PID file: $PIDFILE"
echo "  Host: $HOST"
echo "  Port: $PORT"

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

# Parse remaining arguments (these override environment variables)
while [ $# -gt 0 ]; do
    case "$1" in
        --port=*)
            PORT="${1#*=}"
            JSONDB_PORT="$PORT"
            ;;
        --host=*)
            HOST="${1#*=}"
            JSONDB_HOST="$HOST"
            ;;
        --db-dir=*|--db-path=*)
            DBPATH="${1#*=}"
            JSONDB_DB_DIR="$DBPATH"
            ;;
        --rbac-file=*)
            RBACFILE="${1#*=}"
            JSONDB_RBAC_FILE="$RBACFILE"
            ;;
        --log-file=*)
            LOGFILE="${1#*=}"
            JSONDB_LOG_FILE="$LOGFILE"
            ;;
        --pid-file=*)
            PIDFILE="${1#*=}"
            JSONDB_PID_FILE="$PIDFILE"
            ;;
        --web-root=*)
            WEBROOT="${1#*=}"
            JSONDB_WEB_ROOT="$WEBROOT"
            ;;
        --log-level=*)
            LOGLEVEL="${1#*=}"
            JSONDB_LOG_LEVEL="$LOGLEVEL"
            ;;
        --validators-dir=*)
            VALIDATORS_DIR="${1#*=}"
            JSONDB_VALIDATORS_DIR="$VALIDATORS_DIR"
            ;;
        --transforms-dir=*)
            TRANSFORMS_DIR="${1#*=}"
            JSONDB_TRANSFORMS_DIR="$TRANSFORMS_DIR"
            ;;
        --metrics-dir=*)
            METRICS_DIR="${1#*=}"
            JSONDB_METRICS_DIR="$METRICS_DIR"
            ;;
        --max-connections=*)
            JSONDB_MAX_CONNECTIONS="${1#*=}"
            ;;
        --debug)
            DEBUG_MODE=1
            JSONDB_DEBUG_MODE="true"
            JSONDB_VERBOSE="true"
            echo "Debug mode enabled"
            ;;
        --env-file=*)
            ENV_FILE="${1#*=}"
            if [ -f "$ENV_FILE" ]; then
                echo "Loading custom environment file: $ENV_FILE"
                source "$ENV_FILE"
            else
                echo "Error: Custom environment file not found: $ENV_FILE"
                exit 1
            fi
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 {start|stop|restart|status|run} [OPTIONS]"
            echo "Options:"
            echo "  --port=PORT                Set server port"
            echo "  --host=HOST                Set server bind address"
            echo "  --db-dir=PATH              Set database directory/file"
            echo "  --rbac-file=FILE           Set RBAC file path"
            echo "  --log-file=FILE            Set log file path"
            echo "  --pid-file=FILE            Set PID file path"
            echo "  --log-level=LEVEL          Set log level (error, warn, info, debug, trace)"
            echo "  --web-root=DIR             Set web root directory"
            echo "  --validators-dir=DIR       Set validators directory"
            echo "  --transforms-dir=DIR       Set transforms directory"
            echo "  --metrics-dir=DIR          Set metrics directory"
            echo "  --max-connections=NUM      Set maximum connections"
            echo "  --debug                    Enable debug mode"
            echo "  --env-file=FILE            Use custom environment file"
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
