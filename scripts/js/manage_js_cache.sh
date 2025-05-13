#!/bin/bash
#
# Utility script to manage JavaScript file path cache
#

# Set paths
CACHE_FILE="var/data/jsondb/js_path_cache.json"
PROJECT_ROOT=$(dirname $(dirname $(readlink -f "$0")))
FULL_CACHE_PATH="$PROJECT_ROOT/$CACHE_FILE"

# Ensure colors work
RESET="\033[0m"
RED="\033[0;31m"
GREEN="\033[0;32m"
YELLOW="\033[0;33m"
BLUE="\033[0;34m"

# Display help information
function show_help() {
    echo -e "${BLUE}JavaScript Path Cache Management Utility${RESET}"
    echo "--------------------------------------"
    echo ""
    echo "This script helps manage the JavaScript file path cache."
    echo ""
    echo "Usage:"
    echo "  $0 [command]"
    echo ""
    echo "Commands:"
    echo "  status    Show current cache statistics"
    echo "  list      List all cached paths"
    echo "  clear     Delete the cache file"
    echo "  help      Show this help message"
    echo ""
    echo "Cache location: $FULL_CACHE_PATH"
    echo ""
}

# Show cache status
function show_status() {
    if [ ! -f "$FULL_CACHE_PATH" ]; then
        echo -e "${YELLOW}Cache file does not exist.${RESET}"
        return
    fi

    echo -e "${BLUE}JavaScript Path Cache Status:${RESET}"
    echo "------------------------------"
    echo "Location: $FULL_CACHE_PATH"
    
    # Get statistics
    ENTRY_COUNT=$(wc -l < "$FULL_CACHE_PATH")
    FILE_SIZE=$(du -h "$FULL_CACHE_PATH" | cut -f1)
    LAST_MODIFIED=$(stat -c "%y" "$FULL_CACHE_PATH")
    
    echo "Entries: $ENTRY_COUNT"
    echo "Size: $FILE_SIZE"
    echo "Last Modified: $LAST_MODIFIED"
    echo ""
}

# List all cached paths
function list_cache() {
    if [ ! -f "$FULL_CACHE_PATH" ]; then
        echo -e "${YELLOW}Cache file does not exist.${RESET}"
        return
    fi

    echo -e "${BLUE}JavaScript Path Cache Entries:${RESET}"
    echo "------------------------------"
    
    # Create temp file for formatted output
    TEMP_FILE=$(mktemp)
    
    # Format entries for better display
    echo -e "ORIGINAL PATH | RESOLVED PATH | TIMESTAMP" > "$TEMP_FILE"
    echo -e "------------- | ------------- | ---------" >> "$TEMP_FILE"
    
    while IFS="|" read -r orig resolved timestamp; do
        # Convert Unix timestamp to human-readable date
        if [[ "$timestamp" =~ ^[0-9]+$ ]]; then
            date_str=$(date -d "@$timestamp" "+%Y-%m-%d %H:%M:%S" 2>/dev/null)
            if [ $? -ne 0 ]; then
                # Fallback for BSD date on macOS
                date_str=$(date -r "$timestamp" "+%Y-%m-%d %H:%M:%S" 2>/dev/null || echo "$timestamp")
            fi
        else
            date_str="$timestamp"
        fi
        
        echo -e "$orig | $resolved | $date_str" >> "$TEMP_FILE"
    done < "$FULL_CACHE_PATH"
    
    # Use column for nice formatting
    column -t -s "|" "$TEMP_FILE" | less -S
    
    # Clean up
    rm "$TEMP_FILE"
}

# Clear the cache
function clear_cache() {
    if [ ! -f "$FULL_CACHE_PATH" ]; then
        echo -e "${YELLOW}Cache file does not exist.${RESET}"
        return
    fi

    echo -n "Are you sure you want to clear the JavaScript path cache? [y/N] "
    read -r answer
    
    if [[ "$answer" =~ ^[Yy]$ ]]; then
        rm "$FULL_CACHE_PATH"
        echo -e "${GREEN}Cache file has been removed.${RESET}"
    else
        echo "Operation canceled."
    fi
}

# Main command processing
case "$1" in
    status)
        show_status
        ;;
    list)
        list_cache
        ;;
    clear)
        clear_cache
        ;;
    help|"")
        show_help
        ;;
    *)
        echo -e "${RED}Unknown command: $1${RESET}"
        echo ""
        show_help
        exit 1
        ;;
esac

exit 0