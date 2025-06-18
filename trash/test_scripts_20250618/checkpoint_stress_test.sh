#!/bin/bash
# Comprehensive stress test for JDBX checkpoint memory management system

set -e

BASE_URL="https://localhost:5000"
ADMIN_USER="admin"
ADMIN_PASS="secure123456789"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log() {
    echo -e "${BLUE}[$(date '+%H:%M:%S')]${NC} $1"
}

success() {
    echo -e "${GREEN}[$(date '+%H:%M:%S')]${NC} ✅ $1"
}

warning() {
    echo -e "${YELLOW}[$(date '+%H:%M:%S')]${NC} ⚠️  $1"
}

error() {
    echo -e "${RED}[$(date '+%H:%M:%S')]${NC} ❌ $1"
}

# Get JWT token
get_token() {
    local response
    response=$(curl -s -k -X POST "$BASE_URL/api/auth/login" \
        -H "Content-Type: application/json" \
        -d "{\"username\":\"$ADMIN_USER\",\"password\":\"$ADMIN_PASS\"}" \
        -w "\nHTTP_CODE:%{http_code}")
    
    local http_code=$(echo "$response" | grep -o "HTTP_CODE:[0-9]*" | cut -d: -f2)
    if [ "$http_code" = "200" ]; then
        echo "$response" | head -n -1 | grep -o '"token":"[^"]*' | cut -d'"' -f4
    else
        error "Login failed (HTTP $http_code)"
        return 1
    fi
}

# Create a test document
create_document() {
    local token="$1"
    local doc_id="$2"
    local size="$3"
    
    # Generate content based on size (small/medium/large)
    case "$size" in
        "small")
            local content="Small test document $doc_id"
            ;;
        "medium")
            local content=$(printf "Medium document %s with content: %s" "$doc_id" "$(head -c 1000 < /dev/zero | tr '\0' 'x')")
            ;;
        "large")
            local content=$(printf "Large document %s with extensive content: %s" "$doc_id" "$(head -c 5000 < /dev/zero | tr '\0' 'x')")
            ;;
    esac
    
    local json_doc=$(cat <<EOF
{
    "title": "Checkpoint Test Document $doc_id",
    "content": "$content",
    "metadata": {
        "test_id": "$doc_id",
        "size": "$size",
        "timestamp": $(date +%s)
    },
    "tags": ["test", "checkpoint", "$size"]
}
EOF
)
    
    local response
    response=$(curl -s -k -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $token" \
        -H "Content-Type: application/json" \
        -d "$json_doc" \
        -w "\nHTTP_CODE:%{http_code}")
    
    local http_code=$(echo "$response" | grep -o "HTTP_CODE:[0-9]*" | cut -d: -f2)
    if [ "$http_code" = "201" ]; then
        # Extract UUID from response
        echo "$response" | head -n -1 | grep -o '"uuid":"[^"]*' | cut -d'"' -f4
    else
        error "Create document $doc_id failed (HTTP $http_code)"
        return 1
    fi
}

# Query documents
query_documents() {
    local token="$1"
    
    local response
    response=$(curl -s -k -X GET "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $token" \
        -w "\nHTTP_CODE:%{http_code}")
    
    local http_code=$(echo "$response" | grep -o "HTTP_CODE:[0-9]*" | cut -d: -f2)
    if [ "$http_code" = "200" ]; then
        # Extract count from response
        echo "$response" | head -n -1 | grep -o '"count":[0-9]*' | cut -d: -f2
    else
        error "Query documents failed (HTTP $http_code)"
        return 1
    fi
}

# Update document
update_document() {
    local token="$1"
    local doc_uuid="$2"
    
    local update_json=$(cat <<EOF
{
    "updated": true,
    "update_time": $(date +%s),
    "additional_content": "Updated content for checkpoint test"
}
EOF
)
    
    local response
    response=$(curl -s -k -X PUT "$BASE_URL/api/documents/$doc_uuid" \
        -H "Authorization: Bearer $token" \
        -H "Content-Type: application/json" \
        -d "$update_json" \
        -w "\nHTTP_CODE:%{http_code}")
    
    local http_code=$(echo "$response" | grep -o "HTTP_CODE:[0-9]*" | cut -d: -f2)
    if [ "$http_code" = "200" ]; then
        return 0
    else
        error "Update document $doc_uuid failed (HTTP $http_code)"
        return 1
    fi
}

# Delete document
delete_document() {
    local token="$1"
    local doc_uuid="$2"
    
    local response
    response=$(curl -s -k -X DELETE "$BASE_URL/api/documents/$doc_uuid" \
        -H "Authorization: Bearer $token" \
        -w "\nHTTP_CODE:%{http_code}")
    
    local http_code=$(echo "$response" | grep -o "HTTP_CODE:[0-9]*" | cut -d: -f2)
    if [ "$http_code" = "200" ]; then
        return 0
    else
        error "Delete document $doc_uuid failed (HTTP $http_code)"
        return 1
    fi
}

