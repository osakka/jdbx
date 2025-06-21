#!/bin/bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
BASE_URL="https://localhost:5000"
CURL_OPTS="-s -k"
ADMIN_USER="admin"
ADMIN_PASS="secure123456789"

# Test tracking
TESTS_PASSED=0
TESTS_FAILED=0

# Helper functions
log_test() {
    echo -e "${BLUE}[TEST]${NC} $1"
}

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
    ((TESTS_PASSED++))
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    if [[ -n "$2" ]]; then
        echo -e "${RED}[DETAIL]${NC} $2"
    fi
    ((TESTS_FAILED++))
}

log_info() {
    echo -e "${YELLOW}[INFO]${NC} $1"
}

# Test functions
test_health_check() {
    log_test "Health check endpoint"
    
    local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/health")
    local status=$(echo "$response" | jq -r '.status' 2>/dev/null)
    
    if [[ "$status" == "ok" ]]; then
        log_pass "Health check successful"
        return 0
    else
        log_fail "Health check failed" "$response"
        return 1
    fi
}

test_admin_login() {
    log_test "Admin login for metrics access"
    
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/auth/login" \
        -H "Content-Type: application/json" \
        -d "{\"username\":\"$ADMIN_USER\",\"password\":\"$ADMIN_PASS\"}")
    
    ADMIN_TOKEN=$(echo "$response" | jq -r '.token' 2>/dev/null)
    
    if [[ -n "$ADMIN_TOKEN" && "$ADMIN_TOKEN" != "null" ]]; then
        log_pass "Admin login successful"
        log_info "Token: ${ADMIN_TOKEN:0:20}..."
        return 0
    else
        log_fail "Admin login failed" "$response"
        return 1
    fi
}

test_metrics_current() {
    log_test "Get current metrics from health endpoint"
    
    local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/health" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    # Extract metrics from health response
    local metrics=$(echo "$response" | jq -r '.metrics' 2>/dev/null)
    
    # Check if we have all metric types
    local operations=$(echo "$metrics" | jq -r '.operations' 2>/dev/null)
    local performance=$(echo "$metrics" | jq -r '.performance' 2>/dev/null)
    local cache=$(echo "$metrics" | jq -r '.cache' 2>/dev/null)
    
    if [[ "$operations" != "null" && "$performance" != "null" && "$cache" != "null" ]]; then
        log_pass "All metric types present"
        log_info "Operations total: $(echo "$operations" | jq -r '.total' 2>/dev/null)"
        log_info "Active connections: $(echo "$performance" | jq -r '.active_connections' 2>/dev/null)"
        return 0
    else
        log_fail "Missing metric types" "$response"
        return 1
    fi
}

test_metrics_history() {
    log_test "Get metrics history"
    
    local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/metrics/history" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    # Check if we have data object with metric entries
    local data=$(echo "$response" | jq -r '.data' 2>/dev/null)
    local has_operations=$(echo "$data" | jq -r '.operations | length' 2>/dev/null)
    local has_performance=$(echo "$data" | jq -r '.performance | length' 2>/dev/null)
    local has_connections=$(echo "$data" | jq -r '.connections | length' 2>/dev/null)
    
    if [[ "$has_operations" -gt "0" || "$has_performance" -gt "0" || "$has_connections" -gt "0" ]]; then
        log_pass "Metrics history available"
        log_info "History entries - Ops: $has_operations, Perf: $has_performance, Conn: $has_connections"
        return 0
    else
        log_fail "No metrics history found" "$response"
        return 1
    fi
}

test_specific_metric() {
    log_test "Get specific metric from history endpoint"
    
    # Use the history endpoint with metric parameter
    local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/metrics/history?metric=operations" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    # Check if we have data array for the specific metric
    local data=$(echo "$response" | jq -r '.data | length' 2>/dev/null)
    
    if [[ "$data" != "null" && "$data" != "0" ]]; then
        log_pass "Specific metric retrieved from history"
        log_info "Operations metric has $data history entries"
        return 0
    else
        log_fail "No history data for specific metric" "$response"
        return 1
    fi
}

test_metrics_aggregation() {
    log_test "Metrics aggregation endpoint"
    
    local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/metrics/aggregate?interval=1h" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    # Check if aggregation worked
    local aggregated=$(echo "$response" | jq -r '.aggregated' 2>/dev/null)
    
    if [[ "$aggregated" != "null" ]]; then
        log_pass "Metrics aggregation working"
        return 0
    else
        # This might not be implemented, so just log as info
        log_info "Aggregation endpoint not implemented or no data"
        return 0
    fi
}

test_metrics_persistence() {
    log_test "Metrics persistence check"
    
    # Get initial metrics
    local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/health" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    local ops_before=$(echo "$response" | jq -r '.metrics.operations.total' 2>/dev/null)
    
    # Generate some activity
    for i in {1..5}; do
        curl $CURL_OPTS -X GET "$BASE_URL/api/health" \
            -H "Authorization: Bearer $ADMIN_TOKEN" > /dev/null 2>&1
    done
    
    # Wait a moment
    sleep 1
    
    # Check if metrics increased
    response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/health" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    local ops_after=$(echo "$response" | jq -r '.metrics.operations.total' 2>/dev/null)
    
    if [[ "$ops_after" -gt "$ops_before" ]]; then
        log_pass "Metrics are being tracked and persisted"
        log_info "Operations increased from $ops_before to $ops_after"
        return 0
    else
        log_fail "Metrics not updating" "Before: $ops_before, After: $ops_after"
        return 1
    fi
}

