#!/bin/bash

# UUID Migration Helper Script
# This script helps systematically update all "_id" references to support both "uuid" and "_id"
# for backward compatibility in the JSONdb codebase

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Output directory for analysis results
OUTPUT_DIR="/opt/jsondb/uuid_migration_analysis"
mkdir -p "$OUTPUT_DIR"

# Function to print colored headers
print_header() {
    echo -e "\n${BLUE}=== $1 ===${NC}\n"
}

# Function to analyze a file and categorize _id usage
analyze_file() {
    local file="$1"
    local filename=$(basename "$file")
    local analysis_file="$OUTPUT_DIR/${filename}.analysis"
    
    echo -e "${CYAN}Analyzing: $file${NC}"
    echo "File: $file" > "$analysis_file"
    echo "=" >> "$analysis_file"
    echo >> "$analysis_file"
    
    # Find all _id references with context
    grep -n -B2 -A2 '"_id"' "$file" 2>/dev/null | while IFS= read -r line; do
        echo "$line" >> "$analysis_file"
    done
    
    # Categorize patterns
    echo >> "$analysis_file"
    echo "PATTERN ANALYSIS:" >> "$analysis_file"
    echo "=================" >> "$analysis_file"
    
    # READ patterns (accessing _id from JSON)
    echo >> "$analysis_file"
    echo "READ PATTERNS (accessing _id):" >> "$analysis_file"
    grep -n 'json_object_get.*"_id"' "$file" 2>/dev/null >> "$analysis_file" || true
    grep -n 'doc\["_id"\]' "$file" 2>/dev/null >> "$analysis_file" || true
    grep -n '\.\_id' "$file" 2>/dev/null >> "$analysis_file" || true
    
    # WRITE patterns (setting _id in JSON)
    echo >> "$analysis_file"
    echo "WRITE PATTERNS (setting _id):" >> "$analysis_file"
    grep -n 'json_object_set.*"_id"' "$file" 2>/dev/null >> "$analysis_file" || true
    grep -n '"_id".*:' "$file" 2>/dev/null >> "$analysis_file" || true
    
    # QUERY patterns (searching by _id)
    echo >> "$analysis_file"
    echo "QUERY PATTERNS (searching by _id):" >> "$analysis_file"
    grep -n 'strcmp.*"_id"' "$file" 2>/dev/null >> "$analysis_file" || true
    grep -n 'query.*_id' "$file" 2>/dev/null >> "$analysis_file" || true
    grep -n 'find.*_id' "$file" 2>/dev/null >> "$analysis_file" || true
    
    # SCHEMA patterns (defining _id fields)
    echo >> "$analysis_file"
    echo "SCHEMA PATTERNS (defining _id):" >> "$analysis_file"
    grep -n -B1 -A1 '"_id".*"type"' "$file" 2>/dev/null >> "$analysis_file" || true
    
    return 0
}

# Function to generate migration templates
generate_migration_templates() {
    local templates_file="$OUTPUT_DIR/migration_templates.txt"
    
    cat > "$templates_file" << 'EOF'
UUID MIGRATION TEMPLATES
========================

1. READ PATTERN (accessing document ID):
   OLD: json_object_get(doc, "_id")
   NEW: 
   ```c
   json_t *id = json_object_get(doc, "uuid");
   if (!id) {
       id = json_object_get(doc, "_id");  // Fallback for backward compatibility
   }
   ```

2. WRITE PATTERN (setting document ID):
   OLD: json_object_set(doc, "_id", json_string(id_value))
   NEW:
   ```c
   // Set both fields for backward compatibility
   json_object_set(doc, "uuid", json_string(id_value));
   json_object_set(doc, "_id", json_string(id_value));
   ```

3. QUERY PATTERN (searching by ID):
   OLD: strcmp(key, "_id") == 0
   NEW:
   ```c
   if (strcmp(key, "uuid") == 0 || strcmp(key, "_id") == 0) {
       // Handle both field names
   }
   ```

4. SCHEMA PATTERN (defining ID field):
   OLD: {"_id": {"type": "string", "required": true}}
   NEW:
   ```json
   {
       "uuid": {"type": "string", "required": true},
       "_id": {"type": "string", "required": false}  // Deprecated but supported
   }
   ```

5. JAVASCRIPT READ PATTERN:
   OLD: doc._id || doc["_id"]
   NEW:
   ```javascript
   const docId = doc.uuid || doc._id;  // Prefer uuid, fallback to _id
   ```

6. JAVASCRIPT WRITE PATTERN:
   OLD: doc._id = generateId();
   NEW:
   ```javascript
   const id = generateId();
   doc.uuid = id;
   doc._id = id;  // Maintain backward compatibility
   ```

7. HELPER FUNCTION PATTERN:
   ```c
   // Add this helper function to handle ID access
   const char* get_document_id(json_t *doc) {
       json_t *id = json_object_get(doc, "uuid");
       if (!id) {
           id = json_object_get(doc, "_id");
       }
       return id ? json_string_value(id) : NULL;
   }
   
   // Add this helper to set document ID
   void set_document_id(json_t *doc, const char *id_value) {
       json_object_set(doc, "uuid", json_string(id_value));
       json_object_set(doc, "_id", json_string(id_value));
   }
   ```
EOF
    
    echo -e "${GREEN}Migration templates generated: $templates_file${NC}"
}

