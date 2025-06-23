#!/bin/bash
# End-to-end performance benchmark for the JSON Database Server with JavaScript integration

# Set working directory to project root
cd "$(dirname "$0")/.."

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}=======================================${NC}"
echo -e "${BLUE}= JSON Database Performance Benchmark =${NC}"
echo -e "${BLUE}=======================================${NC}"

# Check if server is already running
if [ -f var/run/jsondb_server.pid ]; then
    echo -e "${YELLOW}Warning: JSON Database server appears to be running already.${NC}"
    echo "PID file exists at var/run/jsondb_server.pid"
    echo "This test should be run with a fresh server instance."
    echo -n "Do you want to stop the existing server? [y/N] "
    read response
    if [[ "$response" =~ ^[Yy]$ ]]; then
        echo "Stopping existing server..."
        bin/jsondb_server -stop
        sleep 2
    else
        echo "Exiting test."
        exit 1
    fi
fi

# Make sure the working directories exist
mkdir -p var/run
mkdir -p var/log/jsondb
mkdir -p var/data/jsondb

# Build the server if necessary
if [ ! -f bin/jsondb_server ]; then
    echo -e "${YELLOW}Server executable not found. Building...${NC}"
    make
    if [ $? -ne 0 ]; then
        echo -e "${RED}Failed to build server. Exiting.${NC}"
        exit 1
    fi
fi

# Set up clean test database
echo -e "${BLUE}Setting up clean test environment...${NC}"
if [ -f var/data/jsondb/test_performance.json ]; then
    echo "Removing existing test database..."
    rm var/data/jsondb/test_performance.json
fi

# Function to check if server started properly
wait_for_server() {
    # Wait up to 10 seconds for server to start
    for i in {1..10}; do
        if [ -f var/run/jsondb_server.pid ]; then
            pid=$(cat var/run/jsondb_server.pid)
            if ps -p $pid > /dev/null; then
                echo -e "${GREEN}Server started successfully with PID $pid${NC}"
                return 0
            fi
        fi
        echo "Waiting for server to start... ($i/10)"
        sleep 1
    done
    echo -e "${RED}Failed to start server within timeout period.${NC}"
    return 1
}

# Start the server in daemon mode with the test database
echo -e "${BLUE}Starting JSON Database server...${NC}"
bin/jsondb_server -daemon -db var/data/jsondb/test_performance.json -port 5000
if ! wait_for_server; then
    echo -e "${RED}Server failed to start. Check logs for details.${NC}"
    exit 1
fi

# Ensure cleanup on exit
function cleanup {
    echo -e "${BLUE}Stopping JSON Database server...${NC}"
    bin/jsondb_server -stop
    # Wait for server to stop
    sleep 2
    if [ -f var/run/jsondb_server.pid ]; then
        echo -e "${YELLOW}Warning: Server PID file still exists. Force killing...${NC}"
        pid=$(cat var/run/jsondb_server.pid)
        if ps -p $pid > /dev/null; then
            kill -9 $pid
        fi
        rm var/run/jsondb_server.pid
    fi
    echo -e "${GREEN}Cleanup complete.${NC}"
}
trap cleanup EXIT

# Warm up the server
echo -e "${BLUE}Warming up the server...${NC}"
bin/jsondb_server -js_eval "({ status: 'ok' })" > /dev/null

# Run the performance benchmark
echo -e "${BLUE}Running JavaScript performance benchmark...${NC}"
echo -e "${YELLOW}This may take several minutes to complete...${NC}"
bin/jsondb_server -js_eval_file tests/js_performance_benchmark.js

# Collect server stats
echo -e "${BLUE}Collecting server statistics...${NC}"
bin/jsondb_server -js_eval "db.getServerStats ? db.getServerStats() : { message: 'Server stats not available' }"

# Print results location
echo -e "${GREEN}Performance benchmark completed.${NC}"
echo "Full logs available at: var/log/jsondb/server.log"