#!/bin/bash

# Simple UUID migration script
# This script updates common _id patterns to support both uuid and _id

echo "Creating backup..."
BACKUP_DIR="/opt/jsondb/backup_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$BACKUP_DIR"
cp -r /opt/jsondb/src "$BACKUP_DIR/"

echo "Starting migration..."

# Pattern 1: json_object_get(xxx, "_id") -> check uuid first
find /opt/jsondb/src -name "*.c" -type f | while read file; do
    echo "Processing: $file"
    
    # Create a temporary file
    tmpfile=$(mktemp)
    
    # Process the file
    awk '
    {
        # Pattern: json_object_get(something, "_id")
        if (match($0, /json_object_get\([^,]+, "_id"\)/)) {
            # Extract the object name
            match($0, /json_object_get\(([^,]+), "_id"\)/, arr)
            obj = arr[1]
            
            # Replace with uuid check first
            gsub(/json_object_get\([^,]+, "_id"\)/, "json_object_get(" obj ", \"uuid\")")
            print $0
            print "      if (!" gensub(/.*?([\w_]+) = json_object_get.*/, "\\1", 1) ") {"
            print "        " gensub(/(.*?)([\w_]+)( = )json_object_get\([^,]+, "uuid"\)(.*)/, "\\1\\2\\3json_object_get(" obj ", \"_id\")\\4", 1)
            print "      }"
        }
        # Pattern: json_object_set(something, "_id", value)
        else if (match($0, /json_object_set\([^,]+, "_id",/)) {
            # Keep the line but add uuid set after
            print $0
            # Extract components
            match($0, /^([[:space:]]*)(.*)json_object_set\(([^,]+), "_id", ([^)]+)\)(.*)$/, arr)
            if (arr[3] && arr[4]) {
                print arr[1] arr[2] "json_object_set(" arr[3] ", \"uuid\", " arr[4] ");" arr[5]
            }
        }
        else {
            print $0
        }
    }
    ' "$file" > "$tmpfile"
    
    # Only replace if changes were made
    if ! diff -q "$file" "$tmpfile" > /dev/null 2>&1; then
        mv "$tmpfile" "$file"
        echo "  Updated: $file"
    else
        rm "$tmpfile"
    fi
done

echo "Migration complete!"
echo "Backup saved to: $BACKUP_DIR"
echo ""
echo "Next steps:"
echo "1. Review the changes"
echo "2. Compile and test"
echo "3. If issues, restore from: $BACKUP_DIR"