#!/bin/bash
# Script to update any remaining include paths that still reference the old structure
# This is the final step in the include migration process

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Starting final include path update...${NC}"

# 1. Update includes referencing "include/jsondb/" to use src/include/ structure
echo -e "${YELLOW}Updating 'include/jsondb/' references...${NC}"
find . -type f -name "*.c" -o -name "*.h" | xargs grep -l "#include.*include/jsondb" | while read file; do
  echo "Processing: $file"
  # Replace include/jsondb/api/* with "src/include/api/*"
  sed -i 's|#include.*"include/jsondb/api/\(.*\)"|#include "src/include/api/\1"|g' "$file"
  # Replace include/jsondb/core/* with "src/include/core/*"
  sed -i 's|#include.*"include/jsondb/core/\(.*\)"|#include "src/include/core/\1"|g' "$file"
  # Replace include/jsondb/database/* with "src/include/database/*"
  sed -i 's|#include.*"include/jsondb/database/\(.*\)"|#include "src/include/database/\1"|g' "$file"
  # Replace include/jsondb/js/* with "src/include/js/*"
  sed -i 's|#include.*"include/jsondb/js/\(.*\)"|#include "src/include/js/\1"|g' "$file"
  # Replace include/jsondb/query/* with "src/include/query/*"
  sed -i 's|#include.*"include/jsondb/query/\(.*\)"|#include "src/include/query/\1"|g' "$file"
  # Replace include/jsondb/rbac/* with "src/include/rbac/*"
  sed -i 's|#include.*"include/jsondb/rbac/\(.*\)"|#include "src/include/rbac/\1"|g' "$file"
  # Replace include/jsondb/transaction/* with "src/include/transaction/*"
  sed -i 's|#include.*"include/jsondb/transaction/\(.*\)"|#include "src/include/transaction/\1"|g' "$file"
  # Replace include/jsondb/utils/* with "src/include/utils/*"
  sed -i 's|#include.*"include/jsondb/utils/\(.*\)"|#include "src/include/utils/\1"|g' "$file"
  # Handle the special case where someone might have included memory-related headers
  sed -i 's|#include.*"include/jsondb/utils/memory/\(.*\)"|#include "src/include/utils/memory/\1"|g' "$file"
done

# 2. Update includes with relative paths to parent directory (../include/*)
echo -e "${YELLOW}Updating relative paths '../include/*' references...${NC}"
find . -type f -name "*.c" -o -name "*.h" | xargs grep -l "#include.*\.\./include" | while read file; do
  echo "Processing: $file"
  # Get directory of file
  dir=$(dirname "$file")
  # Replace ../include/ with appropriate path to src/include/
  sed -i 's|#include.*"\.\./include/\(.*\)"|#include "src/include/\1"|g' "$file"
done

# 3. Update the basic include/jsondb.h usage
echo -e "${YELLOW}Updating 'jsondb.h' references...${NC}"
find . -type f -name "*.c" -o -name "*.h" | xargs grep -l "#include.*jsondb.h" | while read file; do
  echo "Processing: $file"
  # Replace include/jsondb.h with src/include/jsondb.h
  sed -i 's|#include.*"jsondb.h"|#include "src/include/jsondb.h"|g' "$file"
done

# 4. Update includes with "quickjs.h" and "quickjs-libc.h"
echo -e "${YELLOW}Updating QuickJS header references...${NC}"
find . -type f -name "*.c" -o -name "*.h" | xargs grep -l "#include.*quickjs" | while read file; do
  # Don't update the actual QuickJS headers
  if [[ "$file" == *"/quickjs.h" || "$file" == *"/quickjs-libc.h" ]]; then
    continue
  fi
  echo "Processing: $file"
  # Replace with appropriate path
  sed -i 's|#include.*"quickjs.h"|#include "src/include/js/quickjs.h"|g' "$file"
  sed -i 's|#include.*"quickjs-libc.h"|#include "src/include/js/quickjs-libc.h"|g' "$file"
done

# 5. Update component references to use consistent format
echo -e "${YELLOW}Standardizing component references...${NC}"
find . -type f -name "*.c" -o -name "*.h" | xargs grep -l "#include.*components/" | while read file; do
  echo "Processing: $file"
  # File in components directory should use relative paths from src/include 
  if [[ "$file" == *"/components/"* ]]; then
    # Skip the components directory itself to avoid breaking internal component references
    continue
  fi
  # Replace components/ paths with the appropriate src/include/ paths
  sed -i 's|#include.*"components/api/\(.*\)"|#include "src/include/api/\1"|g' "$file"
  sed -i 's|#include.*"components/core/\(.*\)"|#include "src/include/core/\1"|g' "$file"
  sed -i 's|#include.*"components/database/\(.*\)"|#include "src/include/database/\1"|g' "$file"
  sed -i 's|#include.*"components/js/\(.*\)"|#include "src/include/js/\1"|g' "$file"
  sed -i 's|#include.*"components/query/\(.*\)"|#include "src/include/query/\1"|g' "$file"
  sed -i 's|#include.*"components/rbac/\(.*\)"|#include "src/include/rbac/\1"|g' "$file"
  sed -i 's|#include.*"components/transaction/\(.*\)"|#include "src/include/transaction/\1"|g' "$file"
  sed -i 's|#include.*"components/utils/\(.*\)"|#include "src/include/utils/\1"|g' "$file"
done

echo -e "${GREEN}Include path updates completed!${NC}"
echo -e "${YELLOW}Please review and test the changes to ensure everything works correctly.${NC}"