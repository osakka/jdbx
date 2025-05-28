#!/bin/bash
# Script to standardize logging messages across the codebase

cd /opt/jsondb/src

echo "=== JSONdb Logging Standardization ==="
echo ""

# Common issues to fix:
# 1. Messages starting with lowercase
# 2. Messages ending with unnecessary periods
# 3. Inconsistent error message formats
# 4. Missing context in error messages

# Function to process files
process_file() {
    local file=$1
    local temp_file="${file}.tmp"
    local changes=0
    
    # Create backup
    cp "$file" "${file}.bak"
    
    # Fix common patterns
    sed -E \
        -e 's/LOG_ERROR\("failed/LOG_ERROR\("Failed/g' \
        -e 's/LOG_WARNING\("warning/LOG_WARNING\("Warning/g' \
        -e 's/LOG_INFO\("initializing/LOG_INFO\("Initializing/g' \
        -e 's/LOG_INFO\("starting/LOG_INFO\("Starting/g' \
        -e 's/LOG_INFO\("stopping/LOG_INFO\("Stopping/g' \
        -e 's/LOG_DEBUG\("entering/LOG_DEBUG\("Entering/g' \
        -e 's/LOG_DEBUG\("exiting/LOG_DEBUG\("Exiting/g' \
        -e 's/LOG_ERROR\("error/LOG_ERROR\("Error/g' \
        -e 's/\.\\"\\)/\\")/' \
        -e 's/LOG_INFO\("server/LOG_INFO\("Server/g' \
        -e 's/LOG_INFO\("database/LOG_INFO\("Database/g' \
        -e 's/LOG_ERROR\("invalid/LOG_ERROR\("Invalid/g' \
        -e 's/LOG_WARNING\("cannot/LOG_WARNING\("Cannot/g' \
        "$file" > "$temp_file"
    
    # Check if changes were made
    if ! cmp -s "$file" "$temp_file"; then
        mv "$temp_file" "$file"
        ((changes++))
        echo "Updated: $file"
    else
        rm "$temp_file"
        rm "${file}.bak"
    fi
    
    return $changes
}

# Find all C files
total_changes=0
for file in $(find . -name "*.c" -type f); do
    process_file "$file"
    total_changes=$((total_changes + $?))
done

echo ""
echo "Standardization complete. Files updated: $total_changes"

# Additional checks for manual review
echo ""
echo "=== Messages requiring manual review ==="
echo ""

# Find potentially problematic patterns
echo "1. Very long log messages (>100 chars):"
grep -r "LOG_" --include="*.c" | grep -E "LOG_[A-Z]+\(.{100,}" | head -10

echo ""
echo "2. Log messages with multiple format specifiers (complex):"
grep -r "LOG_" --include="*.c" | grep -E "%[^\"]*%[^\"]*%[^\"]*%" | head -10

echo ""
echo "3. Error messages without context:"
grep -r "LOG_ERROR" --include="*.c" | grep -v ":" | grep -v "failed" | grep -v "Failed" | head -10

echo ""
echo "Remember to:"
echo "- Review the LOGGING_STANDARDS.md for guidelines"
echo "- Ensure error messages are actionable"
echo "- Include relevant context in all messages"
echo "- Use consistent terminology"