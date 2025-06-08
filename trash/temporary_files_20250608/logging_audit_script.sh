#!/bin/bash

# JSONdb Logging Standards Audit Script
# Analyzes all C source files for logging compliance

echo "=================================="
echo "JSONdb Logging Standards Audit"
echo "=================================="
echo ""

cd /opt/jsondb/src

# Count total files
total_files=$(find . -name "*.c" -type f | wc -l)
echo "Total C files to audit: $total_files"
echo ""

# Initialize counters
files_with_issues=0
error_issues=0
warning_issues=0
redundant_prefix_issues=0
missing_trace_issues=0
generic_message_issues=0

# Create temporary files for detailed analysis
issues_file=$(mktemp)
redundant_prefixes_file=$(mktemp)
generic_messages_file=$(mktemp)
trace_analysis_file=$(mktemp)

echo "=== ISSUE ANALYSIS ==="
echo ""

echo "1. CHECKING FOR INVALID LOG LEVELS..."
# Check for LOG_CRITICAL usage (should be LOG_ERROR)
critical_count=$(grep -r "LOG_CRITICAL" --include="*.c" . | wc -l)
if [ $critical_count -gt 0 ]; then
    echo "   ❌ Found $critical_count instances of LOG_CRITICAL (should be LOG_ERROR)"
    grep -rn "LOG_CRITICAL" --include="*.c" . >> $issues_file
fi

# Check for LOG_FATAL usage (should be LOG_ERROR)
fatal_count=$(grep -r "LOG_FATAL" --include="*.c" . | wc -l)
if [ $fatal_count -gt 0 ]; then
    echo "   ❌ Found $fatal_count instances of LOG_FATAL (should be LOG_ERROR)"
    grep -rn "LOG_FATAL" --include="*.c" . >> $issues_file
fi

echo ""

echo "2. CHECKING FOR REDUNDANT PREFIXES..."
# Check for common redundant prefixes in log messages
grep -rn "LOG_ERROR.*[Ee]rror:" --include="*.c" . > $redundant_prefixes_file
redundant_error_count=$(cat $redundant_prefixes_file | wc -l)

grep -rn "LOG_WARNING.*[Ww]arning:" --include="*.c" . >> $redundant_prefixes_file
redundant_warning_count=$(grep -n "LOG_WARNING.*[Ww]arning:" $redundant_prefixes_file | wc -l)

grep -rn "LOG_INFO.*[Ii]nfo:" --include="*.c" . >> $redundant_prefixes_file
redundant_info_count=$(grep -n "LOG_INFO.*[Ii]nfo:" $redundant_prefixes_file | wc -l)

grep -rn "LOG_DEBUG.*[Dd]ebug:" --include="*.c" . >> $redundant_prefixes_file
redundant_debug_count=$(grep -n "LOG_DEBUG.*[Dd]ebug:" $redundant_prefixes_file | wc -l)

total_redundant=$(cat $redundant_prefixes_file | wc -l)
if [ $total_redundant -gt 0 ]; then
    echo "   ❌ Found $total_redundant instances of redundant prefixes"
    echo "      - ERROR with 'Error:' prefix: $redundant_error_count"
    echo "      - WARNING with 'Warning:' prefix: $redundant_warning_count"
    echo "      - INFO with 'Info:' prefix: $redundant_info_count"
    echo "      - DEBUG with 'Debug:' prefix: $redundant_debug_count"
    redundant_prefix_issues=$total_redundant
else
    echo "   ✅ No redundant prefixes found"
fi

echo ""

echo "3. CHECKING TRACE CATEGORY USAGE..."
# Check for TRACE_ category usage
trace_net=$(grep -r "TRACE_NET" --include="*.c" . | wc -l)
trace_db=$(grep -r "TRACE_DB" --include="*.c" . | wc -l)
trace_api=$(grep -r "TRACE_API" --include="*.c" . | wc -l)
trace_rbac=$(grep -r "TRACE_RBAC" --include="*.c" . | wc -l)
trace_memory=$(grep -r "TRACE_MEMORY" --include="*.c" . | wc -l)
trace_js=$(grep -r "TRACE_JS" --include="*.c" . | wc -l)
trace_ssl=$(grep -r "TRACE_SSL" --include="*.c" . | wc -l)

