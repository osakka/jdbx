#!/bin/bash

LOG_FILE="/tmp/thread_monitor_$(date +%Y%m%d_%H%M%S).log"

echo "Starting thread monitor test. Log file: $LOG_FILE"
"$(dirname "$0")/thread_monitor"

echo "Thread monitor test complete. Check the log file: $LOG_FILE"
