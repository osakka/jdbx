# JDBX Configuration Reference

**Version**: 6.3.7  
**Last Updated**: June 19, 2025

Complete configuration reference for JDBX with three-tier configuration system and enterprise security.

## Configuration Hierarchy

JDBX implements a comprehensive three-tier configuration system with the following priority (highest to lowest):

1. **Database Configuration** - Runtime changes via `_system_config` collection
2. **Command-Line Flags** - Binary arguments override environment settings
3. **Environment Variables** - Container/deployment configuration
4. **Configuration File** - JSON/ENV file configuration
5. **Secure Defaults** - Cryptographically generated or production-safe defaults

## Environment Variables

### Critical Security Configuration

```bash
# REQUIRED for production - no secure defaults
export JDBX_BOOTSTRAP_ADMIN_USER="admin"              # Min 3 characters
export JDBX_BOOTSTRAP_ADMIN_PASS="secure_password123" # Min 12 characters  
export JDBX_DEFAULT_ADMIN_EMAIL="admin@company.com"   # Valid email required

# Auto-generated if not provided
export JDBX_JWT_SECRET="<64-char-secure-random>"      # Generated via /dev/urandom
```

### Server Configuration

```bash
# Network Configuration
export JDBX_PORT="5000"                    # Server port (default: 5000)
export JDBX_HOST="0.0.0.0"                 # Bind address (default: 0.0.0.0)
export JDBX_BACKLOG="128"                  # Connection backlog (default: 128)

# SSL/TLS Configuration  
export JDBX_USE_SSL="true"                 # Enable SSL (default: true)
export JDBX_SSL_CERT="/path/to/cert.pem"   # SSL certificate path
export JDBX_SSL_KEY="/path/to/key.pem"     # SSL private key path
```

### Database Configuration

```bash
# Storage Configuration
export JDBX_DATA_DIR="/opt/jdbx/var/data"  # Data directory
export JDBX_DB_EXTENSION=".jdbx"           # Database file extension
export JDBX_WAL_EXTENSION=".wal"           # WAL file extension
export JDBX_MMAP_SIZE="1073741824"         # Memory map size (1GB default)

# Storage Backend
export JDBX_STORAGE_BACKEND="jdbx"         # Storage engine: "jdbx" or "mmap"
```

### Thread Pool Configuration

```bash
# Thread Pool Settings
export JDBX_THREAD_POOL_MIN="4"            # Minimum threads
export JDBX_THREAD_POOL_MAX="16"           # Maximum threads  
export JDBX_THREAD_POOL_QUEUE_SIZE="1000"  # Task queue size
export JDBX_THREAD_POOL_IDLE_TIMEOUT="60"  # Idle thread timeout (seconds)
```

### Memory Management

```bash
# Buffer Pool Configuration
export JDBX_BUFFER_POOL_SIZE="268435456"   # Buffer pool size (256MB default)
export JDBX_BUFFER_POOL_DEBUG="0"          # Debug mode (0/1)

# Memory Manager (v6.3.0)
export JDBX_MEMORY_CHECKPOINTS="1"         # Enable checkpoint-based allocation
```

### Logging Configuration

```bash
# Logging Settings
export JDBX_LOG_LEVEL="info"               # Log level: error/warning/info/debug/trace
export JDBX_LOG_FILE="/opt/jdbx/var/jdbxd.log"  # Log file path
export JDBX_LOG_TO_CONSOLE="0"             # Also log to console (0/1)
```

## Command-Line Flags

### Essential Operations

```bash
# Help and version
jdbxd -h, --help                           # Show help
jdbxd -v, --version                        # Show version

# Execution modes
jdbxd -d, --daemon                         # Run as daemon
jdbxd -f, --foreground                     # Run in foreground
jdbxd -c, --config <file>                  # Configuration file
```

### Security Configuration

```bash
# Admin credentials
jdbxd --bootstrap-admin-user <username>    # Bootstrap admin username
jdbxd --bootstrap-admin-pass <password>    # Bootstrap admin password  
jdbxd --jwt-secret <secret>                # JWT signing secret

# SSL/TLS
jdbxd --ssl                                # Enable SSL
jdbxd --no-ssl                             # Disable SSL
jdbxd --ssl-cert <path>                    # SSL certificate
jdbxd --ssl-key <path>                     # SSL private key
```

### Server Configuration

```bash
# Network settings
jdbxd --port <port>                        # Server port
jdbxd --host <address>                     # Bind address
jdbxd --backlog <size>                     # Connection backlog

# Database paths
jdbxd --data-dir <path>                    # Data directory
jdbxd --db-extension <ext>                 # Database extension
jdbxd --wal-extension <ext>                # WAL extension
```

### Performance Tuning

```bash
# Thread pool
jdbxd --thread-min <n>                     # Minimum threads
jdbxd --thread-max <n>                     # Maximum threads
jdbxd --thread-queue <size>                # Queue size
jdbxd --thread-idle <seconds>              # Idle timeout

# Memory settings
jdbxd --buffer-size <bytes>                # Buffer pool size
jdbxd --mmap-size <bytes>                  # Memory map size
```

