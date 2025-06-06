#!/bin/bash

# UUID Migration Auto Update Script
# This script can automatically update common patterns while preserving backward compatibility

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Backup directory
BACKUP_DIR="/opt/jsondb/uuid_migration_backup_$(date +%Y%m%d_%H%M%S)"

# Create backup
create_backup() {
    echo -e "${BLUE}Creating backup in $BACKUP_DIR${NC}"
    mkdir -p "$BACKUP_DIR"
    
    # Backup source files
    cp -r /opt/jsondb/src "$BACKUP_DIR/"
    cp -r /opt/jsondb/tests "$BACKUP_DIR/"
    cp -r /opt/jsondb/share "$BACKUP_DIR/"
    
    echo -e "${GREEN}Backup created successfully${NC}"
}

# Function to update simple read patterns in C files
update_c_read_patterns() {
    local file="$1"
    local temp_file="${file}.tmp"
    local changes=0
    
    echo -e "${CYAN}Checking C read patterns in: $file${NC}"
    
    # Create a script to process the file
    cat > /tmp/uuid_update_reads.awk << 'EOF'
# Match json_object_get with "_id"
/json_object_get\([^,]+,\s*"_id"\)/ {
    # Extract the variable being assigned to and the object being accessed
    if (match($0, /^(\s*)(.+)=\s*json_object_get\(([^,]+),\s*"_id"\)(.*)$/, parts)) {
        indent = parts[1]
        var_part = parts[2]
        obj_name = parts[3]
        rest = parts[4]
        
        # Generate the new code
        print indent var_part "= json_object_get(" obj_name ", \"uuid\")" rest
        print indent "if (!" var_part ") {"
        print indent "    " var_part "= json_object_get(" obj_name ", \"_id\");  // Backward compatibility"
        print indent "}"
        next
    }
}

# Default: print the line unchanged
{ print }
EOF
    
    # Apply the transformation
    awk -f /tmp/uuid_update_reads.awk "$file" > "$temp_file"
    
    # Check if changes were made
    if ! diff -q "$file" "$temp_file" > /dev/null 2>&1; then
        mv "$temp_file" "$file"
        ((changes++))
        echo -e "${GREEN}Updated $changes read patterns${NC}"
    else
        rm -f "$temp_file"
    fi
    
    rm -f /tmp/uuid_update_reads.awk
    return $changes
}

