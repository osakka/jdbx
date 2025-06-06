# JSONdb Reference Documentation

> Comprehensive technical reference for all JSONdb features and configurations

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
| Database Path | /opt/jsondb/build/var | Data directory |
| Log Level | INFO | Logging verbosity |
| Cache Size | 50MB | Document cache size |

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
Location: `_system_config` collection

```json
{
  "server": {
    "port": 5000,
    "host": "0.0.0.0",
    "ssl": false
  },
  "database": {
    "path": "/opt/jsondb/build/var",
    "mmap_size": "10GB"
  },
  "cache": {
    "enabled": true,
    "size": "1GB"
  }
}
```

### Environment Configuration
File: `/opt/jsondb/share/config/jsondb.env`

```bash
# Server settings
JSONDB_PORT=5000
JSONDB_HOST=0.0.0.0

# Database settings
JSONDB_DATABASE_PATH=/opt/jsondb/build/var
JSONDB_LOG_LEVEL=INFO

# Performance
JSONDB_CACHE_SIZE=1073741824  # 1GB in bytes
JSONDB_THREAD_POOL_SIZE=16
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
./build/jsondb_runtime.sh start

# Stop server
./build/jsondb_runtime.sh stop

# Check status
./build/jsondb_runtime.sh status

# View logs
./build/jsondb_runtime.sh logs
```

### Direct Binary Options
```bash
# Custom port
jsondb_server --port 8080

# Custom config
jsondb_server --config /path/to/config.json

# Debug mode
jsondb_server --log-level DEBUG
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