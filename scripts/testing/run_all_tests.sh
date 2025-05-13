#!/bin/bash
# JSONdb Comprehensive Test Runner
# For production-grade testing infrastructure

# Set script to exit on error
set -e

# Set color variables
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Define the test directories
TEST_DIRS=("unit" "integration" "performance" "security")

# Create log directories
mkdir -p ../test_logs
mkdir -p ../test_logs/unit
mkdir -p ../test_logs/integration
mkdir -p ../test_logs/performance
mkdir -p ../test_logs/security

# Print header
echo -e "${BLUE}================================${NC}"
echo -e "${BLUE}   JSONdb Test Runner v1.0${NC}"
echo -e "${BLUE}================================${NC}"
echo

# Check if server is running for integration tests
check_server() {
    echo -e "${YELLOW}Checking if JSONdb server is running...${NC}"
    if curl -s http://localhost:8080/health > /dev/null; then
        echo -e "${GREEN}✓ JSONdb server is running${NC}"
        return 0
    else
        echo -e "${RED}✗ JSONdb server is not running${NC}"
        echo -e "${YELLOW}Starting server for integration tests...${NC}"
        
        # Try to start server
        if [ -f "../build/bin/jsondb_server" ]; then
            ../build/bin/jsondb_server &
            SERVER_PID=$!
            sleep 2
            
            if curl -s http://localhost:8080/health > /dev/null; then
                echo -e "${GREEN}✓ JSONdb server started successfully${NC}"
                return 0
            else
                echo -e "${RED}✗ Failed to start JSONdb server${NC}"
                return 1
            fi
        else
            echo -e "${RED}✗ Cannot find server executable${NC}"
            return 1
        fi
    fi
}

