#!/bin/bash

# Fix include paths in header files after reorganization
# This script updates include statements to use the jsondb namespace

# Process all header files in include directory
for file in $(find include -name "*.h"); do
  # Update includes
  sed -i 's|#include "core/|#include "jsondb/core/|g' "$file"
  sed -i 's|#include "api/|#include "jsondb/api/|g' "$file"
  sed -i 's|#include "database/|#include "jsondb/database/|g' "$file"
  sed -i 's|#include "js/|#include "jsondb/js/|g' "$file"
  sed -i 's|#include "query/|#include "jsondb/query/|g' "$file"
  sed -i 's|#include "rbac/|#include "jsondb/rbac/|g' "$file"
  sed -i 's|#include "transaction/|#include "jsondb/transaction/|g' "$file"
  sed -i 's|#include "utils/|#include "jsondb/utils/|g' "$file"
  
  # Don't change direct includes of system headers
  echo "Fixed includes in $file"
done

echo "All header include paths have been updated."