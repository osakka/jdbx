#!/bin/bash
# Script to migrate all direct memory allocations to use memory manager

echo "Starting comprehensive memory manager migration..."

# Function to replace allocation calls in a file
replace_in_file() {
    local file=$1
    echo "Processing: $file"
    
    # Create backup
    cp "$file" "$file.bak"
    
    # Replace malloc with BUFFER_ALLOC
    sed -i 's/malloc(/BUFFER_ALLOC(/g' "$file"
    
    # Replace calloc with BUFFER_CALLOC
    sed -i 's/calloc(/BUFFER_CALLOC(/g' "$file"
    
    # Replace free with BUFFER_FREE (but not in function names)
    sed -i 's/\bfree(/BUFFER_FREE(/g' "$file"
    
    # Replace realloc with BUFFER_REALLOC
    sed -i 's/realloc(/BUFFER_REALLOC(/g' "$file"
    
    # Replace strdup with BUFFER_STRDUP
    sed -i 's/strdup(/BUFFER_STRDUP(/g' "$file"
    
    # Check if the file includes buffer_pool.h, if not add it
    if ! grep -q "utils/buffer_pool.h" "$file"; then
        # Add include after the first include statement
        sed -i '0,/#include/{s/#include/#include "utils\/buffer_pool.h"\n#include/}' "$file"
    fi
}

# List of files to migrate (from the audit)
FILES=(
    "src/components/api/health_api.c"
    "src/components/core/api.c"
    "src/components/core/client_connection.c"
    "src/components/core/thread_pool.c"
    "src/components/database/batch_operations.c"
    "src/components/database/database.c"
    "src/components/database/index_cleanup.c"
    "src/components/database/lock_manager.c"
    "src/components/database/versioning_policy.c"
    "src/components/lockfree/hazard_pointer.c"
    "src/components/storage/jdbx_btree.c"
    "src/components/storage/jdbx_page_manager.c"
    "src/components/transaction/transaction.c"
    "src/components/transaction/transaction_log.c"
    "src/components/utils/generic_cache.c"
    "src/components/utils/skiplist.c"
    "src/initialize/config.c"
    "src/initialize/logger_init.c"
)

# Process each file
for file in "${FILES[@]}"; do
    if [ -f "$file" ]; then
        replace_in_file "$file"
    else
        echo "Warning: $file not found"
    fi
done

echo "Migration complete! Please review the changes and compile."