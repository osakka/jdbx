#!/bin/bash

# Convert remaining files with direct memory calls

set -e

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

# Remaining files to convert
REMAINING_FILES=(
    "/opt/jdbx/src/components/api/library_api.c"
    "/opt/jdbx/src/components/api/session_terminate_api.c"
    "/opt/jdbx/src/components/core/http_request.c"
    "/opt/jdbx/src/components/core/server.c"
    "/opt/jdbx/src/components/core/server_init_sequence.c"
    "/opt/jdbx/src/components/core/server_thread_safe.c"
    "/opt/jdbx/src/components/core/static_auth.c"
    "/opt/jdbx/src/components/core/static_files.c"
    "/opt/jdbx/src/components/rbac/rbac_db.c"
    "/opt/jdbx/src/components/rbac/rbac_refcount.c"
    "/opt/jdbx/src/components/transaction/transaction_retry.c"
    "/opt/jdbx/src/components/utils/library_metrics.c"
    "/opt/jdbx/src/components/utils/logger.c"
    "/opt/jdbx/src/components/utils/ref_counter.c"
    "/opt/jdbx/src/include/utils/skiplist.h"
)

# Backup directory
BACKUP_DIR="/opt/jdbx/backup_final_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$BACKUP_DIR"

echo -e "${GREEN}Converting remaining files to buffer pool...${NC}"

for file in "${REMAINING_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo -e "${BLUE}Converting: $(basename $file)${NC}"
        
        # Backup file
        cp "$file" "$BACKUP_DIR/$(basename $file)"
        
        # Perform conversions
        sed -i 's/\bmalloc\s*(/BUFFER_ALLOC(/g' "$file"
        sed -i 's/\bfree\s*(/BUFFER_FREE(/g' "$file"
        sed -i 's/\bcalloc\s*(/BUFFER_CALLOC(/g' "$file"
        sed -i 's/\brealloc\s*(/BUFFER_REALLOC(/g' "$file"
        sed -i 's/\bstrdup\s*(/BUFFER_STRDUP(/g' "$file"
        
        # Handle casts before malloc
        sed -i 's/(\([^)]*\))\s*BUFFER_ALLOC(/(\1)BUFFER_ALLOC(/g' "$file"
        sed -i 's/(\([^)]*\))\s*BUFFER_CALLOC(/(\1)BUFFER_CALLOC(/g' "$file"
        sed -i 's/(\([^)]*\))\s*BUFFER_REALLOC(/(\1)BUFFER_REALLOC(/g' "$file"
        
        # Add buffer_pool.h include if not present
        if ! grep -q '#include.*buffer_pool\.h' "$file"; then
            # Find the last include
            last_include_line=$(grep -n "^#include" "$file" | tail -1 | cut -d: -f1)
            if [ -n "$last_include_line" ]; then
                sed -i "${last_include_line}a\\#include \"utils/buffer_pool.h\"" "$file"
                echo "  Added buffer_pool.h include"
            fi
        fi
    fi
done

echo -e "${GREEN}Conversion complete!${NC}"
echo "Backup saved to: $BACKUP_DIR"