echo "   Current TRACE category usage:"
echo "      - TRACE_NET: $trace_net instances"
echo "      - TRACE_DB: $trace_db instances"
echo "      - TRACE_API: $trace_api instances"
echo "      - TRACE_RBAC: $trace_rbac instances"
echo "      - TRACE_MEMORY: $trace_memory instances"
echo "      - TRACE_JS: $trace_js instances"
echo "      - TRACE_SSL: $trace_ssl instances"

# Check for generic TRACE usage (should use categories)
generic_trace=$(grep -rn "TRACE(" --include="*.c" . | wc -l)
if [ $generic_trace -gt 0 ]; then
    echo "   ❌ Found $generic_trace instances of generic TRACE() (should use TRACE_* categories)"
    grep -rn "TRACE(" --include="*.c" . >> $trace_analysis_file
fi

echo ""

echo "4. CHECKING FOR GENERIC/UNCLEAR MESSAGES..."
# Check for generic messages that need more context
grep -rn "LOG_.*\"Failed\"" --include="*.c" . > $generic_messages_file
grep -rn "LOG_.*\"Error\"" --include="*.c" . >> $generic_messages_file
grep -rn "LOG_.*\"Success\"" --include="*.c" . >> $generic_messages_file
grep -rn "LOG_.*\"OK\"" --include="*.c" . >> $generic_messages_file
grep -rn "LOG_.*\"Done\"" --include="*.c" . >> $generic_messages_file

generic_count=$(cat $generic_messages_file | wc -l)
if [ $generic_count -gt 0 ]; then
    echo "   ❌ Found $generic_count instances of generic/unclear messages"
    generic_message_issues=$generic_count
else
    echo "   ✅ No generic messages found"
fi

echo ""

echo "5. ANALYZING FILES WITH MOST ISSUES..."
echo ""

# Find files with the most logging issues
declare -A file_issues
for file in $(find . -name "*.c" -type f); do
    issues=0
    
    # Count redundant prefixes
    redundant=$(grep "LOG_.*[Ee]rror:\|LOG_.*[Ww]arning:\|LOG_.*[Ii]nfo:\|LOG_.*[Dd]ebug:" "$file" 2>/dev/null | wc -l)
    issues=$((issues + redundant))
    
    # Count generic messages
    generic=$(grep "LOG_.*\"Failed\"\|LOG_.*\"Error\"\|LOG_.*\"Success\"\|LOG_.*\"OK\"\|LOG_.*\"Done\"" "$file" 2>/dev/null | wc -l)
    issues=$((issues + generic))
    
    # Count generic TRACE usage
    generic_trace_file=$(grep "TRACE(" "$file" 2>/dev/null | wc -l)
    issues=$((issues + generic_trace_file))
    
    if [ $issues -gt 0 ]; then
        file_issues["$file"]=$issues
        files_with_issues=$((files_with_issues + 1))
    fi
done

# Sort and display top 10 files with most issues
echo "Top 10 files with most logging issues:"
for file in "${!file_issues[@]}"; do
    echo "${file_issues[$file]} $file"
done | sort -nr | head -10

echo ""

# Calculate summary statistics
total_issues=$((redundant_prefix_issues + generic_message_issues + generic_trace))

echo "=== SUMMARY REPORT ==="
echo ""
echo "Files audited: $total_files"
echo "Files with issues: $files_with_issues"
echo "Total issues found: $total_issues"
echo ""
echo "Issue breakdown:"
echo "  - Redundant prefixes: $redundant_prefix_issues"
echo "  - Generic messages: $generic_message_issues"
echo "  - Generic TRACE usage: $generic_trace"
echo ""

if [ $total_issues -eq 0 ]; then
    echo "🎉 AUDIT RESULT: EXCELLENT - No major logging issues found!"
else
    echo "⚠️  AUDIT RESULT: Issues found that need attention"
    echo ""
    echo "RECOMMENDATIONS:"
    echo "1. Remove redundant prefixes from log messages"
    echo "2. Add more context to generic messages"
    echo "3. Use category-specific TRACE_* instead of generic TRACE()"
    echo "4. Follow format: LOG_LEVEL(\"Clear message with context: %s\", context);"
fi

# Cleanup
rm -f $issues_file $redundant_prefixes_file $generic_messages_file $trace_analysis_file

echo ""
echo "Audit completed at $(date)"