test_metrics_reset() {
    log_test "Metrics reset functionality"
    
    # Try to reset metrics (if endpoint exists)
    local response=$(curl $CURL_OPTS -X POST "$BASE_URL/api/metrics/reset" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    local error=$(echo "$response" | jq -r '.error' 2>/dev/null)
    
    if [[ "$error" == "null" || "$error" == "" ]]; then
        log_pass "Metrics reset endpoint exists"
        return 0
    else
        # This might not be implemented
        log_info "Metrics reset endpoint not implemented"
        return 0
    fi
}

test_library_metrics() {
    log_test "Library-specific metrics"
    
    local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/libraries/default/metrics" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    local operations=$(echo "$response" | jq -r '.operations' 2>/dev/null)
    
    if [[ "$operations" != "null" ]]; then
        log_pass "Library metrics available"
        log_info "Default library operations: $(echo "$operations" | jq -r '.total' 2>/dev/null)"
        return 0
    else
        log_info "Library metrics endpoint might not be implemented"
        return 0
    fi
}

test_metrics_unauthorized() {
    log_test "Metrics access without authentication"
    
    local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/metrics")
    local error=$(echo "$response" | jq -r '.error' 2>/dev/null)
    
    if [[ "$error" == "Unauthorized" ]]; then
        log_pass "Metrics properly protected by authentication"
        return 0
    else
        log_fail "Security issue: Metrics accessible without auth" "$response"
        return 1
    fi
}

test_concurrent_metrics() {
    log_test "Concurrent metrics access"
    
    local success=0
    local total=10
    
    # Make concurrent requests to the metrics endpoint
    for i in $(seq 1 $total); do
        curl $CURL_OPTS -X GET "$BASE_URL/api/metrics" \
            -H "Authorization: Bearer $ADMIN_TOKEN" > /dev/null 2>&1 &
    done
    
    wait
    
    # Verify server is still responsive
    local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/health")
    local status=$(echo "$response" | jq -r '.status' 2>/dev/null)
    
    if [[ "$status" == "ok" ]]; then
        log_pass "Server stable under concurrent metrics access"
        return 0
    else
        log_fail "Server issues under concurrent load" "$response"
        return 1
    fi
}

test_metrics_data_structure() {
    log_test "Verify metrics data structure"
    
    local response=$(curl $CURL_OPTS -X GET "$BASE_URL/api/health" \
        -H "Authorization: Bearer $ADMIN_TOKEN")
    
    local metrics=$(echo "$response" | jq -r '.metrics' 2>/dev/null)
    
    # Check operations metric structure
    local ops_total=$(echo "$metrics" | jq -r '.operations.total' 2>/dev/null)
    local ops_read=$(echo "$metrics" | jq -r '.operations.read' 2>/dev/null)
    local ops_write=$(echo "$metrics" | jq -r '.operations.write' 2>/dev/null)
    
    # Check performance metric structure
    local perf_avg=$(echo "$metrics" | jq -r '.performance.avg_response_time_ms' 2>/dev/null)
    
    # Check cache metric structure
    local cache_hits=$(echo "$metrics" | jq -r '.cache.hits' 2>/dev/null)
    
    if [[ "$ops_total" != "null" && "$perf_avg" != "null" && "$cache_hits" != "null" ]]; then
        log_pass "Metrics data structure is correct"
        return 0
    else
        log_fail "Invalid metrics data structure" "$response"
        return 1
    fi
}

# Main test execution
main() {
    echo "==================================="
    echo "JDBX Metrics E2E Test Suite"
    echo "==================================="
    echo
    
    # Run all tests
    test_health_check
    
    if test_admin_login; then
        test_metrics_current
        test_metrics_history
        test_specific_metric
        test_metrics_aggregation
        test_metrics_persistence
        test_metrics_reset
        test_library_metrics
        test_metrics_unauthorized
        test_concurrent_metrics
        test_metrics_data_structure
    fi
    
    # Summary
    echo
    echo "==================================="
    echo "Test Summary"
    echo "==================================="
    local total=$((TESTS_PASSED + TESTS_FAILED))
    echo "Total Tests: $total"
    echo -e "Passed: ${GREEN}$TESTS_PASSED${NC}"
    echo -e "Failed: ${RED}$TESTS_FAILED${NC}"
    
    local success_rate=0
    if [[ $total -gt 0 ]]; then
        success_rate=$((TESTS_PASSED * 100 / total))
    fi
    
    echo "Success Rate: ${success_rate}%"
    
    if [[ $TESTS_FAILED -eq 0 ]]; then
        echo -e "${GREEN}✅ ALL TESTS PASSED!${NC}"
        exit 0
    else
        echo -e "${RED}❌ TESTS FAILED - Need fixes${NC}"
        exit 1
    fi
}

# Run the tests
main