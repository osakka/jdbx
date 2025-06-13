# JDBX Environment Configuration

JDBX server uses environment variables as the primary configuration mechanism. This approach provides flexibility, security, and standardization:

1. All paths are normalized to absolute paths
2. Environment variables can be easily set in different deployment environments
3. Configuration can be version-controlled separately from code
4. Sensitive information can be managed securely

## Configuration Hierarchy

JDBX uses the following configuration hierarchy (highest priority first):

1. Command-line arguments
2. Custom environment file (specified with `--env-file=FILE`)
3. Default environment file (`build/var/jdbxd.env`)
4. System environment variables
5. Hard-coded defaults

## Environment Variables

### Core Settings

| Variable | Description | Default |
|----------|-------------|---------|
| `JDBX_PORT` | Server port | 5000 |
| `JDBX_HOST` | Server bind address | 0.0.0.0 |
| `JDBX_VERBOSE` | Enable verbose output (true/false) | false |
| `JDBX_LOG_LEVEL` | Logging level (error, warning, info, debug, trace) | info |

### Path Settings

| Variable | Description | Default |
|----------|-------------|---------|
| `JDBX_BASE_DIR` | Base installation directory | /opt/jdbx |
| `JDBX_BUILD_DIR` | Build output directory | ${JDBX_BASE_DIR}/build |
| `JDBX_VAR_DIR` | Variable data directory | ${JDBX_BASE_DIR}/var |
| `JDBX_SHARE_DIR` | Shared data directory | ${JDBX_BASE_DIR}/share |

### File Paths

| Variable | Description | Default |
|----------|-------------|---------|
| `JDBX_DB_DIR` | Database file path | ${JDBX_VAR_DIR}/jdbx_database.json |
| `JDBX_RBAC_FILE` | RBAC configuration file | ${JDBX_VAR_DIR}/json_rbac.json |
| `JDBX_LOG_FILE` | Log file path | ${JDBX_VAR_DIR}/jdbxd.log |
| `JDBX_PID_FILE` | PID file path | ${JDBX_VAR_DIR}/jdbxd.pid |
| `JDBX_WEB_ROOT` | Web admin interface root | ${JDBX_SHARE_DIR}/htdocs |
| `JDBX_VALIDATORS_DIR` | Validators directory | ${JDBX_VAR_DIR}/validators |
| `JDBX_TRANSFORMS_DIR` | Transforms directory | ${JDBX_VAR_DIR}/transforms |
| `JDBX_METRICS_DIR` | Metrics directory | ${JDBX_VAR_DIR}/metrics |

### Advanced Settings

| Variable | Description | Default |
|----------|-------------|---------|
| `JDBX_MAX_CONNECTIONS` | Maximum client connections | 100 |
| `JDBX_THREAD_POOL_MIN` | Minimum thread pool size | 4 |
| `JDBX_THREAD_POOL_MAX` | Maximum thread pool size | 16 |
| `JDBX_THREAD_POOL_QUEUE_SIZE` | Thread pool queue size | 64 |

### SSL Configuration

| Variable | Description | Default |
|----------|-------------|---------|
| `JDBX_USE_SSL` | Enable SSL/TLS (true/false) | false |
| `JDBX_SSL_CERT_FILE` | SSL certificate file | ${JDBX_VAR_DIR}/ssl/server.crt |
| `JDBX_SSL_KEY_FILE` | SSL private key file | ${JDBX_VAR_DIR}/ssl/server.key |

## Using Environment Files

You can create a `.env` file with your configuration and use it with the runtime script:

```bash
build/jdbx_runtime.sh start --env-file=/path/to/custom.env
```

## Command-Line Arguments

Command-line arguments override environment variables. Example:

```bash
build/jdbx_runtime.sh start --port=8080 --host=127.0.0.1 --log-level=debug
```

## Testing Environment Configuration

A test script is provided to verify environment variable configuration:

```bash
scripts/testing/test_env_config.sh
```

This script creates a test environment file and verifies that the server correctly uses the configured values.

## Transitioning from etc/jdbx

The previous etc/jdbx configuration approach is being deprecated in favor of environment variables. To migrate:

1. Copy your existing configuration values to an environment file
2. Use the `--env-file` parameter with the runtime script
3. Verify the server starts with the correct configuration