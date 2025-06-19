#!/usr/bin/env python3
"""
Systematically convert all json_free() calls to checkpoint comments.
This ensures single source of truth - checkpoint-based memory management.
"""

import re
import os
import sys

def convert_json_free_to_checkpoint(file_path):
    """Convert json_free calls to checkpoint comments in a single file."""
    
    with open(file_path, 'r') as f:
        content = f.read()
    
    original_content = content
    changes = 0
    
    # Pattern to match json_free calls that are not already commented
    # This handles various formats including those with spaces, newlines, etc.
    patterns = [
        # Simple pattern: json_free(something);
        (r'^(\s*)json_free\s*\(\s*([^)]+)\s*\)\s*;', r'\1/* CHECKPOINT: json_free(\2); */'),
        # Pattern with if: if (x) json_free(x);
        (r'^(\s*)(if\s*\([^)]+\)\s+)json_free\s*\(\s*([^)]+)\s*\)\s*;', r'\1\2/* CHECKPOINT: json_free(\3); */'),
        # Pattern for json_free on its own line
        (r'^(\s*)json_free\s*\(([^)]+)\)\s*;(\s*//.*)?$', r'\1/* CHECKPOINT: json_free(\2); */\3'),
    ]
    
    lines = content.split('\n')
    new_lines = []
    
    for line in lines:
        # Skip if already a checkpoint comment
        if 'CHECKPOINT:' in line and 'json_free' in line:
            new_lines.append(line)
            continue
        
        # Skip if it's inside a comment block
        if line.strip().startswith('/*') or line.strip().startswith('*'):
            new_lines.append(line)
            continue
            
        modified = False
        for pattern, replacement in patterns:
            if re.search(pattern, line, re.MULTILINE):
                new_line = re.sub(pattern, replacement, line, flags=re.MULTILINE)
                if new_line != line:
                    new_lines.append(new_line)
                    changes += 1
                    modified = True
                    break
        
        if not modified:
            new_lines.append(line)
    
    new_content = '\n'.join(new_lines)
    
    if new_content != original_content:
        with open(file_path, 'w') as f:
            f.write(new_content)
        return changes
    
    return 0

def main():
    """Process all C files with json_free calls."""
    
    # Read the list of files from our analysis
    with open('/tmp/json_free_calls.txt', 'r') as f:
        lines = f.readlines()
    
    # Extract unique file paths
    files = set()
    for line in lines:
        if ':' in line:
            file_path = line.split(':')[0]
            files.add(file_path)
    
    total_changes = 0
    processed_files = 0
    
    for file_path in sorted(files):
        if os.path.exists(file_path):
            changes = convert_json_free_to_checkpoint(file_path)
            if changes > 0:
                print(f"Converted {changes} json_free calls in {file_path}")
                total_changes += changes
                processed_files += 1
        else:
            print(f"Warning: File not found: {file_path}")
    
    print(f"\nTotal: Converted {total_changes} json_free calls across {processed_files} files")
    
if __name__ == '__main__':
    main()