# Hardcoded Paths Fix Summary

## Overview

This document summarizes the changes made to remove hardcoded paths from the JSONdb codebase and make all paths configurable through environment variables.

## Changes Made

### 1. config_loader.c
- **Removed**: Hardcoded `/opt/jsondb` fallback path
- **Added**: Support for `JSONDB_BASE_PATH` environment variable
- **Fallback**: Now uses current directory (`.`) if no base path is set
- **Location**: `/opt/jsondb/src/components/utils/config_loader.c`

### 2. database.c
- **Removed**: Hardcoded documentation path `/opt/jsondb/docs/`
- **Changed**: Now references `docs/` directory relative to installation
- **Location**: `/opt/jsondb/src/components/database/database.c`

### 3. jsondb_runtime.sh
- **Added**: Dynamic base directory detection
- **Added**: Support for `JSONDB_BASE_PATH`, `JSONDB_DOC_PATH`, `JSONDB_VAR_PATH`
- **Changed**: All paths now use environment variables instead of hardcoded values
- **Improved**: Script now tries to auto-detect base directory from script location
- **Location**: `/opt/jsondb/build/jsondb_runtime.sh`

### 4. config_defaults.h
- **Added**: Documentation about environment variable overrides
- **Clarified**: Path resolution behavior
- **Location**: `/opt/jsondb/src/include/utils/config_defaults.h`

### 5. environment.c
- **Added**: Support for `JSONDB_BASE_PATH` (primary) and `JSONDB_BASE_DIR` (backward compatibility)
- **Added**: Support for `JSONDB_DOC_PATH` and `JSONDB_VAR_PATH`
- **Location**: `/opt/jsondb/src/components/utils/environment.c`

### 6. jsondb.env template
- **Added**: New section for base path configuration
- **Changed**: All example paths now use environment variable references
- **Updated**: Installation instructions to be more generic
- **Location**: `/opt/jsondb/share/config/jsondb.env`

## New Environment Variables

### Primary Configuration
- `JSONDB_BASE_PATH`: Base installation directory (auto-detected if not set)
- `JSONDB_DOC_PATH`: Documentation directory (default: `${JSONDB_BASE_PATH}/docs`)
- `JSONDB_VAR_PATH`: Variable data directory (default: `${JSONDB_BASE_PATH}/build/var`)

### Path Overrides (existing, still supported)
- `JSONDB_DB_DIR`: Database directory
- `JSONDB_RBAC_FILE`: RBAC configuration file
- `JSONDB_LOG_FILE`: Log file path
- `JSONDB_PID_FILE`: PID file path
- `JSONDB_WEB_ROOT`: Web interface root
- `JSONDB_VALIDATORS_DIR`: Validators directory
- `JSONDB_TRANSFORMS_DIR`: Transforms directory
- `JSONDB_METRICS_DIR`: Metrics directory

## Backward Compatibility

1. **JSONDB_BASE_DIR**: Still supported as fallback if `JSONDB_BASE_PATH` is not set
2. **Default Behavior**: If no environment variables are set, the system will:
   - Try to detect the base directory from the binary location
   - Fall back to current working directory
   - Use relative paths from the detected base

3. **Standard Installation**: The default `/opt/jsondb` location will still work if:
   - The runtime script is executed from `/opt/jsondb/build/`
   - Or `JSONDB_BASE_PATH` is set to `/opt/jsondb`

## Usage Examples

### Standard Installation
```bash
# No changes needed - auto-detection will find /opt/jsondb
cd /opt/jsondb/build
./jsondb_runtime.sh start
```

### Custom Installation
```bash
# Set base path for custom location
export JSONDB_BASE_PATH=/home/user/jsondb
cd $JSONDB_BASE_PATH/build
./jsondb_runtime.sh start
```

### Portable Installation
```bash
# Use relative paths from current directory
cd /path/to/jsondb
export JSONDB_BASE_PATH=$(pwd)
./build/jsondb_runtime.sh start
```

### Override Individual Paths
```bash
# Custom paths for specific components
export JSONDB_BASE_PATH=/opt/jsondb
export JSONDB_DB_DIR=/var/lib/jsondb/data
export JSONDB_LOG_FILE=/var/log/jsondb/server.log
./build/jsondb_runtime.sh start
```

## Testing

A test script has been created at `/opt/jsondb/test_config_paths.sh` to verify:
- Default behavior without environment variables
- Path configuration with `JSONDB_BASE_PATH`
- Custom base path support
- Backward compatibility with `JSONDB_BASE_DIR`

## Benefits

1. **Flexibility**: JSONdb can now be installed anywhere without modifying source code
2. **Portability**: Easier to package for different distributions and containers
3. **Development**: Developers can run multiple instances with different configurations
4. **Deployment**: Better support for various deployment scenarios (Docker, systemd, etc.)
5. **Backward Compatible**: Existing installations continue to work without changes