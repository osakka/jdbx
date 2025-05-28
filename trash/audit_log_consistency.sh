#!/bin/bash

# Script to audit log message consistency and identify patterns that need fixing

echo "=== Log Message Consistency Audit ==="
echo

cd /opt/jsondb/src

# Check for overly verbose INFO messages (should be concise)
echo "=== Checking for verbose INFO messages ==="
grep -n "LOG_INFO.*successfully.*" components/**/*.c | head -10
grep -n "LOG_INFO.*Starting.*" components/**/*.c | head -10
grep -n "LOG_INFO.*Processing.*" components/**/*.c | head -10
echo

# Check for redundant prefixes in ERROR messages
echo "=== Checking for redundant ERROR prefixes ==="
grep -n 'LOG_ERROR.*"Error:' components/**/*.c | head -10
grep -n 'LOG_ERROR.*"Failed to' components/**/*.c | head -10
echo

# Check for messages that include function names (redundant)
echo "=== Checking for redundant function references ==="
grep -n "in function" components/**/*.c | head -10
grep -n "() function" components/**/*.c | head -10
echo

# Check for inconsistent memory error messages
echo "=== Checking for inconsistent memory errors ==="
grep -n "allocation failed" components/**/*.c | head -10
grep -n "malloc failed" components/**/*.c | head -10
grep -n "Out of memory" components/**/*.c | head -10
echo

# Check for overly verbose DEBUG messages
echo "=== Checking for verbose DEBUG messages ==="
grep -n "LOG_DEBUG.*Starting to" components/**/*.c | head -10
grep -n "LOG_DEBUG.*Attempting to" components/**/*.c | head -10
echo

# Check for messages with unnecessary context
echo "=== Checking for redundant context ==="
grep -n "for collection" components/**/*.c | head -10
grep -n "in collection" components/**/*.c | head -10
echo

# Count different log levels to ensure proper usage
echo "=== Log Level Distribution ==="
echo -n "ERROR: "; grep -c "LOG_ERROR" components/**/*.c | awk '{sum+=$1} END {print sum}'
echo -n "WARNING: "; grep -c "LOG_WARNING" components/**/*.c | awk '{sum+=$1} END {print sum}'
echo -n "INFO: "; grep -c "LOG_INFO" components/**/*.c | awk '{sum+=$1} END {print sum}'
echo -n "DEBUG: "; grep -c "LOG_DEBUG" components/**/*.c | awk '{sum+=$1} END {print sum}'
echo -n "TRACE: "; grep -c "LOG_TRACE" components/**/*.c | awk '{sum+=$1} END {print sum}'
echo

# Find messages longer than 80 characters (too verbose)
echo "=== Messages longer than 80 characters ==="
grep -n 'LOG_[A-Z]*("[^"]\{80,\}"' components/**/*.c | head -10
echo

# Check for consistent capitalization
echo "=== Checking capitalization consistency ==="
echo "Lowercase starts:"
grep -n 'LOG_[A-Z]*("[a-z]' components/**/*.c | head -10
echo "Uppercase starts:"
grep -n 'LOG_[A-Z]*("[A-Z]' components/**/*.c | head -10
echo

echo "=== Audit complete ==="