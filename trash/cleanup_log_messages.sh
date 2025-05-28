#!/bin/bash
# Script to clean up redundant log messages

cd /opt/jsondb/src

echo "=== Cleaning up redundant log messages ==="
echo ""

# Backup all files first
echo "Creating backups..."
find . -name "*.c" -exec cp {} {}.bak \;

# Function to clean a specific pattern
cleanup_pattern() {
    local pattern="$1"
    local replacement="$2"
    local description="$3"
    
    echo "Cleaning: $description"
    find . -name "*.c" -exec sed -i "$pattern" {} \;
}

# 1. Remove PERSIST_DEBUG: prefix
cleanup_pattern 's/LOG_\([A-Z]*\)("PERSIST_DEBUG: /LOG_\1("/g' "" "Removing PERSIST_DEBUG: prefix"

# 2. Remove "Entering function" messages - these are redundant with function name in log
cleanup_pattern 's/LOG_[A-Z]*("Entering function.*");//g' "" "Removing 'Entering function' messages"
cleanup_pattern 's/LOG_[A-Z]*("Exiting function.*");//g' "" "Removing 'Exiting function' messages"

# 3. Clean up specific redundant patterns
echo "Cleaning specific redundant patterns..."

# Replace redundant persistence messages
sed -i 's/LOG_DEBUG("Persistence thread entering main loop");/LOG_TRACE("Main loop started");/g' components/database/persistence.c
sed -i 's/LOG_DEBUG("Persistence thread exiting");/LOG_TRACE("Thread shutting down");/g' components/database/persistence.c
sed -i 's/LOG_DEBUG("Persistence thread triggering database save");/LOG_DEBUG("Triggering save");/g' components/database/persistence.c
sed -i 's/LOG_DEBUG("Persistence thread save completed successfully");/LOG_DEBUG("Save completed");/g' components/database/persistence.c
sed -i 's/LOG_DEBUG("Persistence thread structure initialized");/LOG_TRACE("Initialized");/g' components/database/persistence.c
sed -i 's/LOG_DEBUG("Persistence thread structure cleaned up");/LOG_TRACE("Cleaned up");/g' components/database/persistence.c

# Clean binary format messages
sed -i 's/LOG_DEBUG("Freeing temp buffer");/LOG_TRACE("Freeing buffer");/g' components/binary/binary_format.c
sed -i 's/LOG_DEBUG("Temp buffer freed");/LOG_TRACE("Buffer freed");/g' components/binary/binary_format.c
sed -i 's/LOG_DEBUG("Updating header - db_size=%zu, collection_count=%u"/LOG_DEBUG("Header: size=%zu, collections=%u"/g' components/binary/binary_format.c
sed -i 's/LOG_DEBUG("Calculating checksum");/LOG_TRACE("Calculating checksum");/g' components/binary/binary_format.c
sed -i 's/LOG_DEBUG("Checksum calculated: 0x%08x"/LOG_DEBUG("Checksum: 0x%08x"/g' components/binary/binary_format.c
sed -i 's/LOG_DEBUG("Seeking to beginning of file");/LOG_TRACE("Seeking to start");/g' components/binary/binary_format.c
sed -i 's/LOG_DEBUG("Writing updated header");/LOG_TRACE("Writing header");/g' components/binary/binary_format.c
sed -i 's/LOG_DEBUG("Header written successfully");/LOG_TRACE("Header written");/g' components/binary/binary_format.c
sed -i 's/LOG_DEBUG("About to close file descriptor %d"/LOG_TRACE("Closing fd %d"/g' components/binary/binary_format.c
sed -i 's/LOG_DEBUG("File closed successfully");/LOG_TRACE("File closed");/g' components/binary/binary_format.c
sed -i 's/LOG_DEBUG("Checking database pointer before resetting is_modified flag");/LOG_TRACE("Checking database pointer");/g' components/binary/binary_format.c
sed -i 's/LOG_DEBUG("Database pointer valid at %p, resetting is_modified flag"/LOG_TRACE("Resetting modified flag: db=%p"/g' components/binary/binary_format.c
sed -i 's/LOG_DEBUG("is_modified flag reset successfully");/LOG_TRACE("Modified flag reset");/g' components/binary/binary_format.c
sed -i 's/LOG_DEBUG("binary_serialize_database returning success");/LOG_TRACE("Serialization complete");/g' components/binary/binary_format.c

# Clean simplified_db messages
sed -i 's/LOG_DEBUG("About to call binary_serialize_database");/LOG_TRACE("Starting serialization");/g' components/database/simplified_db.c
sed -i 's/LOG_DEBUG("binary_serialize_database returned %d"/LOG_TRACE("Serialization result: %d"/g' components/database/simplified_db.c
sed -i 's/LOG_DEBUG("About to unlock database mutex");/LOG_TRACE("Unlocking mutex");/g' components/database/simplified_db.c
sed -i 's/LOG_DEBUG("Database mutex unlocked, db_save returning %d"/LOG_TRACE("Save complete: %d"/g' components/database/simplified_db.c

# Clean persistence.c messages
sed -i 's/LOG_DEBUG("About to call db_save from persistence thread");/LOG_TRACE("Calling save");/g' components/database/persistence.c
sed -i 's/LOG_DEBUG("db_save returned %d"/LOG_TRACE("Save result: %d"/g' components/database/persistence.c
sed -i 's/LOG_DEBUG("About to re-lock persistence mutex");/LOG_TRACE("Re-locking mutex");/g' components/database/persistence.c
sed -i 's/LOG_DEBUG("Re-locked persistence mutex");/LOG_TRACE("Mutex re-locked");/g' components/database/persistence.c
sed -i 's/LOG_DEBUG("Stopping persistence thread for database: %s"/LOG_DEBUG("Stopping thread: %s"/g' components/database/persistence.c
sed -i 's/LOG_DEBUG("Persistence thread stopped successfully");/LOG_DEBUG("Thread stopped");/g' components/database/persistence.c

# Remove function names from messages when they're redundant
sed -i 's/LOG_ERROR("Invalid parameters for [a-z_]*");/LOG_ERROR("Invalid parameters");/g' components/database/*.c
sed -i 's/LOG_ERROR("Failed to initialize [a-z_]*");/LOG_ERROR("Initialization failed");/g' components/**/*.c

# Clean up thread pool messages
sed -i 's/LOG_TRACE("Entering thread_pool_add_work: function=%p, argument=%p"/LOG_TRACE("Work item: func=%p, arg=%p"/g' components/core/thread_pool.c

# Clean up file path references (file is already in log)
find . -name "*.c" -exec sed -i 's/LOG_[A-Z]*(".*at path: %s".*,.*path.*);/LOG_&/g' {} \;

# Remove duplicate "successfully" patterns
find . -name "*.c" -exec sed -i 's/successfully successfully/successfully/g' {} \;

# Show what changed
echo ""
echo "=== Files modified ==="
for file in $(find . -name "*.c"); do
    if ! cmp -s "$file" "${file}.bak"; then
        echo "Modified: $file"
    fi
done

# Clean up backups
echo ""
read -p "Remove backup files? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    find . -name "*.c.bak" -delete
    echo "Backup files removed"
fi

echo ""
echo "Log message cleanup complete!"