## Configuration File

Create `jdbx.conf` or `jdbx.json`:

```json
{
  "server": {
    "port": 5000,
    "host": "0.0.0.0",
    "ssl": {
      "enabled": true,
      "cert": "/etc/ssl/certs/server.pem",
      "key": "/etc/ssl/private/server.key"
    }
  },
  "database": {
    "data_dir": "/opt/jdbx/var/data",
    "storage_backend": "jdbx",
    "extensions": {
      "db": ".jdbx",
      "wal": ".wal"
    }
  },
  "thread_pool": {
    "min": 4,
    "max": 16,
    "queue_size": 1000,
    "idle_timeout": 60
  },
  "security": {
    "jwt_secret": "NEVER_HARDCODE_IN_PRODUCTION",
    "bootstrap": {
      "admin_user": "admin",
      "admin_pass": "secure_password_123",
      "admin_email": "admin@company.com"
    }
  }
}
```

## Runtime Configuration API

Update configuration at runtime via the database:

```bash
# Update configuration
curl -X PUT https://localhost:5000/api/config \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "thread_pool_max": 32,
    "log_level": "debug"
  }'

# Get current configuration
curl -X GET https://localhost:5000/api/config \
  -H "Authorization: Bearer $TOKEN"
```

## Production Deployment

### Required Environment Variables

```bash
#!/bin/bash
# Production environment file

# CRITICAL SECURITY - Required
export JDBX_BOOTSTRAP_ADMIN_USER="prodadmin"
export JDBX_BOOTSTRAP_ADMIN_PASS="VerySecurePassword123!@#"
export JDBX_DEFAULT_ADMIN_EMAIL="admin@yourcompany.com"

# Production settings
export JDBX_USE_SSL="true"
export JDBX_SSL_CERT="/etc/letsencrypt/live/yourserver/fullchain.pem"
export JDBX_SSL_KEY="/etc/letsencrypt/live/yourserver/privkey.pem"
export JDBX_LOG_LEVEL="warning"
export JDBX_THREAD_POOL_MIN="8"
export JDBX_THREAD_POOL_MAX="64"
```

### Docker Configuration

```dockerfile
# Dockerfile ENV directives
ENV JDBX_BOOTSTRAP_ADMIN_USER=admin
ENV JDBX_BOOTSTRAP_ADMIN_PASS=${ADMIN_PASSWORD}
ENV JDBX_DEFAULT_ADMIN_EMAIL=${ADMIN_EMAIL}
ENV JDBX_USE_SSL=true
ENV JDBX_PORT=5443
```

### Kubernetes ConfigMap

```yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: jdbx-config
data:
  JDBX_HOST: "0.0.0.0"
  JDBX_PORT: "5000"
  JDBX_USE_SSL: "true"
  JDBX_LOG_LEVEL: "info"
  JDBX_THREAD_POOL_MIN: "4"
  JDBX_THREAD_POOL_MAX: "32"
```

## Security Best Practices

1. **Never hardcode credentials** - Always use environment variables
2. **Use strong JWT secrets** - Minimum 64 characters, cryptographically random
3. **Enable SSL in production** - Use proper certificates, not self-signed
4. **Secure admin passwords** - Minimum 12 characters with complexity
5. **Rotate secrets regularly** - Update JWT secrets periodically
6. **Audit configuration** - Review settings before production deployment

## Configuration Validation

JDBX validates configuration on startup:

- **Password strength** - Minimum length and complexity checks
- **Path existence** - Verifies directories exist with proper permissions
- **Certificate validity** - Checks SSL certificates are valid
- **Resource limits** - Ensures settings are within system capabilities
- **Security warnings** - Alerts for insecure placeholder values

## Troubleshooting

### Common Issues

1. **"SECURITY CRITICAL: Using placeholder admin password!"**
   - Set `JDBX_BOOTSTRAP_ADMIN_USER` and `JDBX_BOOTSTRAP_ADMIN_PASS`

2. **"Failed to bind socket: Address already in use"**
   - Change `JDBX_PORT` or stop conflicting service

3. **"SSL certificate file not found"**
   - Verify `JDBX_SSL_CERT` and `JDBX_SSL_KEY` paths

4. **"Insufficient memory for buffer pool"**
   - Reduce `JDBX_BUFFER_POOL_SIZE` or increase system memory

### Debug Configuration

```bash
# Enable debug logging
export JDBX_LOG_LEVEL="debug"
export JDBX_LOG_TO_CONSOLE="1"

# Show configuration on startup
jdbxd --show-config

# Validate configuration without starting
jdbxd --validate-config
```

## See Also

- [Production Deployment Guide](../../deployment/production/production-checklist.md)
- [Security Best Practices](../../security/guidelines/security-best-practices.md)
- [Performance Tuning](../../architecture/performance/optimization-guide.md)
- [SSL Configuration](socket-binding.md)