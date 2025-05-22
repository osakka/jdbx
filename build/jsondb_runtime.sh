#!/bin/bash
# JSONdb Server Runtime Script
# Simplified version for the v2.0.0 binary persistence release

# Go to the script's directory
cd "$(dirname "$0")"

# Default environment file paths (in order of preference)
ENV_FILES=(
    "./var/jsondb_server.env"
    "../var/jsondb_server.env" 
    "../share/config/jsondb_server.env"
)

# Load environment configuration
ENV_FILE=""
for file in "${ENV_FILES[@]}"; do
    if [ -f "$file" ]; then
        ENV_FILE="$file"
        echo "Loading configuration from $ENV_FILE"
        source "$ENV_FILE"
        break
    fi
done

# Set default values if not defined in environment
: ${JSONDB_PORT:=5000}
: ${JSONDB_HOST:="0.0.0.0"}
: ${JSONDB_LOG_LEVEL:="info"}

# Use v2.0.0 binary format with .jdb extension
: ${JSONDB_BASE_DIR:="/opt/jsondb"}
: ${JSONDB_BUILD_DIR:="${JSONDB_BASE_DIR}/build"}
: ${JSONDB_VAR_DIR:="${JSONDB_BUILD_DIR}/var"}
: ${JSONDB_SHARE_DIR:="${JSONDB_BASE_DIR}/share"}

# Updated paths for v2.0.0 binary persistence
: ${JSONDB_DB_DIR:="${JSONDB_VAR_DIR}/database.jdb"}
: ${JSONDB_RBAC_FILE:="${JSONDB_VAR_DIR}/rbac.json"}
: ${JSONDB_LOG_FILE:="${JSONDB_VAR_DIR}/jsondb.log"}
: ${JSONDB_PID_FILE:="${JSONDB_VAR_DIR}/jsondb.pid"}
: ${JSONDB_WEB_ROOT:="${JSONDB_SHARE_DIR}/htdocs"}
: ${JSONDB_VALIDATORS_DIR:="${JSONDB_VAR_DIR}/validators"}
: ${JSONDB_TRANSFORMS_DIR:="${JSONDB_VAR_DIR}/transforms"}
: ${JSONDB_METRICS_DIR:="${JSONDB_VAR_DIR}/metrics"}

# Display current configuration
echo "Using configuration:"
echo "  Database path: $JSONDB_DB_DIR"
echo "  RBAC file: $JSONDB_RBAC_FILE"
echo "  Log file: $JSONDB_LOG_FILE"
echo "  PID file: $JSONDB_PID_FILE"
echo "  Host: $JSONDB_HOST"
echo "  Port: $JSONDB_PORT"

# QuickJS library path
if [ -d "/opt/qjs/lib/quickjs" ]; then
    export LD_LIBRARY_PATH="/opt/qjs/lib/quickjs:$LD_LIBRARY_PATH"
fi

# Simple server status check
check_status() {
    if [ -f "$JSONDB_PID_FILE" ]; then
        PID=$(cat "$JSONDB_PID_FILE")
        if ps -p "$PID" > /dev/null 2>&1; then
            echo "JSONdb server is running (PID: $PID)"
            return 0
        else
            echo "No running server found with PID $PID (stale PID file)"
            rm -f "$JSONDB_PID_FILE"
            return 1
        fi
    else
        echo "JSONdb server is not running (no PID file)"
        return 1
    fi
}

# Check if port is in use
is_port_in_use() {
    local port=$1
    if command -v ss >/dev/null 2>&1; then
        ss -tuln | grep ":$port " >/dev/null 2>&1
    elif command -v netstat >/dev/null 2>&1; then
        netstat -tuln | grep ":$port " >/dev/null 2>&1
    else
        (echo > /dev/tcp/localhost/$port) >/dev/null 2>&1
    fi
}

