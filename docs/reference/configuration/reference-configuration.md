# JDBX Configuration Reference

**Version**: 2.0.6  
**Last Updated**: January 2025

This comprehensive guide covers all configuration options for JDBX, including environment variables, configuration files, and command-line arguments.

## Table of Contents

1. [Configuration Hierarchy](#configuration-hierarchy)
2. [Configuration Files](#configuration-files)
3. [Environment Variables](#environment-variables)
4. [Configuration Options Reference](#configuration-options-reference)
5. [Examples](#examples)
6. [Security Considerations](#security-considerations)

## Configuration Hierarchy

JDBX applies configuration settings in the following order (highest precedence first):

1. **Command-line arguments** - Override all other settings
2. **Environment variables** - Override configuration files and defaults
3. **Configuration files** - Override default values
4. **Default values** - Built-in defaults from `config_defaults.h`

## Configuration Files

JDBX supports two configuration file formats:

### JSON Format

Create a `config.json` file:

```json
{
  "server": {
    "port": 5000,
    "host": "0.0.0.0",
    "max_connections": 100,
    "timeout": 30
  },
  "database": {
    "path": "var/data/jdbx/db.json",
    "auto_save": true,
    "save_interval": 60
  },
  "rbac": {
    "path": "var/data/jdbx/rbac.json",
    "auto_save": true,
    "save_interval": 60
  },
  "jwt": {
    "secret": "REPLACE_THIS_WITH_A_SECURE_SECRET_KEY",
    "expiration": 86400,
    "algorithm": "HS256",
    "issuer": "jdbx"
  },
  "ssl": {
    "enabled": false,
    "cert_path": "certs/certificate.crt",
    "key_path": "certs/private.key",
    "ca_path": "certs/ca.crt"
  },
  "logging": {
    "level": "info",
    "file": "var/log/jdbx/server.log",
    "console": true,
    "max_size": 10485760,
    "max_files": 5
  },
  "cors": {
    "enabled": true,
    "allowed_origins": ["*"],
    "allowed_methods": ["GET", "POST", "PUT", "DELETE", "OPTIONS"],
    "allowed_headers": ["Content-Type", "Authorization"],
    "expose_headers": ["Content-Length"],
    "allow_credentials": false,
    "max_age": 86400
  },
  "throttling": {
    "enabled": false,
    "rate": 100,
    "burst": 50,
    "ip_whitelist": ["127.0.0.1"]
  },
  "admin": {
    "username": "admin",
    "password": "CHANGE_THIS_PASSWORD_IMMEDIATELY"
  }
}
```

### Key-Value Format

Create a `.conf` file:

```ini
# Server configuration
port = 5000
host = 0.0.0.0
max_connections = 100

# Database configuration
db_path = var/data/jdbx/db.json
auto_save = true
save_interval = 60

# RBAC configuration
rbac_path = var/data/jdbx/rbac.json

# JWT configuration
jwt_secret = REPLACE_THIS_WITH_A_SECURE_SECRET_KEY
jwt_expiration = 86400

# Logging configuration
log_level = info
log_file = var/log/jdbx/server.log
```

### Loading Configuration Files

```bash
# Specify configuration file
./bin/jdbxd -config /path/to/config.json

# Or use the runtime script
build/jdbx_runtime.sh start --config=/path/to/config.json
```

## Environment Variables

JDBX supports comprehensive environment variable configuration:

### Core Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `JDBX_BASE_DIR` | Base installation directory | `/opt/jdbx` |
| `JDBX_VAR_DIR` | Variable data directory | `${JDBX_BASE_DIR}/var` |
| `JDBX_SHARE_DIR` | Shared resources directory | `${JDBX_BASE_DIR}/share` |

### Server Configuration

| Variable | Description | Default |
|----------|-------------|---------|
| `JDBX_PORT` | Server listening port | `5000` |
| `JDBX_HOST` | Server binding address | `0.0.0.0` |
| `JDBX_MAX_CONNECTIONS` | Maximum concurrent connections | `100` |
| `JDBX_TIMEOUT` | Connection timeout (seconds) | `30` |
| `JDBX_THREAD_POOL_MIN` | Minimum thread pool size | `4` |
| `JDBX_THREAD_POOL_MAX` | Maximum thread pool size | `16` |

### Database Configuration

| Variable | Description | Default |
|----------|-------------|---------|
| `JDBX_DB_PATH` | Database file path | `${JDBX_VAR_DIR}/data/jdbx/db.json` |
| `JDBX_DB_DIR` | Database directory | `${JDBX_VAR_DIR}/jdbx_database.json` |
| `JDBX_AUTO_SAVE` | Enable auto-save | `true` |
| `JDBX_SAVE_INTERVAL` | Auto-save interval (seconds) | `60` |

### RBAC Configuration

| Variable | Description | Default |
|----------|-------------|---------|
| `JDBX_RBAC_FILE` | RBAC configuration file | `${JDBX_VAR_DIR}/data/jdbx/rbac.json` |
| `JDBX_RBAC_PATH` | Alternative RBAC path | `${JDBX_VAR_DIR}/json_rbac.json` |

### JWT Configuration

| Variable | Description | Default |
|----------|-------------|---------|
| `JDBX_JWT_SECRET` | JWT signing secret | `REPLACE_THIS_WITH_A_SECURE_SECRET_KEY` |
| `JDBX_JWT_EXPIRATION` | Token expiration (seconds) | `86400` |
| `JDBX_JWT_ALGORITHM` | JWT algorithm | `HS256` |
| `JDBX_JWT_ISSUER` | JWT issuer | `jdbx` |

### SSL/TLS Configuration

| Variable | Description | Default |
|----------|-------------|---------|
| `JDBX_USE_SSL` | Enable SSL/TLS | `false` |
| `JDBX_SSL_ENABLED` | Alternative SSL enable flag | `false` |
| `JDBX_SSL_CERT_FILE` | SSL certificate file | `${JDBX_VAR_DIR}/ssl/server.crt` |
| `JDBX_SSL_KEY_FILE` | SSL private key file | `${JDBX_VAR_DIR}/ssl/server.key` |
| `JDBX_SSL_CA_FILE` | Certificate authority file | `${JDBX_VAR_DIR}/ssl/ca.crt` |

### Logging Configuration

| Variable | Description | Default |
|----------|-------------|---------|
| `JDBX_LOG_LEVEL` | Log level (error, warning, info, debug, trace) | `info` |
| `JDBX_LOG_FILE` | Log file path | `${JDBX_VAR_DIR}/log/jdbx/server.log` |
| `JDBX_LOG_CONSOLE` | Log to console | `true` |
| `JDBX_LOG_MAX_SIZE` | Max log file size (bytes) | `10485760` |
| `JDBX_LOG_MAX_FILES` | Max rotated log files | `5` |
| `JDBX_VERBOSE` | Enable verbose logging | `false` |

### Feature Configuration

| Variable | Description | Default |
|----------|-------------|---------|
| `JDBX_JS_ENABLED` | Enable JavaScript support | `true` |
| `JDBX_WEB_ROOT` | Web admin interface root | `${JDBX_SHARE_DIR}/htdocs` |
| `JDBX_PID_FILE` | PID file path | `${JDBX_VAR_DIR}/run/jdbx/jdbxd.pid` |

### Directory Configuration

| Variable | Description | Default |
|----------|-------------|---------|
| `JDBX_VALIDATORS_DIR` | Validators directory | `${JDBX_VAR_DIR}/validators` |
| `JDBX_TRANSFORMS_DIR` | Transforms directory | `${JDBX_VAR_DIR}/transforms` |
| `JDBX_METRICS_DIR` | Metrics directory | `${JDBX_VAR_DIR}/metrics` |
| `JDBX_BACKUP_DIR` | Backup directory | `${JDBX_VAR_DIR}/backups` |

## Configuration Options Reference

### Server Settings

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `server.port` | integer | `5000` | Server listening port |
| `server.host` | string | `"0.0.0.0"` | Server binding address (0.0.0.0 = all interfaces) |
| `server.max_connections` | integer | `100` | Maximum concurrent client connections |
| `server.timeout` | integer | `30` | Connection timeout in seconds |

### Database Settings

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `database.path` | string | `"var/data/jdbx/db.json"` | Database file path (relative to binary) |
| `database.auto_save` | boolean | `true` | Enable automatic saving of changes |
| `database.save_interval` | integer | `60` | Auto-save interval in seconds |

### RBAC Settings

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `rbac.path` | string | `"var/data/jdbx/rbac.json"` | RBAC configuration file path |
| `rbac.auto_save` | boolean | `true` | Enable automatic saving of RBAC changes |
| `rbac.save_interval` | integer | `60` | RBAC auto-save interval in seconds |

### JWT Settings

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `jwt.secret` | string | See security note | JWT signing secret key |
| `jwt.expiration` | integer | `86400` | Token expiration time in seconds (1 day) |
| `jwt.algorithm` | string | `"HS256"` | JWT signing algorithm |
| `jwt.issuer` | string | `"jdbx"` | JWT issuer identifier |

### SSL Settings

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `ssl.enabled` | boolean | `false` | Enable SSL/TLS encryption |
| `ssl.cert_path` | string | `"certs/certificate.crt"` | SSL certificate file path |
| `ssl.key_path` | string | `"certs/private.key"` | SSL private key file path |
| `ssl.ca_path` | string | `"certs/ca.crt"` | Certificate authority file path (optional) |

### Logging Settings

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `logging.level` | string | `"info"` | Log level: error, warning, info, debug, trace |
| `logging.file` | string | `"var/log/jdbx/server.log"` | Log file path |
| `logging.console` | boolean | `true` | Also log to console/stdout |
| `logging.max_size` | integer | `10485760` | Max log file size before rotation (10MB) |
| `logging.max_files` | integer | `5` | Number of rotated log files to keep |

### CORS Settings

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `cors.enabled` | boolean | `true` | Enable CORS support |
| `cors.allowed_origins` | array | `["*"]` | Allowed origin domains |
| `cors.allowed_methods` | array | `["GET", "POST", "PUT", "DELETE", "OPTIONS"]` | Allowed HTTP methods |
| `cors.allowed_headers` | array | `["Content-Type", "Authorization"]` | Allowed request headers |
| `cors.expose_headers` | array | `["Content-Length"]` | Headers exposed to browser |
| `cors.allow_credentials` | boolean | `false` | Allow cookies in CORS requests |
| `cors.max_age` | integer | `86400` | CORS cache duration in seconds |

### Rate Limiting Settings

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `throttling.enabled` | boolean | `false` | Enable rate limiting |
| `throttling.rate` | integer | `100` | Max requests per minute |
| `throttling.burst` | integer | `50` | Max burst size |
| `throttling.ip_whitelist` | array | `["127.0.0.1"]` | IPs exempt from rate limiting |

### Admin Settings

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `admin.username` | string | `"admin"` | Default admin username |
| `admin.password` | string | See security note | Default admin password |

## Examples

### Complete JSON Configuration Example

```json
{
  "server": {
    "port": 5432,
    "host": "localhost",
    "max_connections": 200,
    "timeout": 60
  },
  "database": {
    "path": "/data/jdbx/production.jdb",
    "auto_save": true,
    "save_interval": 30
  },
  "rbac": {
    "path": "/data/jdbx/rbac.json",
    "auto_save": true,
    "save_interval": 30
  },
  "jwt": {
    "secret": "your-very-secure-secret-key-here",
    "expiration": 3600,
    "algorithm": "HS256",
    "issuer": "my-jdbx-server"
  },
  "ssl": {
    "enabled": true,
    "cert_path": "/etc/ssl/certs/jdbx.crt",
    "key_path": "/etc/ssl/private/jdbx.key"
  },
  "logging": {
    "level": "info",
    "file": "/var/log/jdbx/server.log",
    "console": false,
    "max_size": 52428800,
    "max_files": 10
  },
  "cors": {
    "enabled": true,
    "allowed_origins": ["https://app.example.com", "https://admin.example.com"],
    "allowed_methods": ["GET", "POST", "PUT", "DELETE"],
    "allowed_headers": ["Content-Type", "Authorization", "X-Request-ID"],
    "expose_headers": ["Content-Length", "X-Request-ID"],
    "allow_credentials": true,
    "max_age": 3600
  },
  "throttling": {
    "enabled": true,
    "rate": 1000,
    "burst": 100,
    "ip_whitelist": ["127.0.0.1", "10.0.0.0/8"]
  },
  "admin": {
    "username": "admin",
    "password": "change-this-secure-password"
  }
}
```

### Environment File Example

Create a file `jdbx.env`:

```bash
#!/bin/bash
# JDBX Production Environment Configuration

# Base directories
export JDBX_BASE_DIR="/opt/jdbx"
export JDBX_VAR_DIR="/var/lib/jdbx"
export JDBX_LOG_DIR="/var/log/jdbx"

# Server configuration
export JDBX_PORT="5432"
export JDBX_HOST="0.0.0.0"
export JDBX_MAX_CONNECTIONS="200"
export JDBX_THREAD_POOL_MIN="8"
export JDBX_THREAD_POOL_MAX="32"

# Database paths
export JDBX_DB_PATH="/data/jdbx/production.jdb"
export JDBX_RBAC_FILE="/data/jdbx/rbac.json"

# Security
export JDBX_JWT_SECRET="your-very-secure-secret-key-here"
export JDBX_JWT_EXPIRATION="3600"
export JDBX_USE_SSL="true"
export JDBX_SSL_CERT_FILE="/etc/ssl/certs/jdbx.crt"
export JDBX_SSL_KEY_FILE="/etc/ssl/private/jdbx.key"

# Logging
export JDBX_LOG_LEVEL="info"
export JDBX_LOG_FILE="${JDBX_LOG_DIR}/server.log"
export JDBX_LOG_CONSOLE="false"

# Features
export JDBX_JS_ENABLED="true"
export JDBX_VERBOSE="false"
```

Load and use:

```bash
# Using runtime script
build/jdbx_runtime.sh start --env-file=jdbx.env

# Or source directly
source jdbx.env
./bin/jdbxd
```

### Command-Line Override Example

```bash
# Override specific settings via command line
./bin/jdbxd \
  -config production.json \
  -port 5678 \
  -host 127.0.0.1 \
  -verbose

# Or with environment variables
JDBX_PORT=5678 JDBX_LOG_LEVEL=debug ./bin/jdbxd
```

## Security Considerations

### Production Deployment Checklist

1. **JWT Secret Key**
   - **ALWAYS** change the default JWT secret
   - Use a strong, randomly generated key (minimum 32 characters)
   - Store securely and never commit to version control

2. **Admin Credentials**
   - **ALWAYS** change the default admin password immediately
   - Use strong passwords following security best practices
   - Consider implementing password policies

3. **SSL/TLS**
   - Enable SSL in production environments
   - Use valid certificates from a trusted CA
   - Keep certificates up to date

4. **Network Security**
   - Bind to specific interfaces instead of 0.0.0.0 when possible
   - Use firewall rules to restrict access
   - Consider using a reverse proxy for additional security

5. **Rate Limiting**
   - Enable rate limiting to prevent abuse
   - Adjust rates based on expected usage patterns
   - Monitor for unusual activity

6. **CORS Configuration**
   - Restrict allowed origins to specific domains
   - Avoid using "*" for allowed origins in production
   - Only allow necessary HTTP methods

7. **File Permissions**
   - Ensure configuration files are readable only by the server user
   - Protect JWT secrets and SSL keys with restrictive permissions
   - Regularly audit file permissions

### Path Resolution and Security

- All relative paths are resolved relative to the binary directory
- This prevents directory traversal attacks
- Absolute paths are used as-is but should be validated
- Ensure all paths point to intended locations

### Environment Variable Security

- Avoid storing secrets in environment files in production
- Use secure secret management systems when available
- Restrict access to environment configuration files
- Never log or display sensitive configuration values