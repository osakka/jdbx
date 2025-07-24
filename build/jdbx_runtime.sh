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

# Security: Load admin credentials from environment file or require manual configuration
load_admin_credentials() {
    # Save command-line environment variables before loading file
    CMDLINE_USER="$JDBX_BOOTSTRAP_ADMIN_USER"
    CMDLINE_PASS="$JDBX_BOOTSTRAP_ADMIN_PASS"
    CMDLINE_EMAIL="$JDBX_DEFAULT_ADMIN_EMAIL"
    
    # DEVELOPMENT STRATEGY: Always ensure latest canonical config is used
    # Copy canonical config from share/config/ to build/var/ if source is newer or target missing
    CANONICAL_ENV="$BASE_DIR/share/config/jdbx.env"
    if [ -f "$CANONICAL_ENV" ]; then
        if [ ! -f "$ENV_FILE" ] || [ "$CANONICAL_ENV" -nt "$ENV_FILE" ]; then
            echo "📝 Updating configuration from canonical source..."
            echo "   Source: $CANONICAL_ENV"
            echo "   Target: $ENV_FILE"
            cp "$CANONICAL_ENV" "$ENV_FILE"
        fi
    fi
    
    # Load from environment file if it exists
    if [ -f "$ENV_FILE" ]; then
        source "$ENV_FILE"
    fi
    
    # Command-line variables take precedence over environment file
    if [ -n "$CMDLINE_USER" ]; then
        JDBX_BOOTSTRAP_ADMIN_USER="$CMDLINE_USER"
    fi
    if [ -n "$CMDLINE_PASS" ]; then
        JDBX_BOOTSTRAP_ADMIN_PASS="$CMDLINE_PASS"
    fi
    if [ -n "$CMDLINE_EMAIL" ]; then
        JDBX_DEFAULT_ADMIN_EMAIL="$CMDLINE_EMAIL"
    fi
    
    # Check if admin credentials are configured
    if [ -z "$JDBX_BOOTSTRAP_ADMIN_USER" ] || [ -z "$JDBX_BOOTSTRAP_ADMIN_PASS" ]; then
        echo "ERROR: Bootstrap admin credentials not configured!"
        echo "Please set JDBX_BOOTSTRAP_ADMIN_USER and JDBX_BOOTSTRAP_ADMIN_PASS environment variables"
        echo "or add them to $ENV_FILE"
        echo ""
        echo "Example:"
        echo "  export JDBX_BOOTSTRAP_ADMIN_USER=your_admin_username"
        echo "  export JDBX_BOOTSTRAP_ADMIN_PASS=your_secure_password"
        echo "  export JDBX_DEFAULT_ADMIN_EMAIL=admin@yourdomain.com"
        echo ""
        echo "For testing, you can use:"
        echo "  JDBX_BOOTSTRAP_ADMIN_USER=admin JDBX_BOOTSTRAP_ADMIN_PASS=secure123 $0 start"
        return 1
    fi
    
    # Validate password length (minimum 12 characters for security)
    if [ ${#JDBX_BOOTSTRAP_ADMIN_PASS} -lt 12 ]; then
        echo "WARNING: Admin password is shorter than 12 characters (current: ${#JDBX_BOOTSTRAP_ADMIN_PASS})"
        echo "For production use, please use a password with at least 12 characters"
    fi
    
    return 0
}

case "$1" in
    start)
        echo "Starting JDBX server..."
        cd "$BASE_DIR"
        
        # Load and validate admin credentials
        if ! load_admin_credentials; then
            echo "CRITICAL: Cannot start server without secure admin credentials!"
            exit 1
        fi
        
        echo "✅ Admin credentials loaded: User=$JDBX_BOOTSTRAP_ADMIN_USER"
        echo "🔐 Admin password loaded: Pass=$JDBX_BOOTSTRAP_ADMIN_PASS (length=${#JDBX_BOOTSTRAP_ADMIN_PASS})"
        
        # Force kill any existing processes first
        pkill -f jdbxd 2>/dev/null || true
        sleep 1

        # Cleanup
        rm -f "$PID_FILE"
        rm -f "$LOG_FILE"
        
        # Export all JDBX variables for the daemon
        export JDBX_BOOTSTRAP_ADMIN_USER
        export JDBX_BOOTSTRAP_ADMIN_PASS  
        export JDBX_DEFAULT_ADMIN_EMAIL
        export JDBX_SSL_IGNORE_UNEXPECTED_EOF
        
        # Export memory allocator configuration variables
        export JDBX_ENABLE_EXOTIC_ALLOCATORS
        export JDBX_ENABLE_ARENA_ALLOCATOR
        export JDBX_ENABLE_TLSF_ALLOCATOR
        export JDBX_FORCE_SYSTEM_MALLOC
        export JDBX_MEM_DEBUG
        
        # Start server with secure configuration from environment
        echo "🚀 Starting JDBX server with secure configuration..."
        "$DAEMON" --daemon
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
