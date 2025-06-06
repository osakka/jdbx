#!/bin/bash

# UUID Migration Updater Script
# This script provides interactive tools to help update code for UUID migration

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# Function to show a code snippet with line numbers
show_context() {
    local file="$1"
    local line="$2"
    local context="${3:-5}"  # Default 5 lines of context
    
    echo -e "${CYAN}File: $file${NC}"
    echo -e "${CYAN}Line: $line${NC}"
    echo
    
    # Calculate line range
    local start=$((line - context))
    local end=$((line + context))
    
    # Ensure start is at least 1
    [ $start -lt 1 ] && start=1
    
    # Show the code with line numbers, highlighting the target line
    sed -n "${start},${end}p" "$file" | nl -v $start | while IFS= read -r codeline; do
        linenum=$(echo "$codeline" | awk '{print $1}')
        if [ "$linenum" -eq "$line" ]; then
            echo -e "${YELLOW}$codeline${NC}"
        else
            echo "$codeline"
        fi
    done
}

# Function to suggest replacement based on pattern
suggest_replacement() {
    local file="$1"
    local line="$2"
    local pattern="$3"
    
    echo -e "\n${GREEN}Suggested replacement:${NC}"
    
    case "$pattern" in
        "READ")
            cat << 'EOF'
// Instead of:
json_t *id = json_object_get(doc, "_id");

// Use:
json_t *id = json_object_get(doc, "uuid");
if (!id) {
    id = json_object_get(doc, "_id");  // Backward compatibility
}
EOF
            ;;
        "WRITE")
            cat << 'EOF'
// Instead of:
json_object_set(doc, "_id", json_string(id_value));

// Use:
json_object_set(doc, "uuid", json_string(id_value));
json_object_set(doc, "_id", json_string(id_value));  // Backward compatibility
EOF
            ;;
        "QUERY")
            cat << 'EOF'
// Instead of:
if (strcmp(key, "_id") == 0) { ... }

// Use:
if (strcmp(key, "uuid") == 0 || strcmp(key, "_id") == 0) { ... }
EOF
            ;;
        "JS_READ")
            cat << 'EOF'
// Instead of:
const id = doc._id;

// Use:
const id = doc.uuid || doc._id;  // Prefer uuid, fallback to _id
EOF
            ;;
        "JS_WRITE")
            cat << 'EOF'
// Instead of:
doc._id = generateId();

// Use:
const id = generateId();
doc.uuid = id;
doc._id = id;  // Backward compatibility
EOF
            ;;
    esac
}

# Function to analyze a specific file interactively
analyze_file_interactive() {
    local file="$1"
    
    echo -e "\n${BLUE}=== Analyzing $file ===${NC}\n"
    
    # Find all _id occurrences
    grep -n '"_id"' "$file" 2>/dev/null | while IFS=: read -r linenum content; do
        echo -e "\n${YELLOW}Found '_id' at line $linenum${NC}"
        show_context "$file" "$linenum" 5
        
        # Try to determine the pattern type
        local pattern_type="UNKNOWN"
        
        if echo "$content" | grep -q "json_object_get.*_id"; then
            pattern_type="READ"
        elif echo "$content" | grep -q "json_object_set.*_id"; then
            pattern_type="WRITE"
        elif echo "$content" | grep -q "strcmp.*_id"; then
            pattern_type="QUERY"
        fi
        
        echo -e "\n${BLUE}Pattern type: $pattern_type${NC}"
        suggest_replacement "$file" "$linenum" "$pattern_type"
        
        echo -e "\n${CYAN}Press Enter to continue to next occurrence...${NC}"
        read -r
    done
}

