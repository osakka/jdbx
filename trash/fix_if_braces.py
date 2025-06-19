#!/usr/bin/env python3
"""
Fix if statements without braces that have checkpoint comments.
"""

import re
import os

def fix_if_braces(file_path):
    """Fix if statements without braces."""
    
    with open(file_path, 'r') as f:
        content = f.read()
    
    original_content = content
    
    # Pattern to match if statements without braces
    pattern = r'(\s*)(if\s*\([^)]+\))\s*(/\*\s*CHECKPOINT:.*?\*/)'
    
    # Replace with braced version
    def replacer(match):
        indent = match.group(1)
        condition = match.group(2)
        comment = match.group(3)
        return f"{indent}{condition} {{\n{indent}    {comment}\n{indent}}}"
    
    new_content = re.sub(pattern, replacer, content, flags=re.MULTILINE)
    
    if new_content != original_content:
        with open(file_path, 'w') as f:
            f.write(new_content)
        return True
    
    return False

# Files to fix from our grep output
files_to_fix = [
    'src/components/query/query_language.c',
    'src/components/utils/cache.c',
    'src/components/core/api.c',
    'src/components/rbac/jwt.c',
]

for file_path in files_to_fix:
    if os.path.exists(file_path):
        if fix_if_braces(file_path):
            print(f"Fixed if statements in {file_path}")
        else:
            print(f"No changes needed in {file_path}")
    else:
        print(f"File not found: {file_path}")