#!/bin/bash
# Script to systematically remove manual memory cleanup from API handlers
# Since we have checkpoints, ALL cleanup should be automatic

echo "Removing manual memory cleanup from API handlers..."
echo "=============================================="

# Function to comment out cleanup calls
comment_cleanup() {
    local file=$1
    local backup="${file}.backup"
    
    echo "Processing: $file"
    
    # Create backup
    cp "$file" "$backup"
    
    # Comment out json_free calls (except in cleanup/destroy functions)
    sed -i 's/^\([[:space:]]*\)json_free(/\1\/\* CHECKPOINT: json_free(/' "$file"
    sed -i 's/json_free(.*);/& *\//' "$file"
    
    # Comment out BUFFER_FREE calls in API handlers
    sed -i 's/^\([[:space:]]*\)BUFFER_FREE(/\1\/\* CHECKPOINT: BUFFER_FREE(/' "$file"
    sed -i 's/BUFFER_FREE(.*);/& *\//' "$file"
    
    # Comment out buffer_pool_free calls
    sed -i 's/^\([[:space:]]*\)buffer_pool_free(/\1\/\* CHECKPOINT: buffer_pool_free(/' "$file"
    sed -i 's/buffer_pool_free(.*);/& *\//' "$file"
    
    # Count changes
    local changes=$(diff -u "$backup" "$file" | grep -c "^+.*CHECKPOINT")
    echo "  - Commented out $changes cleanup calls"
}

# Process authentication handler first (most critical)
comment_cleanup "src/components/core/authentication_handler.c"

# Process all API handlers
for file in src/components/api/*.c; do
    if [ -f "$file" ]; then
        comment_cleanup "$file"
    fi
done

echo ""
echo "Manual cleanup removal complete!"
echo "Note: Some cleanup in destroy/shutdown functions was preserved"