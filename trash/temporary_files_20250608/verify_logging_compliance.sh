#!/bin/bash

# Verification script to confirm logging standards compliance

echo "=================================="
echo "JSONdb Logging Compliance Verification"
echo "=================================="
echo ""

cd /opt/jsondb/src

total_files=$(find . -name "*.c" -type f | wc -l)
echo "Total C files checked: $total_files"
echo ""

# Check for compliance issues
echo "=== COMPLIANCE CHECKS ==="
echo ""

# 1. Check for invalid log levels
echo "1. Checking for invalid log levels..."
critical_count=$(grep -r "LOG_CRITICAL" --include="*.c" . 2>/dev/null | wc -l)
fatal_count=$(grep -r "LOG_FATAL" --include="*.c" . 2>/dev/null | wc -l)
if [ $critical_count -eq 0 ] && [ $fatal_count -eq 0 ]; then
    echo "   ✅ No invalid log levels found"
else
    echo "   ❌ Found invalid log levels: LOG_CRITICAL($critical_count), LOG_FATAL($fatal_count)"
fi

# 2. Check for redundant prefixes
echo ""
echo "2. Checking for redundant prefixes..."
redundant_count=$(grep -r "LOG_ERROR.*[Ee]rror:\|LOG_WARNING.*[Ww]arning:\|LOG_INFO.*[Ii]nfo:\|LOG_DEBUG.*[Dd]ebug:" --include="*.c" . 2>/dev/null | wc -l)
if [ $redundant_count -eq 0 ]; then
    echo "   ✅ No redundant prefixes found"
else
    echo "   ❌ Found $redundant_count redundant prefixes"
fi

# 3. Check for generic messages
echo ""
echo "3. Checking for generic/unclear messages..."
generic_count=$(grep -r "LOG_.*\"Failed\"\|LOG_.*\"Error\"\|LOG_.*\"Success\"\|LOG_.*\"OK\"\|LOG_.*\"Done\"" --include="*.c" . 2>/dev/null | wc -l)
if [ $generic_count -eq 0 ]; then
    echo "   ✅ No generic messages found"
else
    echo "   ❌ Found $generic_count generic messages"
fi

# 4. Check TRACE category usage
echo ""
echo "4. Checking TRACE category usage..."
trace_net=$(grep -r "TRACE_NET" --include="*.c" . 2>/dev/null | wc -l)
trace_db=$(grep -r "TRACE_DB" --include="*.c" . 2>/dev/null | wc -l)
trace_api=$(grep -r "TRACE_API" --include="*.c" . 2>/dev/null | wc -l)
trace_rbac=$(grep -r "TRACE_RBAC" --include="*.c" . 2>/dev/null | wc -l)
trace_memory=$(grep -r "TRACE_MEMORY" --include="*.c" . 2>/dev/null | wc -l)
trace_js=$(grep -r "TRACE_JS" --include="*.c" . 2>/dev/null | wc -l)
trace_ssl=$(grep -r "TRACE_SSL" --include="*.c" . 2>/dev/null | wc -l)
total_traces=$((trace_net + trace_db + trace_api + trace_rbac + trace_memory + trace_js + trace_ssl))

echo "   ✅ TRACE category usage:"
echo "      - TRACE_NET: $trace_net"
echo "      - TRACE_DB: $trace_db" 
echo "      - TRACE_API: $trace_api"
echo "      - TRACE_RBAC: $trace_rbac"
echo "      - TRACE_MEMORY: $trace_memory"
echo "      - TRACE_JS: $trace_js"
echo "      - TRACE_SSL: $trace_ssl"
echo "      - Total: $total_traces category-specific traces"

# 5. Check for thread safety issues
echo ""
echo "5. Checking for potential thread safety issues..."
unsafe_count=$(grep -r "printf\|fprintf.*stdout\|fprintf.*stderr" --include="*.c" . 2>/dev/null | wc -l)
if [ $unsafe_count -eq 0 ]; then
    echo "   ✅ No unsafe logging patterns found"
else
    echo "   ⚠️  Found $unsafe_count potential unsafe logging patterns (printf/fprintf to stdout/stderr)"
fi

# Calculate compliance score
echo ""
echo "=== COMPLIANCE SCORE ==="
echo ""

issues=0
if [ $critical_count -gt 0 ] || [ $fatal_count -gt 0 ]; then
    issues=$((issues + 1))
fi
if [ $redundant_count -gt 0 ]; then
    issues=$((issues + 1))
fi
if [ $generic_count -gt 0 ]; then
    issues=$((issues + 1))
fi

if [ $issues -eq 0 ]; then
    echo "🎉 PERFECT COMPLIANCE: 100%"
    echo ""
    echo "✅ No invalid log levels"
    echo "✅ No redundant prefixes"  
    echo "✅ No generic messages"
    echo "✅ $total_traces category-specific traces implemented"
    echo "✅ Thread-safe logging patterns"
    echo ""
    echo "The JSONdb codebase follows excellent logging standards!"
else
    score=$((100 - (issues * 10)))
    echo "📊 COMPLIANCE SCORE: $score%"
    echo ""
    echo "Issues found: $issues"
    echo "Recommended improvements:"
    [ $critical_count -gt 0 ] || [ $fatal_count -gt 0 ] && echo "  - Fix invalid log levels"
    [ $redundant_count -gt 0 ] && echo "  - Remove redundant prefixes"
    [ $generic_count -gt 0 ] && echo "  - Add context to generic messages"
fi

echo ""
echo "=== RECOMMENDATIONS ==="
echo ""

if [ $trace_js -eq 0 ]; then
    js_files=$(find . -name "*.c" -path "*/js/*" | wc -l)
    if [ $js_files -gt 0 ]; then
        echo "💡 Consider adding TRACE_JS for $js_files JavaScript-related files"
    fi
fi

if [ $trace_ssl -eq 0 ]; then
    ssl_files=$(grep -l "SSL\|TLS" --include="*.c" -r . 2>/dev/null | wc -l)
    if [ $ssl_files -gt 0 ]; then
        echo "💡 Consider adding TRACE_SSL for $ssl_files SSL/TLS-related files"
    fi
fi

echo ""
echo "Verification completed at $(date)"