#\!/bin/bash

# Source Code Reorganization Script
# This script implements the source code reorganization plan
# described in docs/planning/source_reorganization_implementation.md

set -e  # Exit on any error

echo "Starting source code reorganization..."

# Create backup branch
BACKUP_BRANCH="backup-before-reorg-$(date +%Y%m%d)"
git branch $BACKUP_BRANCH
echo "Created backup branch: $BACKUP_BRANCH"

# Create required directories
echo "Creating directory structure..."
mkdir -p src/core
mkdir -p src/query
mkdir -p src/rbac
mkdir -p include/core
mkdir -p include/query
mkdir -p include/rbac

# First handle files that should be moved to new directories
echo "Moving core files..."
git mv src/server.c src/core/
git mv src/ssl.c src/core/
git mv src/cors.c src/core/
git mv src/main.c src/core/
git mv include/server.h include/core/
git mv include/ssl.h include/core/

echo "Moving query files..."
git mv src/query_language.c src/query/
git mv include/query_language.h include/query/

echo "Moving RBAC files..."
git mv src/rbac.c src/rbac/
git mv src/jwt.c src/rbac/
git mv include/rbac.h include/rbac/
git mv include/jwt.h include/rbac/

echo "Moving JSON utilities..."
git mv src/json.c src/utils/

# Handle files that exist in both root and subdirectories
echo "Consolidating duplicate files..."

# API files
for file in admin_api backup_api cache_api index_api index_query_api schema_api system_api transaction_log_api visualization_api; do
    if [ -f "src/${file}.c" ] && [ -f "src/api/${file}.c" ]; then
        echo "Removing duplicate: src/${file}.c"
        git rm "src/${file}.c"
    fi
done

# Special case for transaction_api.c - files differ
if [ -f "src/transaction_api.c" ] && [ -f "src/api/transaction_api.c" ]; then
    echo "Special case: transaction_api.c (files differ)"
    echo "Creating backup of src/transaction_api.c as src/api/transaction_api.c.root"
    cp "src/transaction_api.c" "src/api/transaction_api.c.root"
    git add "src/api/transaction_api.c.root"
    git rm "src/transaction_api.c"
fi

# Database files
for file in database index schema lock_manager; do
    if [ -f "src/${file}.c" ] && [ -f "src/database/${file}.c" ]; then
        echo "Removing duplicate: src/${file}.c"
        git rm "src/${file}.c"
    fi
done

# JS files
for file in js_api js_engine; do
    if [ -f "src/${file}.c" ] && [ -f "src/js/${file}.c" ]; then
        echo "Removing duplicate: src/${file}.c"
        git rm "src/${file}.c"
    fi
done

# Transaction files
for file in transaction transaction_log transaction_retry transaction_visualization; do
    if [ -f "src/${file}.c" ] && [ -f "src/transaction/${file}.c" ]; then
        echo "Removing duplicate: src/${file}.c"
        git rm "src/${file}.c"
    fi
done

# Clean up alternative main.c files
if [ -f "src/main_fixed.c" ]; then
    echo "Backing up src/main_fixed.c"
    cp "src/main_fixed.c" "src/core/main_fixed.c.bak"
    git add "src/core/main_fixed.c.bak"
    git rm "src/main_fixed.c"
fi

if [ -f "src/main_refactored.c" ]; then
    echo "Backing up src/main_refactored.c"
    cp "src/main_refactored.c" "src/core/main_refactored.c.bak"
    git add "src/core/main_refactored.c.bak"
    git rm "src/main_refactored.c"
fi

# Handle quickjs_mock.c
if [ -f "src/quickjs_mock.c" ]; then
    echo "Moving quickjs_mock.c to js directory"
    git mv "src/quickjs_mock.c" "src/js/"
fi

# Move include files to match source structure
echo "Organizing include files..."

# Update include references (examples - will need to be extended)
echo "Updating include references in source files..."
find src include -name "*.c" -o -name "*.h"  < /dev/null |  xargs sed -i 's|#include "../include/|#include "|g'
find src include -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "include/|#include "|g'

echo "Source code reorganization completed."
echo ""
echo "Next steps:"
echo "1. Update the Makefile to reflect the new directory structure"
echo "2. Test building the project with 'make'"
echo "3. Resolve any build issues"
echo "4. Commit the changes"