# Function to create a summary report
create_summary_report() {
    local summary_file="$OUTPUT_DIR/migration_summary.txt"
    
    print_header "Creating Summary Report"
    
    {
        echo "UUID MIGRATION SUMMARY REPORT"
        echo "============================"
        echo "Generated: $(date)"
        echo
        echo "Total files with _id references:"
        
        # Count C files
        local c_count=$(find "$OUTPUT_DIR" -name "*.c.analysis" | wc -l)
        echo "  C source files: $c_count"
        
        # Count header files
        local h_count=$(find "$OUTPUT_DIR" -name "*.h.analysis" | wc -l)
        echo "  Header files: $h_count"
        
        # Count JS files
        local js_count=$(find "$OUTPUT_DIR" -name "*.js.analysis" | wc -l)
        echo "  JavaScript files: $js_count"
        
        # Count HTML files
        local html_count=$(find "$OUTPUT_DIR" -name "*.html.analysis" | wc -l)
        echo "  HTML files: $html_count"
        
        echo
        echo "Key files requiring attention:"
        echo "=============================="
        
        # List critical files
        echo
        echo "1. Core Database Operations:"
        echo "   - src/components/database/database.c"
        echo "   - src/components/database/operations.c"
        echo "   - src/components/database/indexed_document_operations.c"
        echo "   - src/components/database/batch_operations.c"
        
        echo
        echo "2. Binary Format & Persistence:"
        echo "   - src/components/binary/binary_format.c"
        echo "   - src/components/utils/metrics_persistence.c"
        
        echo
        echo "3. API & Authentication:"
        echo "   - src/components/core/api.c"
        echo "   - src/components/core/authentication_handler.c"
        echo "   - src/components/api/session_api.c"
        echo "   - src/components/api/rbac_api.c"
        
        echo
        echo "4. RBAC & Sessions:"
        echo "   - src/components/rbac/rbac_database.c"
        echo "   - src/components/rbac/rbac_sessions.c"
        echo "   - src/components/rbac/rbac_db.c"
        
        echo
        echo "5. Query & Schema:"
        echo "   - src/components/query/query_language.c"
        echo "   - src/components/database/schema.c"
        echo "   - src/components/database/json_schema_manager.c"
        
        echo
        echo "6. JavaScript Integration:"
        echo "   - src/components/js/js_native_storage.c"
        echo "   - share/htdocs/js/app.js"
        echo "   - share/examples/js-examples/comprehensive_examples.js"
        
        echo
        echo "7. Transaction Management:"
        echo "   - src/components/transaction/transaction.c"
        echo "   - src/components/api/transaction_api.c"
        
    } > "$summary_file"
    
    echo -e "${GREEN}Summary report created: $summary_file${NC}"
}

