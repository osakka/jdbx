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
./bin/jsondb_tools export <db_path> <output_path> [--collections=<name1,name2,...>]
```

**Options:**
- `--collections=<name1,name2,...>` or `-c <name1,name2,...>`: Export only specific collections
- `--help` or `-h`: Show help

**Examples:**

Export an entire database:
```bash
./bin/jsondb_tools export db.json export.json
```

Export specific collections:
```bash
./bin/jsondb_tools export db.json export.json --collections=users,products
```

### Database Import

Import database data from a file:

```bash
./bin/jsondb_tools import <db_path> <input_path> [--overwrite] [--collections=<name1,name2,...>]
```

**Options:**
- `--overwrite` or `-o`: Overwrite existing data
- `--collections=<name1,name2,...>` or `-c <name1,name2,...>`: Import only specific collections
- `--help` or `-h`: Show help

**Examples:**

Import an entire database (requires empty database unless --overwrite is used):
```bash
./bin/jsondb_tools import db.json import.json
```

Import with overwrite:
```bash
./bin/jsondb_tools import db.json import.json --overwrite
```

Import specific collections:
```bash
./bin/jsondb_tools import db.json import.json --collections=users,products
```

### Database Backup

Create a timestamped backup of a database:

```bash
./bin/jsondb_tools backup <db_path> [--dir=<backup_dir>]
```

**Options:**
- `--dir=<path>` or `-d <path>`: Directory for backups (default: ./backups)
- `--help` or `-h`: Show help

**Examples:**

Create a backup in the default directory:
```bash
./bin/jsondb_tools backup db.json
```

Create a backup in a specific directory:
```bash
./bin/jsondb_tools backup db.json --dir=/path/to/backups
```

### Database Information

Display information about a database:

```bash
./bin/jsondb_tools info <db_path>
```

**Options:**
- `--help` or `-h`: Show help

**Examples:**

Get database information:
```bash
./bin/jsondb_tools info db.json
```

## Integration with Scripts

These tools can be easily integrated into scripts for automating database operations:

### Automatic Backup Script Example

```bash
#!/bin/bash
# Automatic daily backup script

DB_PATH="/path/to/db.json"
BACKUP_DIR="/path/to/backups"

# Create backup directory if it doesn't exist
mkdir -p "$BACKUP_DIR"

# Create backup
./bin/jsondb_tools backup "$DB_PATH" --dir="$BACKUP_DIR"

# Remove backups older than 30 days
find "$BACKUP_DIR" -name "*.json" -type f -mtime +30 -delete
```

### Data Migration Script Example

```bash
#!/bin/bash
# Migrate data between database instances

SOURCE_DB="/path/to/source.json"
DEST_DB="/path/to/destination.json"
TEMP_EXPORT="/tmp/export.json"

# Export specific collections from source
./bin/jsondb_tools export "$SOURCE_DB" "$TEMP_EXPORT" --collections=users,products

# Import to destination
./bin/jsondb_tools import "$DEST_DB" "$TEMP_EXPORT" --overwrite

# Clean up
rm "$TEMP_EXPORT"
```

## Error Handling

All tools return appropriate exit codes:
- `0`: Success
- `1`: Error

Error messages are printed to stderr and include specific information about what went wrong.