# Function to update simple write patterns in C files
update_c_write_patterns() {
    local file="$1"
    local temp_file="${file}.tmp"
    local changes=0
    
    echo -e "${CYAN}Checking C write patterns in: $file${NC}"
    
    # Process the file line by line
    while IFS= read -r line; do
        # Check for json_object_set pattern with _id
        if [[ "$line" =~ json_object_set\([^,]+,[[:space:]]*\"_id\",[[:space:]]*[^)]+\) ]]; then
            # Extract the parts
            if [[ "$line" =~ ^([[:space:]]*)(.*)json_object_set\(([^,]+),[[:space:]]*\"_id\",[[:space:]]*([^)]+)\)(.*)$ ]]; then
                indent="${BASH_REMATCH[1]}"
                prefix="${BASH_REMATCH[2]}"
                obj="${BASH_REMATCH[3]}"
                value="${BASH_REMATCH[4]}"
                suffix="${BASH_REMATCH[5]}"
                
                # Write both uuid and _id
                echo "${indent}${prefix}json_object_set(${obj}, \"uuid\", ${value})${suffix}"
                echo "${indent}json_object_set(${obj}, \"_id\", ${value});  // Backward compatibility"
                ((changes++))
            else
                echo "$line"
            fi
        else
            echo "$line"
        fi
    done < "$file" > "$temp_file"
    
    # Replace the file if changes were made
    if [ $changes -gt 0 ]; then
        mv "$temp_file" "$file"
        echo -e "${GREEN}Updated $changes write patterns${NC}"
    else
        rm -f "$temp_file"
    fi
    
    return $changes
}

# Function to update query patterns in C files
update_c_query_patterns() {
    local file="$1"
    local temp_file="${file}.tmp"
    local changes=0
    
    echo -e "${CYAN}Checking C query patterns in: $file${NC}"
    
    # Simple sed replacement for strcmp patterns
    sed -e 's/strcmp(\([^,]*\), "_id") == 0/(strcmp(\1, "uuid") == 0 || strcmp(\1, "_id") == 0)/g' \
        -e 's/!strcmp(\([^,]*\), "_id")/(!strcmp(\1, "uuid") || !strcmp(\1, "_id"))/g' \
        "$file" > "$temp_file"
    
    # Check if changes were made
    if ! diff -q "$file" "$temp_file" > /dev/null 2>&1; then
        mv "$temp_file" "$file"
        changes=1
        echo -e "${GREEN}Updated query patterns${NC}"
    else
        rm -f "$temp_file"
    fi
    
    return $changes
}

# Function to update JavaScript patterns
update_js_patterns() {
    local file="$1"
    local temp_file="${file}.tmp"
    local changes=0
    
    echo -e "${CYAN}Checking JavaScript patterns in: $file${NC}"
    
    # Create a more sophisticated JS update script
    cat > /tmp/uuid_update_js.js << 'EOF'
const fs = require('fs');
const content = fs.readFileSync(process.argv[2], 'utf8');

let updated = content;
let changeCount = 0;

// Pattern 1: Direct property access for reading
// doc._id -> (doc.uuid || doc._id)
updated = updated.replace(/(\w+)\._id(?![a-zA-Z0-9_])/g, (match, obj) => {
    changeCount++;
    return `(${obj}.uuid || ${obj}._id)`;
});

// Pattern 2: Bracket notation for reading
// doc["_id"] -> (doc.uuid || doc["_id"])
updated = updated.replace(/(\w+)\["_id"\]/g, (match, obj) => {
    changeCount++;
    return `(${obj}.uuid || ${obj}["_id"])`;
});

// Pattern 3: Object property in conditions
// if (doc._id) -> if (doc.uuid || doc._id)
updated = updated.replace(/if\s*\(\s*(\w+)\._id\s*\)/g, (match, obj) => {
    changeCount++;
    return `if (${obj}.uuid || ${obj}._id)`;
});

// Pattern 4: Assignment patterns - need both fields
// doc._id = value -> doc.uuid = doc._id = value
updated = updated.replace(/^(\s*)(\w+)\._id\s*=\s*(.+)$/gm, (match, indent, obj, value) => {
    changeCount++;
    return `${indent}${obj}.uuid = ${obj}._id = ${value}  // Maintain backward compatibility`;
});

console.log(changeCount);
fs.writeFileSync(process.argv[3], updated);
EOF
    
    # Note: This would require Node.js. For now, let's use sed patterns
    # Using sed for simpler patterns
    sed -e 's/\([a-zA-Z_][a-zA-Z0-9_]*\)\._id\([^a-zA-Z0-9_]\)/(\1.uuid || \1._id)\2/g' \
        -e 's/\([a-zA-Z_][a-zA-Z0-9_]*\)\["_id"\]/(\1.uuid || \1["_id"])/g' \
        "$file" > "$temp_file"
    
    # Check if changes were made
    if ! diff -q "$file" "$temp_file" > /dev/null 2>&1; then
        mv "$temp_file" "$file"
        changes=1
        echo -e "${GREEN}Updated JavaScript patterns${NC}"
    else
        rm -f "$temp_file"
    fi
    
    rm -f /tmp/uuid_update_js.js
    return $changes
}

# Function to process a single file
process_file() {
    local file="$1"
    local total_changes=0
    
    # Skip backup files and temp files
    if [[ "$file" == *.tmp || "$file" == *.bak ]]; then
        return 0
    fi
    
    # Determine file type and apply appropriate updates
    case "$file" in
        *.c)
            update_c_read_patterns "$file"
            total_changes=$((total_changes + $?))
            
            update_c_write_patterns "$file"
            total_changes=$((total_changes + $?))
            
            update_c_query_patterns "$file"
            total_changes=$((total_changes + $?))
            ;;
        *.js)
            update_js_patterns "$file"
            total_changes=$((total_changes + $?))
            ;;
    esac
    
    if [ $total_changes -gt 0 ]; then
        echo -e "${GREEN}Total changes in $file: $total_changes${NC}"
    fi
}

# Function to process specific components
process_component() {
    local component="$1"
    
    case "$component" in
        "database")
            echo -e "${BLUE}Processing database components...${NC}"
            process_file "/opt/jsondb/src/components/database/database.c"
            process_file "/opt/jsondb/src/components/database/operations.c"
            process_file "/opt/jsondb/src/components/database/batch_operations.c"
            process_file "/opt/jsondb/src/components/database/indexed_document_operations.c"
            ;;
        "api")
            echo -e "${BLUE}Processing API components...${NC}"
            process_file "/opt/jsondb/src/components/core/api.c"
            process_file "/opt/jsondb/src/components/api/session_api.c"
            process_file "/opt/jsondb/src/components/api/rbac_api.c"
            ;;
        "rbac")
            echo -e "${BLUE}Processing RBAC components...${NC}"
            process_file "/opt/jsondb/src/components/rbac/rbac_database.c"
            process_file "/opt/jsondb/src/components/rbac/rbac_sessions.c"
            process_file "/opt/jsondb/src/components/rbac/rbac_db.c"
            ;;
        "js")
            echo -e "${BLUE}Processing JavaScript files...${NC}"
            process_file "/opt/jsondb/share/htdocs/js/app.js"
            find /opt/jsondb/share/examples -name "*.js" -type f | while read -r jsfile; do
                process_file "$jsfile"
            done
            ;;
        "tests")
            echo -e "${BLUE}Processing test files...${NC}"
            find /opt/jsondb/tests -name "*.c" -type f | while read -r testfile; do
                if grep -q '"_id"' "$testfile" 2>/dev/null; then
                    process_file "$testfile"
                fi
            done
            ;;
        *)
            echo -e "${RED}Unknown component: $component${NC}"
            ;;
    esac
}

