# JSONdb Environment Configuration System

## Overview

JSONdb now supports a comprehensive environment variable-based configuration system. This system provides several benefits:

1. **Path Normalization**: All paths are normalized to absolute paths
2. **Environment-Based Configuration**: Server settings can be configured through environment variables
3. **Configuration Hierarchy**: Clear precedence of configuration sources
4. **Standardized Variables**: Consistent naming scheme for all settings

## Configuration Hierarchy

Configuration settings are applied in the following order (highest precedence first):

1. Command-line arguments
2. Environment variables
3. Configuration files
4. Default values

## Implementation

The environment configuration system is implemented in the following files:

- `/opt/jsondb/src/components/utils/path_normalizer.c`: Converts relative paths to absolute and loads environment variables
- `/opt/jsondb/src/include/utils/path_normalizer.h`: Interface for path normalization functions
- `/opt/jsondb/src/initialize/config.c`: Integrates environment loading into the config initialization sequence
- `/opt/jsondb/build/jsondb_runtime.sh`: Loads environment from files and passes them to the server

## Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `JSONDB_BASE_DIR` | Base installation directory | `/opt/jsondb` |
| `JSONDB_PORT` | Server port | 5000 |
| `JSONDB_HOST` | Binding address | 0.0.0.0 |
| `JSONDB_VERBOSE` | Verbose mode (true/false) | false |
| `JSONDB_LOG_LEVEL` | Log level (error, warning, info, debug, trace) | info |
| `JSONDB_DB_DIR` | Database directory | ${JSONDB_VAR_DIR}/jsondb_database.json |
| `JSONDB_LOG_FILE` | Log file path | ${JSONDB_VAR_DIR}/jsondb_server.log |
| `JSONDB_PID_FILE` | PID file path | ${JSONDB_VAR_DIR}/jsondb_server.pid |
| `JSONDB_WEB_ROOT` | Web admin interface root | ${JSONDB_SHARE_DIR}/htdocs |
| `JSONDB_VALIDATORS_DIR` | Validators directory | ${JSONDB_VAR_DIR}/validators |
| `JSONDB_TRANSFORMS_DIR` | Transforms directory | ${JSONDB_VAR_DIR}/transforms |
| `JSONDB_METRICS_DIR` | Metrics directory | ${JSONDB_VAR_DIR}/metrics |
| `JSONDB_RBAC_FILE` | RBAC file path | ${JSONDB_VAR_DIR}/json_rbac.json |
| `JSONDB_MAX_CONNECTIONS` | Maximum client connections | 100 |
| `JSONDB_THREAD_POOL_MIN` | Minimum thread pool size | 4 |
| `JSONDB_THREAD_POOL_MAX` | Maximum thread pool size | 16 |
| `JSONDB_USE_SSL` | Enable SSL/TLS (true/false) | false |
| `JSONDB_SSL_CERT_FILE` | SSL certificate file | ${JSONDB_VAR_DIR}/ssl/server.crt |
| `JSONDB_SSL_KEY_FILE` | SSL private key file | ${JSONDB_VAR_DIR}/ssl/server.key |

## Using Environment Variables

### Direct Environment Variables

You can set environment variables directly:

```bash
JSONDB_PORT=5678 JSONDB_HOST=127.0.0.1 build/bin/jsondb_server --verbose
```

### Environment Variable Files

You can also use an environment file with the runtime script:

```bash
build/jsondb_runtime.sh start --env-file=/path/to/custom.env
```

Example environment file:

```bash
#!/bin/bash
# Environment configuration

# Base paths 
export JSONDB_BASE_DIR="/opt/jsondb"
export JSONDB_VAR_DIR="${JSONDB_BASE_DIR}/var"

# Server configuration
export JSONDB_PORT="5432"
export JSONDB_HOST="127.0.0.1"
export JSONDB_VERBOSE="true"
export JSONDB_LOG_LEVEL="debug"
```

## Path Normalization

The path normalization system handles:

1. Conversion of relative paths to absolute
2. Creation of required directories
3. Consistent path handling across different components

## Testing

Test scripts in the `/opt/jsondb/scripts/testing/` directory demonstrate the configuration system:

- `export_env_test.sh`: Tests direct environment variable setting
- `simple_env_test.sh`: Tests simplified environment configuration
- `test_env_config.sh`: Comprehensive environment configuration test
- `test_daemon_env_config.sh`: Tests environment configuration in daemon mode

## Remaining Issues

While the environment variable loading is working correctly, there are some remaining issues to address:

1. The server initialization sequence appears to hang after RBAC initialization - this needs further debugging
2. Daemon mode socket binding with environment variables is not yet fully tested
3. User documentation for the environment configuration system should be expanded

## Conclusion

The environment variable configuration system provides a flexible and standardized way to configure the JSONdb server. It supports absolute paths, hierarchical configuration, and works with the existing command-line arguments system.