# Start the server
start_server() {
    # Check if already running
    if check_status; then
        echo "JSONdb server is already running"
        return 0
    fi

    # Check port availability
    if is_port_in_use $JSONDB_PORT; then
        echo "Error: Port $JSONDB_PORT is already in use"
        return 1
    fi

    # Create necessary directories
    mkdir -p "$(dirname "$JSONDB_LOG_FILE")" "$(dirname "$JSONDB_PID_FILE")" "$(dirname "$JSONDB_DB_DIR")"
    
    echo "Starting JSONdb server on $JSONDB_HOST:$JSONDB_PORT..."

    # Check for existing jsondb_server processes
    echo "Checking for existing jsondb_server processes..."
    pkill -f "jsondb_server" 2>/dev/null || true
    
    # Check for processes using the port
    echo "Checking for processes using port $JSONDB_PORT..."
    if command -v lsof >/dev/null 2>&1; then
        lsof -ti:$JSONDB_PORT | xargs -r kill 2>/dev/null || true
    fi

    # Start the server
    ./bin/jsondb_server \
        --daemon \
        --log-level="$JSONDB_LOG_LEVEL" \
        --db-dir="$JSONDB_DB_DIR" \
        --rbac-file="$JSONDB_RBAC_FILE" \
        --log-file="$JSONDB_LOG_FILE" \
        --pid-file="$JSONDB_PID_FILE" \
        --web-root="$JSONDB_WEB_ROOT" \
        --port="$JSONDB_PORT" \
        --host="$JSONDB_HOST" \
        --validators-dir="$JSONDB_VALIDATORS_DIR" \
        --transforms-dir="$JSONDB_TRANSFORMS_DIR" \
        --metrics-dir="$JSONDB_METRICS_DIR"

    # Wait for server to start (simplified)
    echo "Waiting up to 30 seconds for server to start..."
    for i in {1..30}; do
        if [ -f "$JSONDB_PID_FILE" ]; then
            PID=$(cat "$JSONDB_PID_FILE")
            if ps -p "$PID" > /dev/null 2>&1; then
                if is_port_in_use $JSONDB_PORT; then
                    echo "JSONdb server is running and port $JSONDB_PORT is active"
                    return 0
                fi
            fi
        fi
        
        # Check for process directly
        if pgrep -f "jsondb_server" > /dev/null; then
            echo "Found server process with PID: $(pgrep -f jsondb_server)"
            if is_port_in_use $JSONDB_PORT; then
                echo "JSONdb server is running and port $JSONDB_PORT is active"
                return 0
            fi
        fi
        
        echo "Still waiting for server to start... ($i/30 seconds)"
        sleep 1
    done

    echo "Failed to start JSONdb server"
    return 1
}

# Stop the server
stop_server() {
    # Try PID file first
    if [ -f "$JSONDB_PID_FILE" ]; then
        PID=$(cat "$JSONDB_PID_FILE")
        if ps -p "$PID" > /dev/null 2>&1; then
            echo "Stopping JSONdb server (PID: $PID)..."
            kill -TERM $PID
            
            # Wait for graceful shutdown
            for i in {1..5}; do
                if ! ps -p "$PID" > /dev/null 2>&1; then
                    echo "Process $PID successfully stopped via SIGTERM."
                    break
                fi
                sleep 1
            done
            
            # Force kill if still running
            if ps -p "$PID" > /dev/null 2>&1; then
                echo "Forcing termination of process $PID..."
                kill -9 $PID
            fi
        else
            echo "No running server found with PID $PID (stale PID file)"
        fi
        rm -f "$JSONDB_PID_FILE"
    else
        echo "No PID file found at $JSONDB_PID_FILE"
    fi
    
    # Find and stop any remaining jsondb_server processes
    echo "Searching for jsondb_server processes..."
    SERVER_PIDS=$(pgrep -f "jsondb_server" 2>/dev/null)
    
    if [ -n "$SERVER_PIDS" ]; then
        echo "Found jsondb_server processes: $SERVER_PIDS"
        for pid in $SERVER_PIDS; do
            echo "Stopping JSONdb server process (PID: $pid)..."
            kill -TERM $pid 2>/dev/null
            sleep 2
            if ps -p "$pid" > /dev/null 2>&1; then
                kill -9 $pid 2>/dev/null
            fi
        done
    fi

    # Check final status
    if is_port_in_use $JSONDB_PORT; then
        echo "WARNING: Port $JSONDB_PORT is still in use after stopping server"
    else
        echo "Server successfully stopped and port $JSONDB_PORT is now available."
    fi
}

# Show usage
show_usage() {
    echo "Usage: $0 {start|stop|restart|status} [OPTIONS]"
    echo ""
    echo "Commands:"
    echo "  start     Start the JSONdb server"
    echo "  stop      Stop the JSONdb server"
    echo "  restart   Restart the JSONdb server"
    echo "  status    Show server status"
    echo ""
    echo "Options:"
    echo "  --port=PORT            Set server port (default: $JSONDB_PORT)"
    echo "  --host=HOST            Set server host (default: $JSONDB_HOST)"
    echo "  --log-level=LEVEL      Set log level (error, warn, info, debug, trace)"
    echo "  --db-dir=PATH          Set database file path"
    echo "  --env-file=FILE        Use custom environment file"
    echo ""
    echo "Environment file locations (in order of preference):"
    for file in "${ENV_FILES[@]}"; do
        echo "  $file"
    done
}

# Parse command line arguments
COMMAND=""
while [ $# -gt 0 ]; do
    case "$1" in
        start|stop|restart|status)
            COMMAND="$1"
            ;;
        --port=*)
            JSONDB_PORT="${1#*=}"
            ;;
        --host=*)
            JSONDB_HOST="${1#*=}"
            ;;
        --log-level=*)
            JSONDB_LOG_LEVEL="${1#*=}"
            ;;
        --db-dir=*)
            JSONDB_DB_DIR="${1#*=}"
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
        -h|--help)
            show_usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
    shift
done

# Execute command
case "$COMMAND" in
    start)
        start_server
        ;;
    stop)
        stop_server
        ;;
    restart)
        stop_server
        sleep 2
        start_server
        ;;
    status)
        check_status
        ;;
    "")
        show_usage
        exit 1
        ;;
    *)
        echo "Unknown command: $COMMAND"
        show_usage
        exit 1
        ;;
esac