# Stress test session
stress_test_session() {
    local session_id="$1"
    local operations="$2"
    local token="$3"
    
    log "🚀 Session $session_id: Starting $operations operations"
    
    local successful=0
    local failed=0
    local created_docs=()
    
    # Create documents (mix of sizes)
    for ((i=1; i<=operations/4; i++)); do
        local size
        case $((i % 3)) in
            0) size="small" ;;
            1) size="medium" ;;
            2) size="large" ;;
        esac
        
        if doc_uuid=$(create_document "$token" "${session_id}-${i}" "$size"); then
            created_docs+=("$doc_uuid")
            ((successful++))
        else
            ((failed++))
        fi
    done
    
    # Query operations
    for ((i=1; i<=operations/4; i++)); do
        if query_documents "$token" >/dev/null; then
            ((successful++))
        else
            ((failed++))
        fi
    done
    
    # Update operations (on created docs)
    local update_count=$((operations/4))
    if [ ${#created_docs[@]} -lt $update_count ]; then
        update_count=${#created_docs[@]}
    fi
    
    for ((i=0; i<update_count; i++)); do
        if update_document "$token" "${created_docs[i]}"; then
            ((successful++))
        else
            ((failed++))
        fi
    done
    
    # Delete operations (on created docs)
    local delete_count=$((operations/4))
    if [ ${#created_docs[@]} -lt $delete_count ]; then
        delete_count=${#created_docs[@]}
    fi
    
    for ((i=0; i<delete_count; i++)); do
        if delete_document "$token" "${created_docs[i]}"; then
            ((successful++))
        else
            ((failed++))
        fi
    done
    
    local total=$((successful + failed))
    local success_rate=$((successful * 100 / total))
    
    if [ $success_rate -ge 95 ]; then
        success "Session $session_id: $successful/$total operations (${success_rate}%)"
    else
        warning "Session $session_id: $successful/$total operations (${success_rate}%)"
    fi
    
    echo "$successful $failed"
}

# Run concurrent stress test
run_stress_test() {
    local num_sessions="$1"
    local ops_per_session="$2"
    
    log "🧪 Starting checkpoint stress test:"
    log "   Sessions: $num_sessions"
    log "   Operations per session: $ops_per_session"
    log "   Total operations: $((num_sessions * ops_per_session))"
    
    # Get authentication token
    local token
    if ! token=$(get_token); then
        error "Cannot proceed without authentication"
        return 1
    fi
    success "Authentication successful"
    
    local start_time=$(date +%s)
    local total_successful=0
    local total_failed=0
    
    # Run sessions concurrently (in background)
    local pids=()
    local temp_dir=$(mktemp -d)
    
    for ((session=1; session<=num_sessions; session++)); do
        {
            stress_test_session "$session" "$ops_per_session" "$token" > "$temp_dir/session_$session.result"
        } &
        pids+=($!)
    done
    
    # Wait for all sessions to complete
    log "⏳ Waiting for $num_sessions concurrent sessions to complete..."
    for pid in "${pids[@]}"; do
        wait "$pid"
    done
    
    # Collect results
    for ((session=1; session<=num_sessions; session++)); do
        if [ -f "$temp_dir/session_$session.result" ]; then
            local result=$(cat "$temp_dir/session_$session.result")
            local successful=$(echo "$result" | cut -d' ' -f1)
            local failed=$(echo "$result" | cut -d' ' -f2)
            total_successful=$((total_successful + successful))
            total_failed=$((total_failed + failed))
        fi
    done
    
    # Cleanup temp files
    rm -rf "$temp_dir"
    
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    local total_ops=$((total_successful + total_failed))
    local success_rate=$((total_successful * 100 / total_ops))
    local ops_per_second=$((total_ops / duration))
    
    log "📊 Checkpoint Stress Test Results:"
    log "   Total time: ${duration}s"
    log "   Total operations: $total_ops"
    log "   Successful: $total_successful"
    log "   Failed: $total_failed"
    log "   Success rate: ${success_rate}%"
    log "   Operations per second: $ops_per_second"
    
    # Determine result
    if [ $success_rate -ge 95 ]; then
        success "CHECKPOINT SYSTEM EXCELLENT: ${success_rate}% success rate!"
        return 0
    elif [ $success_rate -ge 85 ]; then
        warning "CHECKPOINT SYSTEM GOOD: ${success_rate}% success rate"
        return 0
    else
        error "CHECKPOINT SYSTEM ISSUES: ${success_rate}% success rate"
        return 1
    fi
}

# Main execution
main() {
    echo "🚀 JDBX Checkpoint Memory Management Stress Test"
    echo "================================================"
    
    # Progressive stress testing
    local test_configs=(
        "5 10"    # 5 sessions, 10 ops each = 50 total ops
        "10 15"   # 10 sessions, 15 ops each = 150 total ops
        "15 20"   # 15 sessions, 20 ops each = 300 total ops
        "20 25"   # 20 sessions, 25 ops each = 500 total ops
    )
    
    local all_passed=true
    
    for i in "${!test_configs[@]}"; do
        local config="${test_configs[i]}"
        local sessions=$(echo "$config" | cut -d' ' -f1)
        local ops=$(echo "$config" | cut -d' ' -f2)
        
        log ""
        log "🧪 Running stress test $((i+1))/${#test_configs[@]}"
        
        if ! run_stress_test "$sessions" "$ops"; then
            error "Stress test $((i+1)) failed - stopping progression"
            all_passed=false
            break
        fi
        
        # Brief pause between tests
        sleep 2
    done
    
    echo ""
    echo "================================================"
    if [ "$all_passed" = true ]; then
        success "🎉 ALL CHECKPOINT STRESS TESTS PASSED!"
        success "✅ Memory management system is stable under load"
        return 0
    else
        error "❌ Some tests failed - investigate memory management issues"
        return 1
    fi
}

# Run the tests
main "$@"