#!/bin/bash
#
# Debug utility for JavaScript execution in jsondb_server
#

# Project root directory
PROJECT_ROOT=$(dirname $(dirname $(readlink -f "$0")))
SERVER_BIN="$PROJECT_ROOT/bin/jsondb_server"
JS_FILE=""
DEBUG_MODE="none"
OUTPUT_LOG="$PROJECT_ROOT/js_debug.log"

# Colors
RESET="\033[0m"
RED="\033[0;31m"
GREEN="\033[0;32m"
YELLOW="\033[0;33m"
BLUE="\033[0;34m"

# Display help information
function show_help() {
    echo -e "${BLUE}JavaScript Debug Utility${RESET}"
    echo "------------------------"
    echo ""
    echo "This script helps debug JavaScript file execution."
    echo ""
    echo "Usage:"
    echo "  $0 [options] <js_file>"
    echo ""
    echo "Options:"
    echo "  -g, --gdb       Run with GDB debugger"
    echo "  -v, --valgrind  Run with Valgrind memory checker"
    echo "  -t, --trace     Enable trace-level logging"
    echo "  -o, --output    Specify output log file (default: js_debug.log)"
    echo "  -h, --help      Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 my_script.js               # Run with debug logging"
    echo "  $0 -g functions/test.js       # Debug with GDB"
    echo "  $0 -v transforms/user.js      # Check memory issues with Valgrind"
    echo ""
}

# Parse command-line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -g|--gdb)
            DEBUG_MODE="gdb"
            shift
            ;;
        -v|--valgrind)
            DEBUG_MODE="valgrind"
            shift
            ;;
        -t|--trace)
            LOG_LEVEL="trace"
            shift
            ;;
        -o|--output)
            OUTPUT_LOG="$2"
            shift 2
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            if [[ -z "$JS_FILE" ]]; then
                JS_FILE="$1"
            else
                echo -e "${RED}Error: Multiple JavaScript files specified${RESET}"
                show_help
                exit 1
            fi
            shift
            ;;
    esac
done

# Check if a JavaScript file was specified
if [[ -z "$JS_FILE" ]]; then
    echo -e "${RED}Error: No JavaScript file specified${RESET}"
    show_help
    exit 1
fi

# Create a temporary directory for test files
TMP_DIR=$(mktemp -d)
DB_FILE="$TMP_DIR/db.json"
RBAC_FILE="$TMP_DIR/rbac.json"
LOG_FILE="$TMP_DIR/server.log"

# Create minimal DB and RBAC files
echo '{}' > "$DB_FILE"
echo '{"roles":{}, "users":{}}' > "$RBAC_FILE"

# Prepare server command
if [[ "$LOG_LEVEL" == "trace" ]]; then
    SERVER_CMD="$SERVER_BIN -js $JS_FILE -foreground -log-level trace -log $LOG_FILE"
else
    SERVER_CMD="$SERVER_BIN -js $JS_FILE -foreground -log-level debug -log $LOG_FILE"
fi

# Run the server with the appropriate debugging tool
echo -e "${BLUE}Debugging JavaScript File: $JS_FILE${RESET}"
echo "Command: $SERVER_CMD"
echo "Log file: $LOG_FILE"
echo "Temporary directory: $TMP_DIR"
echo ""

case $DEBUG_MODE in
    gdb)
        echo -e "${YELLOW}Running with GDB debugger...${RESET}"
        echo "Type 'run' to start the server"
        echo "Type 'bt' after a crash to see the backtrace"
        echo ""
        gdb --args $SERVER_CMD
        ;;
    valgrind)
        echo -e "${YELLOW}Running with Valgrind memory checker...${RESET}"
        valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file="$OUTPUT_LOG" $SERVER_CMD
        echo "Valgrind output saved to: $OUTPUT_LOG"
        ;;
    *)
        echo -e "${YELLOW}Running with standard debug logging...${RESET}"
        $SERVER_CMD 2>&1 | tee "$OUTPUT_LOG"
        RET=${PIPESTATUS[0]}
        if [ $RET -ne 0 ]; then
            echo -e "${RED}JavaScript execution failed with exit code: $RET${RESET}"
        fi
        ;;
esac

# Copy the log file to the output location
if [[ -f "$LOG_FILE" ]]; then
    cp "$LOG_FILE" "$OUTPUT_LOG"
    echo -e "${GREEN}Server log saved to: $OUTPUT_LOG${RESET}"
fi

# Clean up temporary files
rm -rf "$TMP_DIR"

exit 0