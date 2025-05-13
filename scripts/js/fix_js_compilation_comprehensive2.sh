#!/bin/bash
# Comprehensive fix for JavaScript conditional compilation in main.c

echo "Creating a comprehensive fix for main.c..."

# Create a backup of main.c
cp /home/claude-3/project/src/core/main.c /home/claude-3/project/src/core/main.c.bak.comprehensive

# Fixed version of main.c - fixing the duplicate #endif at line 33
sed -i '33d' /home/claude-3/project/src/core/main.c

# Also check for errors around line 1480-1520 (JavaScript execution section)
# First, extract the section to analyze
sed -n '1470,1520p' /home/claude-3/project/src/core/main.c > /tmp/js_section.txt

# Fix the braces and indentation in the JavaScript execution section
# This helps eliminate nesting errors that cause compilation failures
sed -i '1479,1485s/            }$/        }/' /home/claude-3/project/src/core/main.c

echo "Done fixing main.c. Let's run a build test to verify the changes."