# Function to create helper functions file
create_helper_functions() {
    local helpers_file="/opt/jsondb/src/components/utils/uuid_helpers.c"
    local helpers_header="/opt/jsondb/src/include/utils/uuid_helpers.h"
    
    echo -e "${GREEN}Creating UUID helper functions...${NC}"
    
    # Create the C implementation
    cat > "$helpers_file" << 'EOF'
#include "utils/uuid_helpers.h"
#include "utils/json.h"
#include <string.h>

/**
 * Get document ID, checking uuid first, then _id for backward compatibility
 */
const char* get_document_id(json_t *doc) {
    if (!doc || !json_is_object(doc)) {
        return NULL;
    }
    
    // Try uuid first (new field)
    json_t *id = json_object_get(doc, "uuid");
    if (id && json_is_string(id)) {
        return json_string_value(id);
    }
    
    // Fallback to _id (backward compatibility)
    id = json_object_get(doc, "_id");
    if (id && json_is_string(id)) {
        return json_string_value(id);
    }
    
    return NULL;
}

/**
 * Get document ID as JSON object
 */
json_t* get_document_id_json(json_t *doc) {
    if (!doc || !json_is_object(doc)) {
        return NULL;
    }
    
    // Try uuid first (new field)
    json_t *id = json_object_get(doc, "uuid");
    if (id) {
        return id;
    }
    
    // Fallback to _id (backward compatibility)
    return json_object_get(doc, "_id");
}

/**
 * Set document ID, writing both uuid and _id for compatibility
 */
int set_document_id(json_t *doc, const char *id_value) {
    if (!doc || !json_is_object(doc) || !id_value) {
        return -1;
    }
    
    json_t *id_json = json_string(id_value);
    if (!id_json) {
        return -1;
    }
    
    // Set both fields for backward compatibility
    int result = 0;
    if (json_object_set(doc, "uuid", id_json) != 0) {
        result = -1;
    }
    if (json_object_set(doc, "_id", id_json) != 0) {
        result = -1;
    }
    
    json_decref(id_json);
    return result;
}

/**
 * Check if a field name is a document ID field
 */
bool is_document_id_field(const char *field_name) {
    if (!field_name) {
        return false;
    }
    return (strcmp(field_name, "uuid") == 0 || strcmp(field_name, "_id") == 0);
}

/**
 * Remove document ID fields (both uuid and _id)
 */
int remove_document_id(json_t *doc) {
    if (!doc || !json_is_object(doc)) {
        return -1;
    }
    
    int result = 0;
    if (json_object_del(doc, "uuid") != 0) {
        result = -1;
    }
    if (json_object_del(doc, "_id") != 0) {
        result = -1;
    }
    
    return result;
}

/**
 * Copy document ID from source to destination
 */
int copy_document_id(json_t *dest, json_t *src) {
    const char *id = get_document_id(src);
    if (!id) {
        return -1;
    }
    return set_document_id(dest, id);
}

/**
 * Migrate _id to uuid if only _id exists
 */
int migrate_id_to_uuid(json_t *doc) {
    if (!doc || !json_is_object(doc)) {
        return -1;
    }
    
    // Check if uuid already exists
    if (json_object_get(doc, "uuid")) {
        return 0;  // Already migrated
    }
    
    // Check if _id exists
    json_t *id = json_object_get(doc, "_id");
    if (!id || !json_is_string(id)) {
        return -1;  // No ID to migrate
    }
    
    // Copy _id value to uuid
    return json_object_set(doc, "uuid", id);
}
EOF

    # Create the header file
    cat > "$helpers_header" << 'EOF'
#ifndef UUID_HELPERS_H
#define UUID_HELPERS_H

#include "utils/json.h"
#include <stdbool.h>

/**
 * UUID Migration Helper Functions
 * 
 * These functions provide a compatibility layer for migrating from "_id" to "uuid"
 * while maintaining backward compatibility.
 */

/**
 * Get document ID, checking uuid first, then _id for backward compatibility
 * @param doc The JSON document
 * @return The ID string value, or NULL if not found
 */
const char* get_document_id(json_t *doc);

/**
 * Get document ID as JSON object
 * @param doc The JSON document
 * @return The ID JSON object, or NULL if not found
 */
json_t* get_document_id_json(json_t *doc);

/**
 * Set document ID, writing both uuid and _id for compatibility
 * @param doc The JSON document
 * @param id_value The ID string value to set
 * @return 0 on success, -1 on error
 */
int set_document_id(json_t *doc, const char *id_value);

/**
 * Check if a field name is a document ID field (uuid or _id)
 * @param field_name The field name to check
 * @return true if the field is a document ID field
 */
bool is_document_id_field(const char *field_name);

/**
 * Remove document ID fields (both uuid and _id)
 * @param doc The JSON document
 * @return 0 on success, -1 on error
 */
int remove_document_id(json_t *doc);

/**
 * Copy document ID from source to destination
 * @param dest The destination document
 * @param src The source document
 * @return 0 on success, -1 on error
 */
int copy_document_id(json_t *dest, json_t *src);

/**
 * Migrate _id to uuid if only _id exists
 * @param doc The JSON document to migrate
 * @return 0 on success or if already migrated, -1 on error
 */
int migrate_id_to_uuid(json_t *doc);

#endif /* UUID_HELPERS_H */
EOF

    echo -e "${GREEN}Helper functions created:${NC}"
    echo "  - $helpers_file"
    echo "  - $helpers_header"
}

