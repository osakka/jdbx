#!/bin/bash
# Complete Performance Test Suite for JSON Database with JavaScript Integration
# This script runs all the performance tests and generates a comprehensive report

# Set working directory to project root
cd "$(dirname "$0")"

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}===============================================${NC}"
echo -e "${BLUE}= JSON Database Performance Benchmark Suite =${NC}"
echo -e "${BLUE}===============================================${NC}"

# Check if Python is available for report generation
if ! command -v python3 &> /dev/null; then
    echo -e "${YELLOW}Warning: Python 3 not found. HTML report generation will be skipped.${NC}"
    SKIP_HTML=1
else
    SKIP_HTML=0
fi

# Create output directory
OUTPUT_DIR="performance_results"
mkdir -p "$OUTPUT_DIR"

# Generate timestamp for this run
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_BASE="${OUTPUT_DIR}/performance_${TIMESTAMP}"

echo -e "${BLUE}Performance test results will be saved to: ${OUTPUT_BASE}_*${NC}"

# Step 1: Run the performance tests
echo -e "\n${BLUE}Step 1: Running basic performance tests...${NC}"
chmod +x tests/run_performance_tests.sh
tests/run_performance_tests.sh > "${OUTPUT_BASE}_basic.log" 2>&1
if [ $? -ne 0 ]; then
    echo -e "${RED}Basic performance tests failed. Check log for details.${NC}"
    echo "Log file: ${OUTPUT_BASE}_basic.log"
    exit 1
fi
echo -e "${GREEN}Basic performance tests completed.${NC}"

# Step 2: Run the detailed performance report
echo -e "\n${BLUE}Step 2: Generating detailed performance report...${NC}"
echo -e "${YELLOW}This may take several minutes to complete...${NC}"
bin/jsondb_server -js_eval_file tests/generate_performance_report.js > "${OUTPUT_BASE}_report.json"
if [ $? -ne 0 ]; then
    echo -e "${RED}Performance report generation failed.${NC}"
    exit 1
fi
echo -e "${GREEN}Performance report generated: ${OUTPUT_BASE}_report.json${NC}"

# Step 3: Generate HTML report
if [ $SKIP_HTML -eq 0 ]; then
    echo -e "\n${BLUE}Step 3: Generating HTML performance report...${NC}"
    python3 tests/generate_html_report.py "${OUTPUT_BASE}_report.json" "${OUTPUT_BASE}_report.html"
    if [ $? -ne 0 ]; then
        echo -e "${RED}HTML report generation failed.${NC}"
    else
        echo -e "${GREEN}HTML report generated: ${OUTPUT_BASE}_report.html${NC}"
        
        # Try to open the HTML report
        if command -v xdg-open &> /dev/null; then
            echo -e "${BLUE}Opening HTML report...${NC}"
            xdg-open "${OUTPUT_BASE}_report.html" &
        elif command -v open &> /dev/null; then
            echo -e "${BLUE}Opening HTML report...${NC}"
            open "${OUTPUT_BASE}_report.html" &
        else
            echo -e "${YELLOW}HTML report can be viewed at: ${OUTPUT_BASE}_report.html${NC}"
        fi
    fi
else
    echo -e "\n${YELLOW}Step 3: Skipping HTML report generation (Python 3 not available)${NC}"
fi

# Step 4: Summarize results
echo -e "\n${BLUE}Step 4: Summarizing results...${NC}"
echo "Basic performance test log: ${OUTPUT_BASE}_basic.log"
echo "Detailed performance report (JSON): ${OUTPUT_BASE}_report.json"
if [ $SKIP_HTML -eq 0 ]; then
    echo "HTML performance report: ${OUTPUT_BASE}_report.html"
fi

echo -e "\n${GREEN}Performance benchmark suite completed successfully!${NC}"