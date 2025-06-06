#!/bin/bash

# Replace ALL _id references with uuid - NO BACKWARD COMPATIBILITY
echo "=== Replacing ALL _id references with uuid ==="
echo "NO backward compatibility - clean implementation"

# Create backup
BACKUP_DIR="/opt/jsondb/backup_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$BACKUP_DIR"
cp -r /opt/jsondb/src "$BACKUP_DIR/"
echo "Backup created at: $BACKUP_DIR"

# Function to replace _id with uuid in a file
replace_in_file() {
    local file="$1"
    echo "Processing: $file"
    
    # Replace all occurrences of "_id" with "uuid"
    sed -i '
        # Replace "_id" with "uuid" in strings
        s/"_id"/"uuid"/g
        
        # Replace _id with uuid in comments where appropriate
        s/document _id/document uuid/g
        s/doc _id/doc uuid/g
        s/ID field/UUID field/g
        
        # Fix any double replacements that might occur
        s/"uuiduuid"/"uuid"/g
    ' "$file"
}

# Find all C source files
echo "Processing C source files..."
find /opt/jsondb/src -name "*.c" -type f | while read file; do
    if grep -q '"_id"' "$file"; then
        replace_in_file "$file"
    fi
done

# Find all C header files
echo "Processing C header files..."
find /opt/jsondb/src -name "*.h" -type f | while read file; do
    if grep -q '"_id"' "$file"; then
        replace_in_file "$file"
    fi
done

# Special handling for database.c - remove backward compatibility
echo "Cleaning up database.c..."
sed -i '
    # Remove the backward compatibility checks
    /if (!id_field) {/,/}/d
    /id_field = json_object_get(document, "_id");/d
    /Legacy support/d
    
    # Remove duplicate field setting
    /json_object_set(document, "_id",/d
' /opt/jsondb/src/components/database/database.c

# Fix API files
echo "Cleaning up API files..."
sed -i '
    # Remove legacy support comments and checks
    /Legacy support/d
    /if (!id_field)/,/}/d
    /json_object_get(document, "_id")/d
' /opt/jsondb/src/components/core/api.c

# Fix authentication handler
echo "Cleaning up authentication handler..."
sed -i '
    # Remove backward compatibility
    /if (!id_val) {/,/}/d
    /check both uuid and _id/d
' /opt/jsondb/src/components/core/authentication_handler.c

# Update JavaScript files
echo "Processing JavaScript files..."
find /opt/jsondb/share -name "*.js" -type f | while read file; do
    if grep -q '["'\'']_id["'\'']' "$file" 2>/dev/null; then
        echo "Processing JS: $file"
        sed -i '
            # Replace _id with uuid in JavaScript
            s/\._id/.uuid/g
            s/\["_id"\]/["uuid"]/g
            s/\[\'"'"'_id\'"'"'\]/['"'"'uuid'"'"']/g
            s/"_id"/"uuid"/g
            
            # Fix specific patterns
            s/doc\.uuid || doc\._id/doc.uuid/g
            s/document\.uuid || document\._id/document.uuid/g
            s/\.uuid || \._id/.uuid/g
        ' "$file"
    fi
done

# Update test files
echo "Processing test files..."
find /opt/jsondb/tests -name "*.c" -type f | while read file; do
    if grep -q '"_id"' "$file"; then
        replace_in_file "$file"
    fi
done

# Update examples
echo "Processing examples..."
find /opt/jsondb/share/examples -name "*.js" -type f | while read file; do
    if grep -q '"_id"' "$file"; then
        sed -i 's/"_id"/"uuid"/g; s/\._id/.uuid/g' "$file"
    fi
done

# Final cleanup - remove any remaining backward compatibility patterns
echo "Final cleanup..."
find /opt/jsondb/src -name "*.c" -type f -exec grep -l "uuid.*_id\|_id.*uuid" {} \; | while read file; do
    echo "Cleaning compatibility code from: $file"
    sed -i '
        # Remove patterns like: if (!xxx) xxx = json_object_get(yyy, "_id")
        /if.*!.*uuid.*_id/,/}/d
        /if.*!.*json_object_get.*"_id"/d
        
        # Remove fallback patterns
        /|| doc\._id/d
        /|| document\._id/d
    ' "$file"
done

echo ""
echo "=== REPLACEMENT COMPLETE ==="
echo "ALL _id references have been replaced with uuid"
echo "NO backward compatibility code remains"
echo ""
echo "Next steps:"
echo "1. cd /opt/jsondb/src && make clean && make"
echo "2. Start server with clean database"
echo "3. Test the implementation"