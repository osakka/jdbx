# Database Tools

This document describes the utility tools included with the JSON Database Server.

## Overview

The database tools provide functionality to:
- Export database data
- Import database data
- Create and manage backups
- Get information about a database

These tools are essential for data migration, backup/restore operations, and database maintenance.

## Building the Tools

The tools are built automatically when you run:

```bash
make
```

This will build both the server and the tools. You can also build just the tools:

```bash
make tools
```

The tools will be placed in the `bin` directory.

## Using the Tools

### Database Export

Export database data to a file:

```bash
./bin/jdbx_tools export <db_path> <output_path> [--collections=<name1,name2,...>]
```

**Options:**
- `--collections=<name1,name2,...>` or `-c <name1,name2,...>`: Export only specific collections
- `--help` or `-h`: Show help

**Examples:**

Export an entire database:
```bash
./bin/jdbx_tools export db.json export.json
```

Export specific collections:
```bash
./bin/jdbx_tools export db.json export.json --collections=users,products
```

### Database Import

Import database data from a file:

```bash
./bin/jdbx_tools import <db_path> <input_path> [--overwrite] [--collections=<name1,name2,...>]
```

**Options:**
- `--overwrite` or `-o`: Overwrite existing data
- `--collections=<name1,name2,...>` or `-c <name1,name2,...>`: Import only specific collections
- `--help` or `-h`: Show help

**Examples:**

Import an entire database (requires empty database unless --overwrite is used):
```bash
./bin/jdbx_tools import db.json import.json
```

Import with overwrite:
```bash
./bin/jdbx_tools import db.json import.json --overwrite
```

Import specific collections:
```bash
./bin/jdbx_tools import db.json import.json --collections=users,products
```

### Database Backup

Create a timestamped backup of a database:

```bash
./bin/jdbx_tools backup <db_path> [--dir=<backup_dir>]
```

**Options:**
- `--dir=<path>` or `-d <path>`: Directory for backups (default: backups directory relative to binary)
- `--help` or `-h`: Show help

**Examples:**

Create a backup in the default directory:
```bash
./bin/jdbx_tools backup db.json
```

Create a backup in a specific directory:
```bash
./bin/jdbx_tools backup db.json --dir=custom/backup/path
```
Note: If the backup directory is not an absolute path, it will be treated as relative to the binary location.

### Database Information

Display information about a database:

```bash
./bin/jdbx_tools info <db_path>
```

**Options:**
- `--help` or `-h`: Show help

**Examples:**

Get database information:
```bash
./bin/jdbx_tools info db.json
```

## Integration with Scripts

These tools can be easily integrated into scripts for automating database operations:

### Automatic Backup Script Example

```bash
#!/bin/bash
# Automatic daily backup script

# Get the directory of the script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Set paths relative to the script directory
DB_PATH="var/data/jdbx/db.json"
BACKUP_DIR="backups"

# Change to the script directory to ensure paths are relative to server binary
cd "$SCRIPT_DIR"

# Create backup directory if it doesn't exist (will create relative to binary)
mkdir -p "$BACKUP_DIR"

# Create backup
./bin/jdbx_tools backup "$DB_PATH" --dir="$BACKUP_DIR"

# Remove backups older than 30 days
find "$BACKUP_DIR" -name "*.json" -type f -mtime +30 -delete
```

### Data Migration Script Example

```bash
#!/bin/bash
# Migrate data between database instances

# Get the directory of the script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Change to the script directory to ensure paths are relative to server binary
cd "$SCRIPT_DIR"

SOURCE_DB="var/data/jdbx/source.json"
DEST_DB="var/data/jdbx/destination.json"
TEMP_EXPORT="tmp/export.json"

# Create temp directory if it doesn't exist
mkdir -p "tmp"

# Export specific collections from source
./bin/jdbx_tools export "$SOURCE_DB" "$TEMP_EXPORT" --collections=users,products

# Import to destination
./bin/jdbx_tools import "$DEST_DB" "$TEMP_EXPORT" --overwrite

# Clean up
rm "$TEMP_EXPORT"
```

## Error Handling

All tools return appropriate exit codes:
- `0`: Success
- `1`: Error

Error messages are printed to stderr and include specific information about what went wrong.