# Main function
main() {
    echo -e "${BLUE}=== UUID Migration Auto Update Tool ===${NC}"
    echo
    echo "This tool can automatically update common _id patterns to support"
    echo "both uuid and _id fields for backward compatibility."
    echo
    echo -e "${YELLOW}WARNING: This will modify files in place!${NC}"
    echo -e "${YELLOW}A backup will be created first.${NC}"
    echo
    
    read -rp "Do you want to proceed? (y/N): " confirm
    if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
        echo "Aborted."
        exit 0
    fi
    
    # Create backup
    create_backup
    
    echo
    echo "Select what to update:"
    echo "1. Database components only"
    echo "2. API components only"
    echo "3. RBAC components only"
    echo "4. JavaScript files only"
    echo "5. Test files only"
    echo "6. All components (recommended)"
    echo "7. Specific file"
    echo
    
    read -rp "Select option: " choice
    
    case $choice in
        1) process_component "database" ;;
        2) process_component "api" ;;
        3) process_component "rbac" ;;
        4) process_component "js" ;;
        5) process_component "tests" ;;
        6)
            process_component "database"
            process_component "api"
            process_component "rbac"
            process_component "js"
            process_component "tests"
            ;;
        7)
            read -rp "Enter file path: " filepath
            if [ -f "$filepath" ]; then
                process_file "$filepath"
            else
                echo -e "${RED}File not found: $filepath${NC}"
            fi
            ;;
        *)
            echo -e "${RED}Invalid option${NC}"
            exit 1
            ;;
    esac
    
    echo
    echo -e "${GREEN}Update complete!${NC}"
    echo -e "${YELLOW}Backup saved in: $BACKUP_DIR${NC}"
    echo
    echo "Next steps:"
    echo "1. Review the changes using 'git diff'"
    echo "2. Compile and test the changes"
    echo "3. Run the test suite"
    echo "4. If issues occur, restore from backup: cp -r $BACKUP_DIR/* /opt/jsondb/"
}

# Run main function
main "$@"