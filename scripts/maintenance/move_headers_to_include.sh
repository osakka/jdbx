#!/bin/bash
# Script to move header files from components directory to include directory

ROOT_DIR="/home/claude-3/project"
cd "$ROOT_DIR" || exit 1

echo "Moving header files from components to include directory..."

# Find all header files in components directory
HEADER_FILES=$(find src/components -name "*.h")

for header in $HEADER_FILES; do
    # Determine destination path
    rel_path=${header#src/components/}
    dir_path=$(dirname "$rel_path")
    file_name=$(basename "$header")
    dest_dir="src/include/$dir_path"
    dest_file="$dest_dir/$file_name"
    
    # Create destination directory if it doesn't exist
    mkdir -p "$dest_dir"
    
    # Check if the file already exists in the include directory
    if [ -f "$dest_file" ]; then
        echo "CONFLICT: $dest_file already exists"
        # Compare files
        if cmp -s "$header" "$dest_file"; then
            echo "Files are identical - removing from components"
            rm "$header"
        else
            echo "Files differ - keeping newer version"
            if [ "$header" -nt "$dest_file" ]; then
                echo "Components version is newer - replacing include version"
                cp "$header" "$dest_file"
                rm "$header"
            else
                echo "Include version is newer - removing components version"
                rm "$header"
            fi
        fi
    else
        echo "Moving $header to $dest_file"
        mv "$header" "$dest_file"
    fi
done

echo "Header migration complete"