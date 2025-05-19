#!/bin/bash

# Start server in debug mode
# This script runs the server in foreground mode with detailed debug output

JSONDB_DIR="/opt/jsondb"
SERVER_BIN="${JSONDB_DIR}/build/bin/jsondb_server"
DEBUG_LOG="${JSONDB_DIR}/build/var/debug_server.log"

echo "Starting JSONdb server in debug mode..."
echo "Debug log: ${DEBUG_LOG}"

# Make sure log directory exists
mkdir -p "$(dirname "${DEBUG_LOG}")"

# Kill any existing server process
pkill -f jsondb_server 2>/dev/null

# Clear previous debug log
rm -f "${DEBUG_LOG}"
echo "Starting debug server at $(date)" > "${DEBUG_LOG}"

# Run server in foreground mode with verbose output
cd "${JSONDB_DIR}" || exit 1
${SERVER_BIN} -V -p 5000 2>&1 | tee -a "${DEBUG_LOG}"