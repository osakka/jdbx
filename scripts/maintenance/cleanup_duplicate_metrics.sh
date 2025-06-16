#!/bin/bash

# JDBX Duplicate Metrics Cleanup Script
# Removes duplicate metric documents, keeping only the most recent one for each metric type

echo "🧹 JDBX Duplicate Metrics Cleanup"
echo "================================="

# Check if server is running
if ! curl -k -s https://localhost:5000/api/health >/dev/null 2>&1; then
    echo "❌ JDBX server is not running or not accessible"
    exit 1
fi

# Get admin token
echo "🔐 Authenticating..."
TOKEN=$(curl -k -s -X POST https://localhost:5000/api/login \
    -H "Content-Type: application/json" \
    -d '{"username": "admin", "password": "admin"}' | jq -r '.token')

if [ "$TOKEN" = "null" ] || [ -z "$TOKEN" ]; then
    echo "❌ Authentication failed"
    exit 1
fi

echo "✅ Authenticated successfully"

# Get metrics count before cleanup
BEFORE_COUNT=$(curl -k -s -X GET "https://localhost:5000/api/documents?type=metric&library=system" \
    -H "Authorization: Bearer $TOKEN" | jq -r '.count')

echo "📊 Current metrics documents: $BEFORE_COUNT"

# Define metric types to deduplicate
METRIC_TYPES=("operations" "performance" "cache" "memory" "connections")

echo "🔍 Cleaning up duplicate metrics..."

for METRIC_TYPE in "${METRIC_TYPES[@]}"; do
    echo "  Processing $METRIC_TYPE metrics..."
    
    # Get all documents for this metric type
    DOCS=$(curl -k -s -X GET "https://localhost:5000/api/documents?type=metric&library=system&name=$METRIC_TYPE" \
        -H "Authorization: Bearer $TOKEN" | jq -r '.documents[]')
    
    # Parse documents and sort by updated_at timestamp
    SORTED_DOCS=$(echo "$DOCS" | jq -s 'sort_by(.updated_at) | reverse')
    
    # Keep the first (most recent) document, delete the rest
    DOCS_TO_DELETE=$(echo "$SORTED_DOCS" | jq -r '.[1:][].uuid // empty')
    
    if [ ! -z "$DOCS_TO_DELETE" ]; then
        DELETE_COUNT=$(echo "$DOCS_TO_DELETE" | wc -l)
        echo "    🗑️  Deleting $DELETE_COUNT duplicate $METRIC_TYPE documents..."
        
        echo "$DOCS_TO_DELETE" | while read -r UUID; do
            if [ ! -z "$UUID" ]; then
                curl -k -s -X DELETE "https://localhost:5000/api/documents/$UUID" \
                    -H "Authorization: Bearer $TOKEN" >/dev/null
            fi
        done
    else
        echo "    ✅ No duplicates found for $METRIC_TYPE"
    fi
done

# Get metrics count after cleanup
AFTER_COUNT=$(curl -k -s -X GET "https://localhost:5000/api/documents?type=metric&library=system" \
    -H "Authorization: Bearer $TOKEN" | jq -r '.count')

CLEANED_COUNT=$((BEFORE_COUNT - AFTER_COUNT))

echo ""
echo "🎯 Cleanup Results:"
echo "   Before: $BEFORE_COUNT documents"
echo "   After:  $AFTER_COUNT documents"
echo "   Cleaned: $CLEANED_COUNT duplicate documents"

if [ $CLEANED_COUNT -gt 0 ]; then
    echo "✅ Duplicate metrics cleanup completed successfully!"
else
    echo "ℹ️  No duplicate metrics found to clean up"
fi

echo ""
echo "🔄 Restarting server to reset metric IDs..."
/opt/jdbx/build/jdbx_runtime.sh restart

echo "✅ Cleanup complete!"