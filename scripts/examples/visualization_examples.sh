#!/bin/bash

# Examples for using the JSON Database Server's transaction visualization API
# This script provides examples for testing the various visualization formats
# and export capabilities of the transaction system
#
# NOTE: This script is currently non-functional due to build issues.
# It serves as a reference implementation for testing when the build issues are resolved.

# Configuration
SERVER="http://localhost:5000"  # Using the default server port
OUTPUT_DIR="./visualization_output"

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Create output directory
mkdir -p "$OUTPUT_DIR"

echo -e "${YELLOW}=== JSON Database Server Visualization Examples ===${NC}"

# Function to execute API call and save output
call_and_save() {
    local endpoint=$1
    local params=$2
    local output_file=$3
    local description=$4
    
    echo -e "\n${BLUE}$description${NC}"
    echo -e "Calling: $SERVER$endpoint$params"
    echo -e "Saving to: $output_file"
    
    curl -s "$SERVER$endpoint$params" > "$output_file"
    
    # Show a sample of the output (first 10 lines)
    echo -e "${GREEN}Output sample:${NC}"
    head "$output_file"
    echo "..."
}

# 1. Basic Transaction History
echo -e "\n${YELLOW}=== Basic Transaction Visualizations ===${NC}"

call_and_save "/api/visualization/transaction-history" "" \
    "$OUTPUT_DIR/transaction_history_default.json" \
    "Default transaction history visualization"

call_and_save "/api/visualization/transaction-metrics" "" \
    "$OUTPUT_DIR/transaction_metrics.json" \
    "Transaction metrics visualization"

call_and_save "/api/visualization/transaction-relationships" "" \
    "$OUTPUT_DIR/transaction_relationships.json" \
    "Transaction relationships visualization"

# 2. Timeline-based Visualizations
echo -e "\n${YELLOW}=== Timeline-based Visualizations ===${NC}"

call_and_save "/api/visualization/transaction-history" "?format=timeline" \
    "$OUTPUT_DIR/transaction_timeline.json" \
    "Timeline visualization of transactions"

call_and_save "/api/visualization/transaction-history" "?format=lifecycle" \
    "$OUTPUT_DIR/transaction_lifecycle.json" \
    "Lifecycle visualization of transactions"

# 3. Analytical Visualizations
echo -e "\n${YELLOW}=== Analytical Visualizations ===${NC}"

call_and_save "/api/visualization/transaction-history" "?format=heatmap" \
    "$OUTPUT_DIR/transaction_heatmap.json" \
    "Heatmap visualization of resource contention"

call_and_save "/api/visualization/transaction-history" "?format=distribution" \
    "$OUTPUT_DIR/transaction_distribution.json" \
    "Distribution visualization of transaction performance"

# 4. Graph Visualizations
echo -e "\n${YELLOW}=== Graph Visualizations ===${NC}"

call_and_save "/api/visualization/transaction-history" "?format=dependency" \
    "$OUTPUT_DIR/transaction_dependency.json" \
    "Dependency graph visualization"

call_and_save "/api/visualization/transaction-history" "?format=sankey" \
    "$OUTPUT_DIR/transaction_sankey.json" \
    "Sankey diagram visualization"

# 5. Graph Export Formats
echo -e "\n${YELLOW}=== Graph Export Formats ===${NC}"

call_and_save "/api/visualization/transaction-export" "?format=dot" \
    "$OUTPUT_DIR/transaction_graph.dot" \
    "DOT/GraphViz format export"

call_and_save "/api/visualization/transaction-export" "?format=graphml" \
    "$OUTPUT_DIR/transaction_graph.graphml" \
    "GraphML format export"

call_and_save "/api/visualization/transaction-export" "?format=cytoscape" \
    "$OUTPUT_DIR/transaction_graph_cytoscape.json" \
    "Cytoscape.js JSON format export"

call_and_save "/api/visualization/transaction-export" "?format=d3" \
    "$OUTPUT_DIR/transaction_graph_d3.json" \
    "D3.js JSON format export"

# 6. Filtered Visualizations
echo -e "\n${YELLOW}=== Filtered Visualizations ===${NC}"

# Last hour of transactions in timeline format
call_and_save "/api/visualization/transaction-history" "?format=timeline&hours=1" \
    "$OUTPUT_DIR/transaction_timeline_1hour.json" \
    "Timeline visualization of transactions for the last hour"

# Transactions from a specific user
call_and_save "/api/visualization/transaction-history" "?format=timeline&user_id=admin" \
    "$OUTPUT_DIR/transaction_timeline_admin.json" \
    "Timeline visualization of transactions for user 'admin'"

# Limited number of transactions
call_and_save "/api/visualization/transaction-history" "?format=timeline&limit=5" \
    "$OUTPUT_DIR/transaction_timeline_limit5.json" \
    "Timeline visualization limited to 5 transactions"

echo -e "\n${YELLOW}=== All examples completed ===${NC}"
echo -e "Output files saved to $OUTPUT_DIR"
echo -e "You can now use these files with visualization tools or analyze the JSON structures"