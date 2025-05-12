#!/bin/bash
# Monitor script for JSONdb server
# This script monitors the JSONdb server and collects metrics

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
LOG_DIR="${BUILD_DIR}/logs"
METRICS_DIR="${BUILD_DIR}/metrics"
SERVER_URL=${SERVER_URL:-"http://localhost:8080"}
INTERVAL=${INTERVAL:-5}
RUNNING=1

# Print section header
print_section() {
    echo -e "\n${BLUE}===================================="
    echo -e " $1"
    echo -e "====================================${NC}"
}

# Print success message
print_success() {
    echo -e "${GREEN}SUCCESS: $1${NC}"
}

# Print error message
print_error() {
    echo -e "${RED}ERROR: $1${NC}"
}

# Print info message
print_info() {
    echo -e "${YELLOW}INFO: $1${NC}"
}

# Ensure directory exists
ensure_dir() {
    if [ ! -d "$1" ]; then
        mkdir -p "$1"
        print_info "Created directory: $1"
    fi
}

# Initialize directories
initialize() {
    print_section "Initializing monitor"
    
    ensure_dir "$LOG_DIR"
    ensure_dir "$METRICS_DIR"
    
    # Create metrics files
    echo "timestamp,requests,connections,cpu_percent,memory_kb" > "${METRICS_DIR}/server_metrics.csv"
    
    print_success "Monitor initialized"
}

# Check if server is running
check_server() {
    # Try to connect to the server
    if ! curl -s -o /dev/null -w "%{http_code}" "$SERVER_URL" &> /dev/null; then
        print_error "Cannot connect to server at $SERVER_URL"
        return 1
    fi
    
    return 0
}

# Collect basic system metrics
collect_system_metrics() {
    local timestamp=$(date +%s)
    local server_pid=$(ps aux | grep "jsondb_server" | grep -v grep | awk '{print $2}')
    
    if [ -z "$server_pid" ]; then
        print_error "Server process not found"
        return 1
    fi
    
    # Get CPU usage
    local cpu_percent=$(ps -p $server_pid -o %cpu | tail -n 1 | tr -d ' ')
    
    # Get memory usage
    local memory_kb=$(ps -p $server_pid -o rss | tail -n 1 | tr -d ' ')
    
    # Get connection count (using netstat)
    local connections=$(netstat -an | grep ":8080" | grep "ESTABLISHED" | wc -l)
    
    # Get request count (from server metrics if available)
    local requests=0
    if curl -s "$SERVER_URL/metrics" | grep -q "requests"; then
        requests=$(curl -s "$SERVER_URL/metrics" | grep "requests" | awk '{print $2}')
    fi
    
    # Save metrics
    echo "$timestamp,$requests,$connections,$cpu_percent,$memory_kb" >> "${METRICS_DIR}/server_metrics.csv"
    
    # Print current metrics
    print_info "Metrics: Requests: $requests, Connections: $connections, CPU: ${cpu_percent}%, Memory: ${memory_kb}KB"
    
    return 0
}

# Collect server health metrics
collect_server_health() {
    # Check if server has a health endpoint
    local health_status=$(curl -s -o /dev/null -w "%{http_code}" "$SERVER_URL/health" 2>/dev/null)
    
    if [ "$health_status" = "200" ]; then
        local health_json=$(curl -s "$SERVER_URL/health")
        print_info "Server health: $health_json"
    else
        print_info "Server health endpoint not available"
    fi
    
    return 0
}

# Generate a metrics report
generate_report() {
    print_section "Generating Metrics Report"
    
    local report_file="${METRICS_DIR}/metrics_report.txt"
    
    # Create report header
    echo "JSONdb Server Metrics Report" > "$report_file"
    echo "Generated at: $(date)" >> "$report_file"
    echo "Server URL: $SERVER_URL" >> "$report_file"
    echo "" >> "$report_file"
    
    # Add summary statistics
    echo "Summary Statistics:" >> "$report_file"
    echo "-------------------" >> "$report_file"
    
    # Calculate average CPU and memory
    local avg_cpu=$(awk -F',' 'NR>1 {sum+=$4; count++} END {if(count>0) print sum/count; else print "N/A"}' "${METRICS_DIR}/server_metrics.csv")
    local avg_memory=$(awk -F',' 'NR>1 {sum+=$5; count++} END {if(count>0) print sum/count; else print "N/A"}' "${METRICS_DIR}/server_metrics.csv")
    local max_cpu=$(awk -F',' 'NR>1 {if($4>max) max=$4} END {print max}' "${METRICS_DIR}/server_metrics.csv")
    local max_memory=$(awk -F',' 'NR>1 {if($5>max) max=$5} END {print max}' "${METRICS_DIR}/server_metrics.csv")
    local max_connections=$(awk -F',' 'NR>1 {if($3>max) max=$3} END {print max}' "${METRICS_DIR}/server_metrics.csv")
    
    echo "Average CPU Usage: ${avg_cpu}%" >> "$report_file"
    echo "Average Memory Usage: ${avg_memory}KB" >> "$report_file"
    echo "Maximum CPU Usage: ${max_cpu}%" >> "$report_file"
    echo "Maximum Memory Usage: ${max_memory}KB" >> "$report_file"
    echo "Maximum Concurrent Connections: $max_connections" >> "$report_file"
    
    # Add monitoring duration
    local start_time=$(head -n 2 "${METRICS_DIR}/server_metrics.csv" | tail -n 1 | cut -d',' -f1)
    local end_time=$(tail -n 1 "${METRICS_DIR}/server_metrics.csv" | cut -d',' -f1)
    local duration=$((end_time - start_time))
    
    echo "Monitoring Duration: ${duration} seconds" >> "$report_file"
    
    print_success "Metrics report generated: $report_file"
    
    return 0
}

# Handle interruption
handle_interrupt() {
    print_section "Stopping monitor"
    RUNNING=0
    generate_report
    print_success "Monitor stopped successfully"
    exit 0
}

# Main monitoring loop
monitor_loop() {
    print_section "Starting monitor"
    print_info "Monitoring server at $SERVER_URL (interval: ${INTERVAL}s)"
    print_info "Press Ctrl+C to stop"
    
    # Set up trap to handle interruption
    trap handle_interrupt SIGINT SIGTERM
    
    # Main loop
    while [ $RUNNING -eq 1 ]; do
        timestamp=$(date "+%Y-%m-%d %H:%M:%S")
        echo -e "\n${BLUE}[$timestamp] Collecting metrics...${NC}"
        
        if ! check_server; then
            print_error "Server not running or not responding"
            sleep $INTERVAL
            continue
        fi
        
        collect_system_metrics
        collect_server_health
        
        sleep $INTERVAL
    done
}

# Main function
main() {
    print_section "JSONdb Server Monitor"
    
    # Initialize
    initialize
    
    # Start monitoring loop
    monitor_loop
    
    # Generate final report
    generate_report
}

# Execute the main function
main