# Function to create migration checklist
create_migration_checklist() {
    local checklist_file="$OUTPUT_DIR/migration_checklist.md"
    
    cat > "$checklist_file" << 'EOF'
# UUID Migration Checklist

## Phase 1: Add Helper Functions
- [ ] Create `get_document_id()` helper in json_helpers.c
- [ ] Create `set_document_id()` helper in json_helpers.c
- [ ] Add helpers to json_helpers.h
- [ ] Test helper functions

## Phase 2: Update Core Database Operations
- [ ] Update database.c READ operations
- [ ] Update database.c WRITE operations
- [ ] Update operations.c for all CRUD operations
- [ ] Update indexed_document_operations.c
- [ ] Update batch_operations.c

## Phase 3: Update Binary Format
- [ ] Update binary_format.c serialization
- [ ] Update binary_format.c deserialization
- [ ] Ensure backward compatibility in file format

## Phase 4: Update API Layer
- [ ] Update api.c request handling
- [ ] Update api.c response generation
- [ ] Update authentication_handler.c
- [ ] Update session_api.c
- [ ] Update rbac_api.c

## Phase 5: Update Query System
- [ ] Update query_language.c to support both fields
- [ ] Update query optimizer
- [ ] Add query rewriting for _id -> uuid

## Phase 6: Update Schema System
- [ ] Update schema.c validation
- [ ] Update json_schema_manager.c
- [ ] Update system schemas to include uuid

## Phase 7: Update JavaScript Integration
- [ ] Update js_native_storage.c
- [ ] Update app.js frontend code
- [ ] Update example scripts
- [ ] Create migration utilities

## Phase 8: Update Tests
- [ ] Update all test files
- [ ] Create backward compatibility tests
- [ ] Create migration tests

## Phase 9: Documentation
- [ ] Update API documentation
- [ ] Create migration guide
- [ ] Update examples
- [ ] Update README files

## Phase 10: Final Testing
- [ ] Full regression testing
- [ ] Performance testing
- [ ] Backward compatibility testing
- [ ] Migration testing with existing data
EOF
    
    echo -e "${GREEN}Migration checklist created: $checklist_file${NC}"
}

# Main execution
main() {
    print_header "UUID Migration Analysis Tool"
    
    echo "This tool will analyze all files containing '_id' references and help"
    echo "you systematically update them to support both 'uuid' and '_id' fields."
    echo
    echo "Analysis output will be saved to: $OUTPUT_DIR"
    echo
    
    # Find all relevant files
    print_header "Finding all files with _id references"
    
    # C source files
    echo -e "${YELLOW}Analyzing C source files...${NC}"
    find /opt/jsondb/src -name "*.c" -type f | while read -r file; do
        if grep -q '"_id"' "$file" 2>/dev/null; then
            analyze_file "$file"
        fi
    done
    
    # Header files
    echo -e "${YELLOW}Analyzing header files...${NC}"
    find /opt/jsondb/src/include -name "*.h" -type f | while read -r file; do
        if grep -q '"_id"' "$file" 2>/dev/null; then
            analyze_file "$file"
        fi
    done
    
    # JavaScript files
    echo -e "${YELLOW}Analyzing JavaScript files...${NC}"
    find /opt/jsondb/share -name "*.js" -type f | while read -r file; do
        if grep -q '_id' "$file" 2>/dev/null; then
            analyze_file "$file"
        fi
    done
    
    # HTML files
    echo -e "${YELLOW}Analyzing HTML files...${NC}"
    find /opt/jsondb/share/htdocs -name "*.html" -type f | while read -r file; do
        if grep -q '_id' "$file" 2>/dev/null; then
            analyze_file "$file"
        fi
    done
    
    # Test files
    echo -e "${YELLOW}Analyzing test files...${NC}"
    find /opt/jsondb/tests -name "*.c" -type f | while read -r file; do
        if grep -q '"_id"' "$file" 2>/dev/null; then
            analyze_file "$file"
        fi
    done
    
    # Generate templates and reports
    generate_migration_templates
    create_summary_report
    create_migration_checklist
    
    print_header "Analysis Complete!"
    
    echo "Next steps:"
    echo "1. Review the analysis files in: $OUTPUT_DIR"
    echo "2. Use the migration templates in: $OUTPUT_DIR/migration_templates.txt"
    echo "3. Follow the checklist in: $OUTPUT_DIR/migration_checklist.md"
    echo "4. Start with helper functions, then update core components"
    echo
    echo -e "${GREEN}Run 'ls -la $OUTPUT_DIR' to see all generated files.${NC}"
}

# Run the main function
main "$@"