#!/bin/bash

echo "=== Connection Lifecycle Analysis ==="
echo

LOG_FILE="/opt/jsondb/build/var/jsondb.log"

echo "1. Connection States:"
echo "===================="
grep -E "(TRACE_HANDLER_START|TRACE_HANDLER_COMPLETE|TRACE_HANDLER_EXIT|TRACE_HANDLER_CONTINUE)" "$LOG_FILE" | tail -50 | \
    awk '{print $1, $2, $5, $6, substr($0, index($0,$7))}' | \
    sed 's/handle_client_thread_safe_internal\.server_thread_safe//' | \
    sed 's/TRACE_HANDLER_//'

echo -e "\n2. Keep-alive Analysis:"
echo "======================="
echo "Keep-alive starts (is_keepalive_continuation=1):"
grep "keepalive=1" "$LOG_FILE" | wc -l
echo "Non-keepalive starts (is_keepalive_continuation=0):"
grep "keepalive=0" "$LOG_FILE" | wc -l

echo -e "\n3. Connection Metrics:"
echo "====================="
echo "Increments: $(grep -c 'Incremented active connections' "$LOG_FILE")"
echo "Decrements: $(grep -c 'Decremented active connections' "$LOG_FILE")"
echo "Difference: $(($(grep -c 'Incremented active connections' "$LOG_FILE") - $(grep -c 'Decremented active connections' "$LOG_FILE")))"

echo -e "\n4. Connection Closing Patterns:"
echo "==============================="
echo "Normal closes (Connection: close):"
grep -B5 "connection closing normally" "$LOG_FILE" | grep -c "should_keep_alive"
echo "Timeout closes (5 second timeout):"
grep -c "5004\." "$LOG_FILE"
echo "Error closes:"
grep -c "TRACE_HANDLER_EXIT.*due to" "$LOG_FILE"

echo -e "\n5. Recent Connection Flow (last 20 connections):"
echo "================================================"
grep -E "(TRACE_HANDLER_START.*keepalive=0|Incremented active|Decremented active|TRACE_HANDLER_EXIT|CLOSING state)" "$LOG_FILE" | \
    tail -40 | \
    awk '{
        if ($0 ~ /keepalive=0/) print "NEW CONNECTION: " $1 " " $2 " " $5;
        else if ($0 ~ /Incremented/) print "  [+] INCREMENT";
        else if ($0 ~ /Decremented/) print "  [-] DECREMENT";
        else if ($0 ~ /TRACE_HANDLER_EXIT/) print "  EXIT: " substr($0, index($0,"due to"));
        else if ($0 ~ /CLOSING state/) print "  CLOSING";
        else print "  " $0;
    }'

echo -e "\n6. Connection ID Tracking:"
echo "=========================="
echo "Unique connection IDs in last 1000 lines:"
tail -1000 "$LOG_FILE" | grep -oE "connection [0-9]+" | sort -u | wc -l