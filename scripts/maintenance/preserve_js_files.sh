#!/bin/bash
# Script to preserve JavaScript files from src/js/utils/ that don't exist in components

JS_DIR="/home/claude-3/project/src/js/utils"
COMPONENTS_JS_DIR="/home/claude-3/project/src/components/js/utils"
TARGET_DIR="/home/claude-3/project/share/js/functions"

# Create target directory if it doesn't exist
mkdir -p "$TARGET_DIR"

echo "Preserving JavaScript files from $JS_DIR that don't exist in components..."

# Find all JavaScript files in src/js/utils
JS_FILES=$(find "$JS_DIR" -name "*.js")

# Check each file
for js_file in $JS_FILES; do
    filename=$(basename "$js_file")
    
    # Check if file exists in components
    if [ ! -f "$COMPONENTS_JS_DIR/$filename" ]; then
        echo "Preserving $filename"
        cp -v "$js_file" "$TARGET_DIR/"
    else
        echo "Skipping $filename (already exists in components)"
    fi
done

echo "Done. JavaScript files preserved in $TARGET_DIR"