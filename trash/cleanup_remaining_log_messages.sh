#!/bin/bash

# Script to clean up remaining verbose log messages and ensure consistency
# This is part 2 of the logging cleanup effort

echo "Cleaning up remaining verbose log messages..."

# Find all source files
cd /opt/jsondb/src

# Clean up verbose "successfully" patterns
echo "Removing redundant 'successfully' from messages..."
find . -name "*.c" -type f -exec sed -i 's/"Successfully \([^"]*\)"/"\u\1"/g' {} \;
find . -name "*.c" -type f -exec sed -i 's/"successfully \([^"]*\)"/"\1"/g' {} \;

# Clean up long messages that just describe the function (since function name is already in log)
echo "Simplifying function description messages..."
find . -name "*.c" -type f -exec sed -i 's/"Starting to \([^"]*\)"/"\u\1"/g' {} \;
find . -name "*.c" -type f -exec sed -i 's/"Beginning \([^"]*\)"/"\u\1"/g' {} \;
find . -name "*.c" -type f -exec sed -i 's/"Attempting to \([^"]*\)"/"\u\1"/g' {} \;
find . -name "*.c" -type f -exec sed -i 's/"Trying to \([^"]*\)"/"\u\1"/g' {} \;

# Clean up messages that say "Failed to" when LOG_ERROR already indicates failure
echo "Simplifying error messages..."
find . -name "*.c" -type f -exec sed -i 's/LOG_ERROR("Failed to \([^"]*\)"/LOG_ERROR("\u\1"/g' {} \;

# Clean up messages with redundant "Error:" prefix
echo "Removing redundant 'Error:' prefixes..."
find . -name "*.c" -type f -exec sed -i 's/LOG_ERROR("Error: \([^"]*\)"/LOG_ERROR("\u\1"/g' {} \;

# Clean up overly verbose connection/request messages
echo "Simplifying connection messages..."
find . -name "*.c" -type f -exec sed -i 's/"Accepted new connection from \([^"]*\)"/"Connection: \1"/g' {} \;
find . -name "*.c" -type f -exec sed -i 's/"Handling request: \([^"]*\)"/"Request: \1"/g' {} \;
find . -name "*.c" -type f -exec sed -i 's/"Processing request: \([^"]*\)"/"Request: \1"/g' {} \;

# Convert overly verbose debug messages to be more concise
echo "Simplifying debug messages..."
find . -name "*.c" -type f -exec sed -i 's/LOG_DEBUG("Checking \([^"]*\)"/LOG_DEBUG("Check: \1"/g' {} \;
find . -name "*.c" -type f -exec sed -i 's/LOG_DEBUG("Looking for \([^"]*\)"/LOG_DEBUG("Find: \1"/g' {} \;
find . -name "*.c" -type f -exec sed -i 's/LOG_DEBUG("Searching for \([^"]*\)"/LOG_DEBUG("Search: \1"/g' {} \;

# Clean up messages that include "in function" or similar (function name is already in log)
echo "Removing redundant function references..."
find . -name "*.c" -type f -exec sed -i 's/" in function [^"]*"/""/g' {} \;
find . -name "*.c" -type f -exec sed -i 's/" in [a-zA-Z_]*() function"/""/g' {} \;

# Standardize common patterns
echo "Standardizing common patterns..."
find . -name "*.c" -type f -exec sed -i 's/"Memory allocation failed"/"Out of memory"/g' {} \;
find . -name "*.c" -type f -exec sed -i 's/"Failed to allocate memory"/"Out of memory"/g' {} \;
find . -name "*.c" -type f -exec sed -i 's/"malloc failed"/"Out of memory"/g' {} \;

# Clean up "for collection" and similar redundant context
echo "Removing redundant context..."
find . -name "*.c" -type f -exec sed -i 's/" for collection [^"]*"/""/g' {} \;
find . -name "*.c" -type f -exec sed -i 's/" in collection [^"]*"/""/g' {} \;

# Fix any double spaces created by our replacements
echo "Fixing double spaces..."
find . -name "*.c" -type f -exec sed -i 's/"  /" /g' {} \;
find . -name "*.c" -type f -exec sed -i 's/  / /g' {} \;

# Fix empty log messages that might have been created
echo "Removing empty log messages..."
find . -name "*.c" -type f -exec sed -i '/LOG_[A-Z]*("");/d' {} \;

echo "Logging cleanup complete!"