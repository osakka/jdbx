#!/bin/bash
# Script to migrate the JSON Database Server to the new directory structure

set -e  # Exit on any error

PROJECT_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/../.." && pwd )"
cd "$PROJECT_ROOT"

echo "JSON Database Server - Migration Script"
echo "======================================"
echo
echo "This script will migrate the current project to the new directory structure."
echo "Working directory: $PROJECT_ROOT"

# Check if we're already migrated
if [ -f "Makefile.new" ]; then
    echo "Migration file Makefile.new found. Proceeding with migration."
else
    echo "Error: Makefile.new not found. Aborting migration."
    exit 1
fi

# Create a backup
echo "Creating backup of current structure..."
BACKUP_DIR="backup_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$BACKUP_DIR"
cp -r src include Makefile "$BACKUP_DIR"
echo "Backup created at: $BACKUP_DIR"

# Set up new directory structure
echo "Setting up runtime directories..."
mkdir -p var/run var/log/jsondb var/data/jsondb

# Copy database files to new location
echo "Migrating database files..."
if [ -f "db.json" ]; then
    cp db.json var/data/jsondb/
    echo "Copied db.json to var/data/jsondb/"
fi

if [ -f "rbac.json" ]; then
    cp rbac.json var/data/jsondb/
    echo "Copied rbac.json to var/data/jsondb/"
fi

# Install the new Makefile
echo "Installing new Makefile..."
mv Makefile.new Makefile

echo
echo "Migration completed successfully!"
echo
echo "To build the project with the new structure, run:"
echo "  make clean && make"
echo
echo "To start the server in daemon mode:"
echo "  make daemon"
echo
echo "To stop the server:"
echo "  make stop"