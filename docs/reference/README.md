# JDBX Reference Documentation

> Comprehensive technical reference for all JDBX features and configurations

## Reference Categories

### Configuration
- **[Configuration Reference](configuration.md)** - All configuration options
- **[Environment Variables](environment-variables.md)** - Environment-based configuration
- **[Runtime Options](runtime-options.md)** - Command-line flags and options

### Query Language
- **[Query Language Reference](query-language.md)** - Complete query syntax
- **[Query Operators](query-operators.md)** - All supported operators
- **[Query Examples](query-examples.md)** - Common query patterns

### Data Formats
- **[JSON Document Format](json-format.md)** - Document structure and types
- **[Binary Format Specification](binary-format-spec.md)** - Internal storage format
- **[Import/Export Formats](import-export-formats.md)** - Data interchange formats

### System Internals
- **[Error Codes](error-codes.md)** - Complete error code reference
- **[Metrics Reference](metrics.md)** - Available metrics and meanings
- **[Log Format](log-format.md)** - Log message structure

### Performance
- **[Performance Benchmarks](performance-benchmarks.md)** - Test results and methodology
- **[Performance Tuning](performance-tuning.md)** - Optimization parameters
- **[Resource Limits](resource-limits.md)** - System limitations

### Security
- **[Permission Model](permissions.md)** - RBAC permission system
- **[Authentication Methods](authentication-methods.md)** - Supported auth mechanisms
- **[Security Best Practices](security-practices.md)** - Hardening guidelines

### Troubleshooting
- **[Troubleshooting Guide](troubleshooting.md)** - Common issues and solutions
- **[FAQ](faq.md)** - Frequently asked questions
- **[Known Issues](known-issues.md)** - Current limitations

## Quick Reference Tables

### Default Configuration

| Setting | Default | Description |
|---------|---------|-------------|
| Port | 5000 | Server listen port |
| Host | 0.0.0.0 | Bind address |
| Database Path | var/data/jdbx/db.jdb | Database file path |
| Log Level | INFO | Logging verbosity |
| Cache Size | 10MB | Document cache size |
| SSL Enabled | true | SSL/TLS encryption enabled |
| Thread Pool Min | 4 | Minimum thread pool size |
| Thread Pool Max | 16 | Maximum thread pool size |

### Common Query Operators

| Operator | Description | Example |
|----------|-------------|------|
| `$eq` | Equals | `{"age": {"$eq": 25}}` |
| `$gt` | Greater than | `{"age": {"$gt": 18}}` |
| `$in` | In array | `{"status": {"$in": ["active", "pending"]}}` |
| `$regex` | Pattern match | `{"name": {"$regex": "^John"}}` |
| `$exists` | Field exists | `{"email": {"$exists": true}}` |

### HTTP Status Codes

| Code | Meaning | Common Cause |
|------|---------|-------------|
| 200 | Success | Request completed |
| 201 | Created | Document created |
| 400 | Bad Request | Invalid syntax |
| 401 | Unauthorized | Missing/invalid auth |
| 404 | Not Found | Resource doesn't exist |
| 409 | Conflict | Duplicate key |
| 500 | Server Error | Internal error |

### Performance Metrics

| Metric | Unit | Target |
|--------|------|--------|
| Insert Latency | microseconds | < 1000 |
| Query Latency | microseconds | < 1000 |
| Throughput | ops/sec | > 10000 |
| Cache Hit Rate | percentage | > 80% |

## Configuration Files

### System Configuration
Location: `_system_config` collection (highest priority in 3-tier config system)

```json
{
  "server": {
    "port": 5000,
    "host": "0.0.0.0",
    "ssl": true,
    "ssl_cert": "/etc/ssl/certs/server.pem",
    "ssl_key": "/etc/ssl/private/server.key"
  },
  "database": {
    "path": "var/data/jdbx/db.jdb",
    "mmap_size": "10GB"
  },
  "cache": {
    "enabled": true,
    "size": "10MB"
  },
  "thread_pool": {
    "min_threads": 4,
    "max_threads": 16,
    "queue_size": 1024,
    "idle_timeout": 60
  }
}
```

### Environment Configuration
File: `/opt/jdbx/share/config/jdbx.env`

```bash
# Server settings
JDBX_PORT=5000
JDBX_HOST=0.0.0.0

# Database settings
JDBX_DATABASE_PATH=/opt/jdbx/build/var
JDBX_LOG_LEVEL=INFO

# Performance
JDBX_CACHE_SIZE=1073741824  # 1GB in bytes
JDBX_THREAD_POOL_SIZE=16
```

## API Endpoints Quick Reference

See [REST API Reference](../api/rest-api.md) for complete documentation.

### Authentication
- `POST /api/auth/login` - Get JWT token
- `POST /api/auth/refresh` - Refresh token
- `POST /api/auth/logout` - Invalidate token

### Collections
- `GET /api/collections` - List collections
- `POST /api/collections` - Create collection
- `DELETE /api/collections/{name}` - Drop collection

### Documents
- `GET /api/collections/{name}/documents` - Query documents
- `POST /api/collections/{name}/documents` - Insert document
- `GET /api/collections/{name}/documents/{id}` - Get document
- `PUT /api/collections/{name}/documents/{id}` - Update document
- `DELETE /api/collections/{name}/documents/{id}` - Delete document

### System
- `GET /api/health` - Health check
- `GET /api/metrics` - System metrics
- `GET /api/info` - Server information

## Command Line Reference

### Server Commands
```bash
# Start server
./build/jdbx_runtime.sh start

# Stop server
./build/jdbx_runtime.sh stop

# Check status
./build/jdbx_runtime.sh status

# View logs
./build/jdbx_runtime.sh logs
```

### Direct Binary Options
```bash
# Custom port
jdbxd --port 8080

# Disable SSL
jdbxd --no-ssl

# Custom SSL certificates
jdbxd --ssl-cert /path/to/cert.pem --ssl-key /path/to/key.pem

# Debug mode
jdbxd --log-level DEBUG

# Thread pool configuration
jdbxd --thread-pool-min 8 --thread-pool-max 32
```

## File Formats

### Document ID Format
```
doc-{timestamp}-{nanoseconds}-{random}
Example: doc-1699564234-123456789-a1b2
```

### Log Entry Format
```
[TIMESTAMP] [LEVEL] [FILE:LINE:FUNCTION] Message
[2025-06-05 10:30:45.123] [INFO] [server.c:123:start_server] Server started on port 5000
```

## See Also

- [Getting Started Guide](../getting-started/README.md)
- [API Documentation](../api/README.md)
- [Architecture Overview](../architecture/README.md)
- [Development Guide](../development/README.md)