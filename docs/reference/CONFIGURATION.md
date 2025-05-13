# JSONdb Configuration System

This document outlines the configuration system for JSONdb, explaining how configuration values are managed throughout the codebase.

## Configuration Architecture

The JSONdb configuration system follows a layered approach with a centralized set of defaults:

1. **Centralized Defaults**: Common default values defined in `config_defaults.h`
2. **Path Resolution**: Automatic resolution of relative paths to be relative to the binary directory
3. **Configuration Files**: Support for both JSON and key-value configuration files
4. **Runtime Configuration**: Ability to override settings via command-line arguments

## Centralized Defaults

All default values are defined in a single header file: `src/include/utils/config_defaults.h`. This serves as the single source of truth for default configuration values across the codebase.

Categories of defaults include:

- Server configuration (port, host, connections)
- File paths (database, RBAC, logs, PID)
- Security settings (JWT secret, SSL)
- Component-specific settings (backup retention, cache size)

Example:
```c
/* Server defaults */
#define DEFAULT_PORT 5000
#define DEFAULT_HOST "claude-code.uk.home.arpa"
#define DEFAULT_MAX_CONNECTIONS 100

/* Path defaults */
#define DEFAULT_DB_PATH "var/data/jsondb/db.json"
#define DEFAULT_BACKUP_DIR "var/backups"
```

## Path Resolution

The configuration system automatically resolves relative paths to be relative to the binary directory:

1. The binary directory is detected at startup using `/proc/self/exe`
2. All relative paths (those not starting with `/`) are resolved relative to this directory
3. Absolute paths (starting with `/`) are used as-is

This enables consistent path handling regardless of the working directory when the server is started.

Example:
```c
/* In config_loader.c */
const char* bin_dir = config_get_binary_dir();
snprintf(resolved_path, PATH_MAX, "%s/%s", bin_dir, relative_path);
```

## Configuration Files

The system supports two configuration file formats:

1. **JSON format** (`.json` extension)
   ```json
   {
     "server": {
       "port": 5000,
       "host": "localhost"
     },
     "database": {
       "path": "/var/data/jsondb/db.json"
     }
   }
   ```

2. **Key-value format** (`.conf` extension)
   ```
   port = 5000
   host = localhost
   db_path = /var/data/jsondb/db.json
   ```

The configuration loader automatically detects the format based on file extension.

## Configuration Loading Sequence

1. Initialize with hardcoded defaults from `config_defaults.h`
2. Apply values from configuration file if specified
3. Override with command-line arguments if provided

This sequence ensures that command-line arguments take precedence over configuration files, which in turn override the defaults.

## Using the Configuration System

### Accessing Configuration Values

The global server configuration is available via `g_server_config`:

```c
server_config_t* config = g_server_config;
if (config) {
    int port = config->port;
    const char* host = config->host;
}
```

### Path Resolution

To resolve a path relative to the binary directory:

```c
const char* bin_dir = config_get_binary_dir();
char resolved_path[PATH_MAX];
snprintf(resolved_path, PATH_MAX, "%s/%s", bin_dir, DEFAULT_BACKUP_DIR);
```

### Component Configuration

Components should:

1. Include `utils/config_defaults.h` for default values
2. Use `config_get_binary_dir()` for path resolution
3. Consider both absolute and relative paths

Example from backup_api.c:
```c
/* Initialize backup directory */
if (DEFAULT_BACKUP_DIR[0] == '/') {
    /* Absolute path */
    strncpy(backup_dir, DEFAULT_BACKUP_DIR, PATH_MAX - 1);
} else {
    /* Relative to binary directory */
    const char* bin_dir = config_get_binary_dir();
    snprintf(backup_dir, PATH_MAX, "%s/%s", bin_dir, DEFAULT_BACKUP_DIR);
}
```

## Configuration Reference

| Category | Setting | Default | Description |
|----------|---------|---------|-------------|
| **Server** | port | 5000 | Server listening port |
| | host | "claude-code.uk.home.arpa" | Server hostname |
| | max_connections | 100 | Maximum simultaneous connections |
| **Paths** | db_path | "var/data/jsondb/db.json" | Database file path |
| | rbac_path | "var/data/jsondb/rbac.json" | RBAC configuration file path |
| | log_file | "var/log/jsondb/server.log" | Log file path |
| | pid_file | "var/run/jsondb/jsondb_server.pid" | PID file path |
| **Features** | js_enabled | 1 | Enable JavaScript support |
| | log_level | LOG_LEVEL_INFO | Logging verbosity |
| **Backup** | backup_interval_hours | 24 | Automatic backup interval |
| | backup_retention_count | 10 | Number of backups to retain |

## Best Practices

1. **Always Use Centralized Defaults**: Never hardcode a default value; always reference `config_defaults.h`
2. **Handle Path Resolution**: Consider both absolute and relative paths
3. **Validate Configuration**: Check values for validity before using them
4. **Provide Useful Logging**: Log configuration values during initialization
5. **Document New Settings**: Update this document when adding new configuration options