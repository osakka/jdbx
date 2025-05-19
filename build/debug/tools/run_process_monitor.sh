#!/bin/bash

LOG_FILE="/tmp/process_monitor_$(date +%Y%m%d_%H%M%S).log"

echo "Starting process monitor test. Log file: $LOG_FILE"
"$(dirname "$0")/process_monitor"

echo "Process monitor test complete. Check the log file: $LOG_FILE"