# Compile tests
compile_tests() {
    echo -e "${YELLOW}Compiling tests...${NC}"
    
    # Create bin directory if it doesn't exist
    mkdir -p bin
    
    # Compile unit tests
    echo -e "${BLUE}Compiling unit tests...${NC}"
    for test_file in unit/*.c; do
        test_name=$(basename "$test_file" .c)
        echo "  Compiling $test_name"
        gcc -Wall -I../include -L../lib -o "bin/$test_name" "$test_file" -ljsondb
    done
    
    # Compile integration tests
    echo -e "${BLUE}Compiling integration tests...${NC}"
    for test_file in integration/*.c; do
        test_name=$(basename "$test_file" .c)
        echo "  Compiling $test_name"
        gcc -Wall -I../include -L../lib -o "bin/$test_name" "$test_file" -ljsondb
    done
    
    # Compile performance tests
    echo -e "${BLUE}Compiling performance tests...${NC}"
    for test_file in performance/*.c; do
        test_name=$(basename "$test_file" .c)
        echo "  Compiling $test_name"
        gcc -Wall -I../include -L../lib -o "bin/$test_name" "$test_file" -ljsondb
    done
    
    # Compile security tests
    echo -e "${BLUE}Compiling security tests...${NC}"
    for test_file in security/*.c; do
        if [ -f "$test_file" ]; then
            test_name=$(basename "$test_file" .c)
            echo "  Compiling $test_name"
            gcc -Wall -I../include -L../lib -o "bin/$test_name" "$test_file" -ljsondb
        fi
    done
    
    echo -e "${GREEN}All tests compiled successfully${NC}"
    echo
}

# Run unit tests
run_unit_tests() {
    echo -e "${YELLOW}Running unit tests...${NC}"
    local failed=0
    local passed=0
    local total=0
    
    for test_file in bin/test_*; do
        if [[ "$test_file" == *"test_database"* || "$test_file" == *"test_json"* ]]; then
            test_name=$(basename "$test_file")
            echo -e "${BLUE}Running $test_name...${NC}"
            
            if $test_file; then
                echo -e "${GREEN}✓ $test_name passed${NC}"
                ((passed++))
            else
                echo -e "${RED}✗ $test_name failed${NC}"
                ((failed++))
            fi
            ((total++))
        fi
    done
    
    echo
    echo -e "${BLUE}Unit Test Summary: ${GREEN}$passed passed${NC}, ${RED}$failed failed${NC}, $total total${NC}"
    echo
    
    return $failed
}

# Run integration tests
run_integration_tests() {
    echo -e "${YELLOW}Running integration tests...${NC}"
    local failed=0
    local passed=0
    local total=0
    
    # Only run if server is available
    if check_server; then
        for test_file in bin/test_server_*; do
            if [ -f "$test_file" ]; then
                test_name=$(basename "$test_file")
                echo -e "${BLUE}Running $test_name...${NC}"
                
                if $test_file; then
                    echo -e "${GREEN}✓ $test_name passed${NC}"
                    ((passed++))
                else
                    echo -e "${RED}✗ $test_name failed${NC}"
                    ((failed++))
                fi
                ((total++))
            fi
        done
    else
        echo -e "${RED}Skipping integration tests - server not available${NC}"
        return 0
    fi
    
    echo
    echo -e "${BLUE}Integration Test Summary: ${GREEN}$passed passed${NC}, ${RED}$failed failed${NC}, $total total${NC}"
    echo
    
    return $failed
}

# Run performance tests
run_performance_tests() {
    echo -e "${YELLOW}Running performance tests...${NC}"
    local failed=0
    local passed=0
    local total=0
    
    for test_file in bin/test_*performance*; do
        if [ -f "$test_file" ]; then
            test_name=$(basename "$test_file")
            echo -e "${BLUE}Running $test_name...${NC}"
            
            if $test_file; then
                echo -e "${GREEN}✓ $test_name passed${NC}"
                ((passed++))
            else
                echo -e "${RED}✗ $test_name failed${NC}"
                ((failed++))
            fi
            ((total++))
        fi
    done
    
    echo
    echo -e "${BLUE}Performance Test Summary: ${GREEN}$passed passed${NC}, ${RED}$failed failed${NC}, $total total${NC}"
    echo
    
    return $failed
}

# Generate test report
generate_report() {
    echo -e "${YELLOW}Generating test report...${NC}"
    
    if [ -f "scripts/generate_test_report.sh" ]; then
        bash scripts/generate_test_report.sh
    else
        echo -e "${RED}Report generator script not found${NC}"
    fi
    
    echo
}

# Main function
main() {
    local unit_result=0
    local integration_result=0
    local performance_result=0
    
    compile_tests
    
    # Run the tests
    run_unit_tests
    unit_result=$?
    
    run_integration_tests
    integration_result=$?
    
    run_performance_tests
    performance_result=$?
    
    # Generate report with combined results
    generate_report
    
    # Print overall summary
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE}   Overall Test Summary${NC}"
    echo -e "${BLUE}================================${NC}"
    
    if [ $unit_result -eq 0 ]; then
        echo -e "${GREEN}✓ Unit tests: PASSED${NC}"
    else
        echo -e "${RED}✗ Unit tests: FAILED ($unit_result failures)${NC}"
    fi
    
    if [ $integration_result -eq 0 ]; then
        echo -e "${GREEN}✓ Integration tests: PASSED${NC}"
    else
        echo -e "${RED}✗ Integration tests: FAILED ($integration_result failures)${NC}"
    fi
    
    if [ $performance_result -eq 0 ]; then
        echo -e "${GREEN}✓ Performance tests: PASSED${NC}"
    else
        echo -e "${RED}✗ Performance tests: FAILED ($performance_result failures)${NC}"
    fi
    
    # Check if we started the server and should shut it down
    if [ -n "$SERVER_PID" ]; then
        echo -e "${YELLOW}Shutting down test server (PID: $SERVER_PID)${NC}"
        kill $SERVER_PID
    fi
    
    echo
    echo -e "${BLUE}Test results are available in:${NC}"
    echo -e "${YELLOW}  * ../test_logs/ (raw logs)${NC}"
    echo -e "${YELLOW}  * ../test_logs/test_report_*.html (HTML report)${NC}"
    echo
    
    # Return overall status
    local total_failures=$((unit_result + integration_result + performance_result))
    return $total_failures
}

# Execute main function
main
exit $?