# Function to show migration statistics
show_migration_stats() {
    echo -e "\n${BLUE}=== Migration Statistics ===${NC}\n"
    
    # Count occurrences in different file types
    echo "C source files with '_id':"
    find /opt/jsondb/src -name "*.c" -type f -exec grep -l '"_id"' {} \; 2>/dev/null | wc -l
    
    echo "Header files with '_id':"
    find /opt/jsondb/src/include -name "*.h" -type f -exec grep -l '"_id"' {} \; 2>/dev/null | wc -l
    
    echo "JavaScript files with '_id':"
    find /opt/jsondb/share -name "*.js" -type f -exec grep -l '_id' {} \; 2>/dev/null | wc -l
    
    echo "Test files with '_id':"
    find /opt/jsondb/tests -name "*.c" -type f -exec grep -l '"_id"' {} \; 2>/dev/null | wc -l
    
    echo -e "\n${YELLOW}Top 10 files with most '_id' occurrences:${NC}"
    find /opt/jsondb -name "*.c" -o -name "*.h" -o -name "*.js" | \
        xargs grep -c '"_id"\|_id' 2>/dev/null | \
        sort -t: -k2 -nr | \
        head -10
}

# Main menu
main_menu() {
    while true; do
        echo -e "\n${BLUE}=== UUID Migration Helper ===${NC}"
        echo "1. Run full analysis (creates analysis files)"
        echo "2. Show migration statistics"
        echo "3. Create helper functions"
        echo "4. Analyze specific file interactively"
        echo "5. Show migration templates"
        echo "6. Exit"
        echo
        read -rp "Select option: " choice
        
        case $choice in
            1)
                /opt/jsondb/scripts/uuid_migration_helper.sh
                ;;
            2)
                show_migration_stats
                ;;
            3)
                create_helper_functions
                ;;
            4)
                read -rp "Enter file path: " filepath
                if [ -f "$filepath" ]; then
                    analyze_file_interactive "$filepath"
                else
                    echo -e "${RED}File not found: $filepath${NC}"
                fi
                ;;
            5)
                if [ -f "/opt/jsondb/uuid_migration_analysis/migration_templates.txt" ]; then
                    cat "/opt/jsondb/uuid_migration_analysis/migration_templates.txt"
                else
                    echo -e "${YELLOW}Run option 1 first to generate templates${NC}"
                fi
                ;;
            6)
                echo "Exiting..."
                exit 0
                ;;
            *)
                echo -e "${RED}Invalid option${NC}"
                ;;
        esac
    done
}

# Run main menu
main_menu