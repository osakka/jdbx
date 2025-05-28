#!/bin/bash

# Targeted cleanup of specific log message patterns identified in the audit

echo "=== Starting targeted log message cleanup ==="

cd /opt/jsondb/src

# 1. Remove "successfully" from INFO messages - it's redundant
echo "Removing 'successfully' from INFO messages..."
find . -name "*.c" -type f -exec sed -i 's/LOG_INFO("\([^"]*\) successfully\([^"]*\)"/LOG_INFO("\1\2"/g' {} \;
find . -name "*.c" -type f -exec sed -i 's/LOG_INFO("\([^"]*\) successfully")/LOG_INFO("\1")/g' {} \;

# 2. Remove "Starting" from INFO messages - make them more direct
echo "Simplifying 'Starting' messages..."
find . -name "*.c" -type f -exec sed -i 's/LOG_INFO("Starting \([^"]*\)"/LOG_INFO("\1"/g' {} \;

# 3. Remove "Failed to" from ERROR messages - LOG_ERROR already indicates failure
echo "Simplifying ERROR messages..."
find . -name "*.c" -type f -exec sed -i 's/LOG_ERROR("Failed to \([^"]*\)"/LOG_ERROR("\1"/g' {} \;

# 4. Standardize memory error messages
echo "Standardizing memory error messages..."
find . -name "*.c" -type f -exec sed -i 's/"Memory allocation failed"/"Out of memory"/g' {} \;
find . -name "*.c" -type f -exec sed -i 's/"allocation failed"/"Out of memory"/g' {} \;
find . -name "*.c" -type f -exec sed -i 's/"malloc failed"/"Out of memory"/g' {} \;

# 5. Remove verbose context from messages
echo "Removing verbose context..."
find . -name "*.c" -type f -exec sed -i 's/" for collection '\''%s'\''"/""/g' {} \;
find . -name "*.c" -type f -exec sed -i 's/" in collection '\''%s'\''"/""/g' {} \;
find . -name "*.c" -type f -exec sed -i 's/", collection_name/"/g' {} \;

# 6. Simplify document operation messages
echo "Simplifying document operation messages..."
find . -name "*.c" -type f -exec sed -i 's/"document insertion for collection"/"insertion"/g' {} \;
find . -name "*.c" -type f -exec sed -i 's/"document retrieval for collection"/"retrieval"/g' {} \;
find . -name "*.c" -type f -exec sed -i 's/"document update for collection"/"update"/g' {} \;
find . -name "*.c" -type f -exec sed -i 's/"document deletion for collection"/"deletion"/g' {} \;
find . -name "*.c" -type f -exec sed -i 's/"document query for collection"/"query"/g' {} \;

# 7. Fix specific verbose messages
echo "Fixing specific verbose messages..."
sed -i 's/"Processing cache invalidations for binary format database"/"Cache invalidation"/g' components/database/simplified_db.c
sed -i 's/"RBAC save with enhanced fixed implementation"/"RBAC save"/g' components/rbac/rbac_db_fix.c
sed -i 's/"RBAC initialization"/"RBAC init"/g' components/rbac/rbac_enhanced.c
sed -i 's/"server with thread pool implementation"/"server"/g' components/core/server.c
sed -i 's/"accept loop on socket %d (port %d)"/"accept loop: socket=%d port=%d"/g' components/core/server.c

# 8. Clean up capitalization - make consistent (capitalize first letter)
echo "Standardizing capitalization..."
# Fix lowercase starts to uppercase
find . -name "*.c" -type f -exec sed -i 's/LOG_\([A-Z]*\)("binary_/LOG_\1("Binary_/g' {} \;
find . -name "*.c" -type f -exec sed -i 's/LOG_\([A-Z]*\)("rbac_/LOG_\1("RBAC_/g' {} \;
find . -name "*.c" -type f -exec sed -i 's/LOG_\([A-Z]*\)("select()/LOG_\1("Select()/g' {} \;
find . -name "*.c" -type f -exec sed -i 's/LOG_\([A-Z]*\)("accept()/LOG_\1("Accept()/g' {} \;
find . -name "*.c" -type f -exec sed -i 's/LOG_\([A-Z]*\)("should_save_now/LOG_\1("Should_save_now/g' {} \;

# 9. Shorten long messages
echo "Shortening long messages..."
# Shorten transaction log replay message
sed -i 's/"Transaction log replay completed: %d operations processed, %d active transactions"/"Replay complete: %d ops, %d active"/g' components/binary/binary_transactions.c
# Shorten index creation error
sed -i 's/"create index: Invalid parameters (db: %p, collection: %s, name: %s, field_path: %s)"/"create index: invalid params"/g' components/database/index.c
# Shorten full scan warning
sed -i 's/"Performing full collection scan for query on '\''%s.%s'\'' - consider creating an index"/"Full scan on %s.%s - needs index"/g' components/database/index_optimized.c

# 10. Clean up any double spaces created
echo "Cleaning up formatting..."
find . -name "*.c" -type f -exec sed -i 's/  / /g' {} \;
find . -name "*.c" -type f -exec sed -i 's/" "/""/g' {} \;

echo "=== Targeted cleanup complete ==="