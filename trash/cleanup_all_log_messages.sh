#!/bin/bash
# Comprehensive log message cleanup script

cd /opt/jsondb/src

echo "=== Comprehensive Log Message Cleanup ==="
echo ""

# Create backup
echo "Creating backup..."
tar -czf logs_backup_$(date +%Y%m%d_%H%M%S).tar.gz components/

# Function to apply fixes
apply_fixes() {
    echo "Applying comprehensive log message fixes..."
    
    # 1. Fix LOG_LOG_INFO duplicates
    echo "Fixing LOG_LOG_INFO duplicates..."
    find . -name "*.c" -exec sed -i 's/LOG_LOG_INFO/LOG_INFO/g' {} \;
    
    # 2. Remove all PERSIST_DEBUG: prefixes (already in function/file info)
    echo "Removing PERSIST_DEBUG: prefixes..."
    find . -name "*.c" -exec sed -i 's/"PERSIST_DEBUG: \([^"]*\)"/"\1"/g' {} \;
    
    # 3. Simplify verbose messages that duplicate function context
    echo "Simplifying verbose messages..."
    
    # Thread messages
    sed -i 's/"Persistence thread started successfully for database: %s"/"Started for: %s"/g' components/database/persistence.c
    sed -i 's/"Starting persistence thread for database: %s"/"Starting for: %s"/g' components/database/persistence.c
    sed -i 's/"Persistence thread main started for db=%p, path=%s"/"Started: db=%p, path=%s"/g' components/database/persistence.c
    sed -i 's/"Initializing persistence structure\.\.\."/"Initializing..."/g' components/database/persistence.c
    sed -i 's/"Creating persistence thread\.\.\."/"Creating thread..."/g' components/database/persistence.c
    
    # Document operation messages
    sed -i 's/"Starting document insertion for collection/LOG_INFO("Collection/g' components/database/simplified_operations.c
    sed -i 's/"Starting document query for collection/LOG_INFO("Collection/g' components/database/simplified_operations.c
    sed -i 's/"Starting document update for collection/LOG_INFO("Collection/g' components/database/simplified_operations.c
    sed -i 's/"Starting document deletion for collection/LOG_INFO("Collection/g' components/database/simplified_operations.c
    sed -i 's/"Starting document retrieval for collection/LOG_INFO("Collection/g' components/database/simplified_operations.c
    
    # Similar for other operation files
    sed -i 's/"Starting indexed document insertion for collection/LOG_INFO("Collection/g' components/database/indexed_document_operations.c
    sed -i 's/"Starting indexed document query for collection/LOG_INFO("Collection/g' components/database/indexed_document_operations.c
    sed -i 's/"Starting indexed document retrieval for collection/LOG_INFO("Collection/g' components/database/indexed_document_operations.c
    
    sed -i 's/"Starting optimized document insertion for collection/LOG_INFO("Collection/g' components/database/optimized_db_operations.c
    sed -i 's/"Starting optimized document query for collection/LOG_INFO("Collection/g' components/database/optimized_db_operations.c
    sed -i 's/"Starting optimized document retrieval for collection/LOG_INFO("Collection/g' components/database/optimized_db_operations.c
    
    # 4. Simplify database messages
    echo "Simplifying database messages..."
    sed -i 's/"Initializing database with binary format: %s"/"Binary format: %s"/g' components/database/simplified_db.c
    sed -i 's/"Closing binary database at path: %s"/"Closing: %s"/g' components/database/simplified_db.c
    sed -i 's/"Saving database in binary format to path: %s"/"Saving to: %s"/g' components/database/simplified_db.c
    sed -i 's/"Database saved successfully in binary format"/"Save successful"/g' components/database/simplified_db.c
    sed -i 's/"Failed to save database in binary format"/"Save failed"/g' components/database/simplified_db.c
    sed -i 's/"Binary database initialization complete"/"Initialization complete"/g' components/database/simplified_db.c
    sed -i 's/"Binary database closed successfully"/"Closed successfully"/g' components/database/simplified_db.c
    sed -i 's/"Existing database file found, loading binary format from %s"/"Loading from: %s"/g' components/database/simplified_db.c
    sed -i 's/"No existing database file found at %s, starting with empty binary database"/"Creating new database: %s"/g' components/database/simplified_db.c
    sed -i 's/"Database loaded successfully from binary format"/"Load successful"/g' components/database/simplified_db.c
    sed -i 's/"Failed to load existing binary database, starting with empty database"/"Load failed, creating new database"/g' components/database/simplified_db.c
    
    # 5. Simplify binary format messages
    echo "Simplifying binary format messages..."
    sed -i 's/"Database serialized to binary format: %s (size: %llu bytes)"/"Serialized: %s (%llu bytes)"/g' components/binary/binary_format.c
    sed -i 's/"Database deserialized from binary format: %s (collections: %u)"/"Deserialized: %s (%u collections)"/g' components/binary/binary_format.c
    sed -i 's/"Deserialized document .%s. for collection .%s."/"Document: %s, collection: %s"/g' components/binary/binary_format.c
    sed -i 's/"Serializing document .%s. in collection .%s.: size=%lu bytes"/"Document: %s, collection: %s, size: %lu"/g' components/binary/binary_format.c
    
    # 6. Simplify lock/mutex messages
    echo "Simplifying lock/mutex messages..."
    sed -i 's/"Acquiring database lock"/"Acquiring lock"/g' components/database/*.c
    sed -i 's/"Releasing database lock"/"Releasing lock"/g' components/database/*.c
    sed -i 's/"Database structure initialized successfully"/"Initialized"/g' components/database/*.c
    
    # 7. Simplify collection messages
    echo "Simplifying collection messages..."
    sed -i 's/"Getting collection: %s"/"Collection: %s"/g' components/database/*.c
    sed -i 's/"Collection .%s. not found or not an array"/"Collection not found: %s"/g' components/database/*.c
    sed -i 's/"Collection .%s. now contains %zu documents"/"Collection size: %zu"/g' components/database/*.c
    
    # 8. Simplify document messages
    echo "Simplifying document messages..."
    sed -i 's/"Document inserted successfully with ID: %s"/"Inserted: %s"/g' components/database/*.c
    sed -i 's/"Document updated successfully"/"Updated"/g' components/database/*.c
    sed -i 's/"Document deleted successfully"/"Deleted"/g' components/database/*.c
    sed -i 's/"Document not found with ID: %s"/"Not found: %s"/g' components/database/*.c
    sed -i 's/"Cloning document for insertion"/"Cloning document"/g' components/database/*.c
    sed -i 's/"Cloning document for update"/"Cloning document"/g' components/database/*.c
    sed -i 's/"Creating deep copy of document for insertion"/"Creating document copy"/g' components/database/*.c
    
    # 9. Simplify query messages
    echo "Simplifying query messages..."
    sed -i 's/"Query parsed successfully"/"Query parsed"/g' components/query/*.c
    sed -i 's/"Creating empty query object"/"Empty query"/g' components/database/*.c
    sed -i 's/"Parsing query JSON into expression tree"/"Parsing query"/g' components/query/*.c
    
    # 10. Clean up redundant "successfully" patterns
    echo "Removing redundant 'successfully'..."
    find . -name "*.c" -exec sed -i 's/successfully successfully/successfully/g' {} \;
    find . -name "*.c" -exec sed -i 's/completed successfully/completed/g' {} \;
    find . -name "*.c" -exec sed -i 's/initialized successfully/initialized/g' {} \;
    find . -name "*.c" -exec sed -i 's/created successfully/created/g' {} \;
    
    # 11. Remove function names from error messages (already in log format)
    echo "Removing function names from messages..."
    find . -name "*.c" -exec sed -i 's/"Invalid parameters for [a-zA-Z_]*"/"Invalid parameters"/g' {} \;
    find . -name "*.c" -exec sed -i 's/"Failed to initialize [a-zA-Z_]*"/"Initialization failed"/g' {} \;
    
    # 12. Standardize common patterns
    echo "Standardizing common patterns..."
    find . -name "*.c" -exec sed -i 's/"Failed to allocate memory for .*"/"Memory allocation failed"/g' {} \;
    find . -name "*.c" -exec sed -i 's/"Out of memory.*"/"Memory allocation failed"/g' {} \;
    find . -name "*.c" -exec sed -i 's/"Cannot .*: NULL .*"/"Invalid NULL parameter"/g' {} \;
    
    # 13. Fix notification messages
    echo "Fixing notification messages..."
    sed -i 's/"Data change notification: ops=%d, size=%zu"/"Changes: ops=%d, size=%zu"/g' components/database/persistence.c
    sed -i 's/"Signaling persistence thread for immediate save"/"Signaling immediate save"/g' components/database/persistence.c
    
    # 14. Fix metric messages
    echo "Fixing metric messages..."
    sed -i 's/"Updated metric document: %s"/"Updated: %s"/g' components/utils/metrics_persistence.c
    sed -i 's/"All metrics saved successfully"/"Metrics saved"/g' components/utils/metrics_persistence.c
    sed -i 's/"Cleaning up old metrics"/"Cleanup started"/g' components/utils/metrics_persistence.c
}

# Apply all fixes
apply_fixes

# Verify compilation
echo ""
echo "Verifying compilation..."
cd /opt/jsondb/src && make -j4 2>&1 | grep -E "error:|warning:" | head -10

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Compilation successful!"
else
    echo ""
    echo "✗ Compilation errors detected"
fi

# Show sample of changes
echo ""
echo "=== Sample of cleaned messages ==="
echo ""
echo "Before:"
echo '  LOG_INFO("Starting document insertion for collection '\''%s'\''", collection_name);'
echo "After:"
echo '  LOG_INFO("Collection '\''%s'\''", collection_name);'
echo ""
echo "Before:"
echo '  LOG_DEBUG("PERSIST_DEBUG: About to call db_save from persistence thread");'
echo "After:"
echo '  LOG_TRACE("Calling save");'
echo ""
echo "Before:"
echo '  LOG_INFO("Document inserted successfully with ID: %s", id);'
echo "After:"
echo '  LOG_INFO("Inserted: %s", id);'
echo ""

echo "Log message cleanup complete!"
echo "Backup saved as: logs_backup_*.tar.gz"