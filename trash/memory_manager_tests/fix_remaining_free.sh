#!/bin/bash

# Fix remaining free calls in specific files

echo "Fixing remaining free calls..."

# Files to fix
FILES=(
    "/opt/jdbx/src/components/api/static_api.c"
    "/opt/jdbx/src/components/js/js_api.c"
    "/opt/jdbx/src/components/rbac/rbac_persistence.c"
    "/opt/jdbx/src/components/rbac/rbac_permissions.c"
    "/opt/jdbx/src/initialize/database_init.c"
    "/opt/jdbx/src/initialize/core.c"
    "/opt/jdbx/src/initialize/rbac.c"
)

for file in "${FILES[@]}"; do
    if [ -f "$file" ]; then
        echo "Processing: $file"
        
        # Replace free( with BUFFER_FREE(
        sed -i 's/\bfree\s*(/BUFFER_FREE(/g' "$file"
        
        # Add buffer_pool.h include if not present
        if ! grep -q '#include.*buffer_pool\.h' "$file"; then
            # Find the last include
            last_include_line=$(grep -n "^#include" "$file" | tail -1 | cut -d: -f1)
            if [ -n "$last_include_line" ]; then
                sed -i "${last_include_line}a\\#include \"utils/buffer_pool.h\"" "$file"
            fi
        fi
    fi
done

echo "Done!"